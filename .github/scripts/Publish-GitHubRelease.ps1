<#
.SYNOPSIS
Creates or updates an OIViewer GitHub release from prepared archives.

.DESCRIPTION
Validates immutable tags, optionally moves a rolling tag to the requested source commit, creates the release when
needed, uploads all archives, and removes obsolete assets only after replacement uploads succeed. GitHub CLI
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

.PARAMETER Archives
Paths to the release archives to upload.

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
    [string[]]$Archives
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Package selection is shared by the workflow and offline tests. A release is complete only when all
# four products exist, so a missing WLX build cannot silently turn into a standalone-only release.
function Get-ReleasePackageSet {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Directory,
        [Parameter(Mandatory)][bool]$OfficialRelease
    )

    $symbolsPattern = '-Symbols(?:\.[^.]+)+$'
    $wlxPattern = '-Win32-[^-]+-[^-.]+-WLX\.zip$'
    $archives = @(Get-ChildItem -LiteralPath $Directory -Recurse -File | Where-Object { $_.Name -match '^OIV-.+(?:\.[^.]+)+$' })
    $groups = [ordered]@{
        "Windows runtime" = @($archives | Where-Object { $_.Name -notmatch $symbolsPattern -and $_.Name -notmatch $wlxPattern -and $_.Name -match '-Win32-[^-]+-[^-.]+\.' })
        "Linux runtime" = @($archives | Where-Object { $_.Name -notmatch $symbolsPattern -and $_.Name -match '-linux-[^-]+-[^-.]+\.' })
        "Windows WLX" = @($archives | Where-Object { $_.Name -match $wlxPattern })
        "symbols" = @($archives | Where-Object { $_.Name -match $symbolsPattern })
    }
    foreach ($group in $groups.GetEnumerator()) {
        if ($group.Value.Count -ne 1) {
            $found = if ($group.Value.Count -eq 0) { "<none>" } else { $group.Value.FullName -join "`n" }
            throw "Expected exactly one $($group.Key) archive, found $($group.Value.Count):`n$found"
        }
    }

    $titlePattern = if ($OfficialRelease) {
        '^(?<title>OIV-\d+\.\d+\.\d+)-.+(?:\.[^.]+)+$'
    } else {
        '^(?<title>OIV-\d+\.\d+\.\d+\.\d+)-[0-9a-f]+-.+(?:\.[^.]+)+$'
    }
    $title = $null
    foreach ($group in $groups.GetEnumerator()) {
        $archive = $group.Value[0]
        if ($archive.Name -notmatch $titlePattern) {
            throw "Archive name '$($archive.Name)' does not match the selected release mode."
        }
        $archiveTitle = $Matches.title
        if ($title -and $archiveTitle -ne $title) {
            throw "Release archive versions do not match."
        }
        $title = $archiveTitle
    }

    [PSCustomObject]@{
        WindowsRuntimeArchivePath = $groups["Windows runtime"][0].FullName
        LinuxRuntimeArchivePath = $groups["Linux runtime"][0].FullName
        WindowsWlxArchivePath = $groups["Windows WLX"][0].FullName
        SymbolsArchivePath = $groups["symbols"][0].FullName
        ReleaseTitle = $title
        ReleaseTag = if ($OfficialRelease) { $title } else { "snapshot" }
        Prerelease = -not $OfficialRelease
    }
}

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
        [Parameter(Mandatory)][string[]]$Archives
    )

    if ([string]::IsNullOrWhiteSpace($env:GH_TOKEN)) {
        throw "Release token is not configured."
    }
    $archiveNames = @($Archives | ForEach-Object { [IO.Path]::GetFileName($_) })

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
    $uploadArguments = @("release", "upload", $Tag) + $Archives + @("--repo", $Repository, "--clobber")
    Invoke-Gh -Arguments $uploadArguments | Out-Null

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

        # Cleanup is deliberately last: successful replacement assets remain available if later metadata work fails.
        foreach ($asset in @($release.assets | Where-Object { $_.name -notin $archiveNames })) {
            Invoke-Gh -Arguments @(
                "release", "delete-asset", $Tag, $asset.name,
                "--repo", $Repository,
                "--yes"
            ) | Out-Null
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($env:GITHUB_STEP_SUMMARY)) {
        Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "Published release assets to $Repository release '$Tag' for ${TargetSha}: $($archiveNames -join ', ')."
    }
}

# Tests dot-source this file to exercise the functions without running the publication entry point.
if ($MyInvocation.InvocationName -ne ".") {
    Invoke-ReleasePublication @PSBoundParameters
}
