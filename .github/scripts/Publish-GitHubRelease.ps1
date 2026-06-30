<#
.SYNOPSIS
Creates or updates an OIViewer GitHub release from prepared runtime and symbols archives.

.DESCRIPTION
Validates immutable tags, optionally moves a rolling tag to the requested source commit, creates the release when
needed, uploads both archives, and removes obsolete assets only after replacement uploads succeed. GitHub CLI
failures are fatal except for an explicitly allowed HTTP 404 used to represent an absent tag or release.

.PARAMETER Repository
GitHub repository in owner/name form.

.PARAMETER Tag
Git tag and release identifier to create or update.

.PARAMETER TargetSha
Source commit that the release tag must target.

.PARAMETER Title
Display title for the GitHub release.

.PARAMETER Prerelease
Whether the release must be marked as a prerelease.

.PARAMETER MutableTag
Whether an existing tag may move to TargetSha after replacement assets upload successfully.

.PARAMETER RuntimeArchive
Path to the runtime archive to upload.

.PARAMETER SymbolsArchive
Path to the matching symbols archive to upload.

.NOTES
Requires the GitHub CLI and a GH_TOKEN environment variable. Dot-sourcing the file loads its functions without
publishing; executing it normally invokes Invoke-ReleasePublication with the supplied parameters.
#>
[CmdletBinding()]
param(
    [string]$Repository,
    [string]$Tag,
    [string]$TargetSha,
    [string]$Title,
    [bool]$Prerelease,
    [bool]$MutableTag = $false,
    [string]$RuntimeArchive,
    [string]$SymbolsArchive
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-GhNative {
    param([string[]]$Arguments)

    # Capture native exit status and stderr ourselves so callers can distinguish an expected 404 from other failures.
    $nativeErrorPreference = Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue
    $previousNativeErrorPreference = if ($null -ne $nativeErrorPreference) { $nativeErrorPreference.Value } else { $null }
    try {
        if ($null -ne $nativeErrorPreference) {
            Set-Variable -Name PSNativeCommandUseErrorActionPreference -Value $false
        }
        $output = @(& gh @Arguments 2>&1)
        $exitCode = $LASTEXITCODE
    } finally {
        if ($null -ne $nativeErrorPreference) {
            Set-Variable -Name PSNativeCommandUseErrorActionPreference -Value $previousNativeErrorPreference
        }
    }

    [PSCustomObject]@{
        ExitCode = $exitCode
        Output   = $output -join [Environment]::NewLine
    }
}

function Invoke-Gh {
    param(
        [string[]]$Arguments,
        [switch]$AllowNotFound
    )

    $result = Invoke-GhNative -Arguments $Arguments
    if ($result.ExitCode -eq 0) {
        return $result.Output
    }
    # Absence is valid only for lookups that opt in; authentication, rate-limit, and server failures remain fatal.
    if ($AllowNotFound -and $result.Output -match '(?i)\bHTTP\s+404\b') {
        return $null
    }

    throw "GitHub CLI command 'gh $($Arguments -join ' ')' failed: $($result.Output)"
}

function Get-TagCommitSha {
    param(
        [string]$Repository,
        [string]$Tag
    )

    $referenceJson = Invoke-Gh -Arguments @("api", "repos/$Repository/git/ref/tags/$Tag") -AllowNotFound
    if ($null -eq $referenceJson) {
        return $null
    }

    $target = ($referenceJson | ConvertFrom-Json).object
    # Annotated tags point to tag objects, so peel each layer until the underlying commit is reached.
    while ($target.type -eq "tag") {
        $tagJson = Invoke-Gh -Arguments @("api", "repos/$Repository/git/tags/$($target.sha)")
        $target = ($tagJson | ConvertFrom-Json).object
    }

    if ($target.type -ne "commit") {
        throw "Release tag '$Tag' resolves to unsupported object type '$($target.type)'."
    }

    $target.sha
}

function Get-GitHubRelease {
    param(
        [string]$Repository,
        [string]$Tag
    )

    $releaseJson = Invoke-Gh -Arguments @("api", "repos/$Repository/releases/tags/$Tag") -AllowNotFound
    if ($null -eq $releaseJson) {
        return $null
    }

    $release = $releaseJson | ConvertFrom-Json
    [PSCustomObject]@{
        isPrerelease = [bool]$release.prerelease
        assets       = @($release.assets)
    }
}

function Set-TagCommitSha {
    param(
        [string]$Repository,
        [string]$Tag,
        [string]$TargetSha
    )

    Invoke-Gh -Arguments @(
        "api", "--method", "PATCH",
        "repos/$Repository/git/refs/tags/$Tag",
        "-f", "sha=$TargetSha",
        "-F", "force=true"
    ) | Out-Null
}

function Invoke-ReleasePublication {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Repository,
        [Parameter(Mandatory)][string]$Tag,
        [Parameter(Mandatory)][string]$TargetSha,
        [Parameter(Mandatory)][string]$Title,
        [Parameter(Mandatory)][bool]$Prerelease,
        [bool]$MutableTag = $false,
        [Parameter(Mandatory)][string]$RuntimeArchive,
        [Parameter(Mandatory)][string]$SymbolsArchive
    )

    if ([string]::IsNullOrWhiteSpace($env:GH_TOKEN)) {
        throw "Release token is not configured."
    }

    $tagCommitSha = Get-TagCommitSha -Repository $Repository -Tag $Tag
    if (-not $MutableTag -and $null -ne $tagCommitSha -and $tagCommitSha -ne $TargetSha) {
        # Official release tags remain immutable.
        throw "Release tag '$Tag' already targets '$tagCommitSha', not '$TargetSha'."
    }

    $release = Get-GitHubRelease -Repository $Repository -Tag $Tag
    $notes = "Source commit: $TargetSha"
    if ($null -eq $release) {
        if ($MutableTag -and $null -ne $tagCommitSha -and $tagCommitSha -ne $TargetSha) {
            Set-TagCommitSha -Repository $Repository -Tag $Tag -TargetSha $TargetSha
        }
        $createArguments = @(
            "release", "create", $Tag,
            "--repo", $Repository,
            "--target", $TargetSha,
            "--title", $Title,
            "--notes", $notes
        )
        if ($Prerelease) {
            $createArguments += "--prerelease"
        }
        Invoke-Gh -Arguments $createArguments | Out-Null
    } elseif (-not $MutableTag -and [bool]$release.isPrerelease -ne $Prerelease) {
        throw "Release '$Tag' has the wrong prerelease state for the selected mode."
    }

    # Upload first so a failed transfer cannot remove every asset from an existing public release.
    Invoke-Gh -Arguments @(
        "release", "upload", $Tag,
        $RuntimeArchive, $SymbolsArchive,
        "--repo", $Repository,
        "--clobber"
    ) | Out-Null

    if ($null -ne $release) {
        if ($MutableTag -and $tagCommitSha -ne $TargetSha) {
            Set-TagCommitSha -Repository $Repository -Tag $Tag -TargetSha $TargetSha
        }

        $editArguments = @(
            "release", "edit", $Tag,
            "--repo", $Repository,
            "--title", $Title,
            "--notes", $notes
        )
        if ($Prerelease) {
            $editArguments += "--prerelease"
        }
        Invoke-Gh -Arguments $editArguments | Out-Null

        $expectedAssetNames = @(
            [IO.Path]::GetFileName($RuntimeArchive),
            [IO.Path]::GetFileName($SymbolsArchive)
        )
        # Cleanup is deliberately last: successful replacement assets remain available if later metadata work fails.
        foreach ($asset in @($release.assets | Where-Object { $_.name -notin $expectedAssetNames })) {
            Invoke-Gh -Arguments @(
                "release", "delete-asset", $Tag, $asset.name,
                "--repo", $Repository,
                "--yes"
            ) | Out-Null
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($env:GITHUB_STEP_SUMMARY)) {
        $runtimeArchiveName = [IO.Path]::GetFileName($RuntimeArchive)
        $symbolsArchiveName = [IO.Path]::GetFileName($SymbolsArchive)
        Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "Published release assets to $Repository release '$Tag' for ${TargetSha}: $runtimeArchiveName, $symbolsArchiveName."
    }
}

# Tests dot-source this file to exercise the functions without running the publication entry point.
if ($MyInvocation.InvocationName -ne ".") {
    Invoke-ReleasePublication @PSBoundParameters
}
