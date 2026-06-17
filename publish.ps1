#===============================================================
# PowerShell Build & Packaging Script for OIV
#===============================================================

[CmdletBinding()]
param (
    [string]$BuildType = "RelWithDebInfo",
    [string]$BuildDir = "publish",
    [bool]$EnableConfigure = $true,
    [bool]$EnableBuild = $true,
    [bool]$EnablePackage = $true,
    [bool]$OfficialBuild = $false,
    [bool]$OfficialRelease = $false,
    [bool]$CleanConfigureOnMismatch = $true,
    [switch]$CleanConfigure,
    [switch]$EchoCommands,
    [string]$CMakePath,
    [string]$NinjaPath,
    [string]$SevenZipPath,
    [string]$GitPath,
    [string]$ClangPath,
    [string]$RcPath,
    [string]$MtPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

#---------------------------------------------------------------
# Common Helpers
#---------------------------------------------------------------

function Join-PathForward {
    [CmdletBinding(DefaultParameterSetName = 'PathChild')]
    param (
        [Parameter(Position = 0, Mandatory)][string]$Path,
        [Parameter(Position = 1, ParameterSetName = 'PathChild')][string]$ChildPath,
        [Parameter(Position = 1, ParameterSetName = 'AdditionalChildren')][string[]]$AdditionalChildPaths
    )
    process {
        $result = switch ($PSCmdlet.ParameterSetName) {
            'PathChild'          { Join-Path -Path $Path -ChildPath $ChildPath }
            'AdditionalChildren' { Join-Path -Path $Path -AdditionalChildPaths $AdditionalChildPaths }
        }
        $result -replace '\\', '/'
    }
}

function Raise-Error {
    param([string]$Message)

    throw [System.InvalidOperationException]::new($Message)
}

function Require-Value {
    param(
        [AllowNull()][object]$Value,
        [string]$Message
    )

    if ($null -eq $Value) {
        Raise-Error $Message
    }

    if ($Value -is [string] -and [string]::IsNullOrWhiteSpace($Value)) {
        Raise-Error $Message
    }

    if ($Value -is [array] -and $Value.Count -eq 0) {
        Raise-Error $Message
    }

    $Value
}

function Ensure-Directory {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Convert-ToForwardSlash {
    param([AllowNull()][string]$Path)

    if (-not $Path) { return $Path }
    $Path -replace '\\', '/'
}

function Resolve-BuildPath {
    param([string]$RootDir, [string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return Convert-ToForwardSlash $Path
    }

    Join-PathForward $RootDir $Path
}

function Quote-CommandArgument {
    param([AllowNull()][string]$Argument)

    if ($null -eq $Argument) { return '""' }
    if ($Argument -eq "") { return '""' }
    if ($Argument -match '[\s"]') {
        return '"' + ($Argument -replace '"', '\"') + '"'
    }

    $Argument
}

function Format-CommandLine {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    (@((Quote-CommandArgument $FilePath)) + @($Arguments | ForEach-Object { Quote-CommandArgument $_ })) -join " "
}

function Invoke-NativeCommand {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$ErrorMessage,
        [bool]$EchoCommands,
        [switch]$PassThru
    )

    $FilePath = Require-Value $FilePath "Cannot run native command because the command path is empty. Context: $ErrorMessage"
    $commandLine = Format-CommandLine $FilePath $Arguments

    if ($EchoCommands) {
        Write-Host ("+ {0}" -f $commandLine)
    }

    try {
        if ($PassThru) {
            $output = & $FilePath @Arguments
            if ($LASTEXITCODE -ne 0) {
                Raise-Error "$ErrorMessage`nCommand: $commandLine`nExit code: $LASTEXITCODE"
            }

            return $output
        }

        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            Raise-Error "$ErrorMessage`nCommand: $commandLine`nExit code: $LASTEXITCODE"
        }
    } catch {
        if ($_.Exception -is [System.InvalidOperationException]) {
            throw
        }

        throw [System.InvalidOperationException]::new("$ErrorMessage`nCommand: $commandLine", $_.Exception)
    }
}

function Test-IsWindowsPlatform {
    $isWindowsVariable = Get-Variable -Name IsWindows -Scope Global -ErrorAction SilentlyContinue
    if ($isWindowsVariable) {
        return [bool]$isWindowsVariable.Value
    }

    $env:OS -eq "Windows_NT"
}

#---------------------------------------------------------------
# Tool Discovery
#---------------------------------------------------------------

function Get-ExistingToolPath {
    param([string[]]$Paths)

    foreach ($path in $Paths) {
        if ($path -and (Test-Path -LiteralPath $path -PathType Leaf)) {
            return Convert-ToForwardSlash (Resolve-Path -LiteralPath $path).Path
        }
    }

    return $null
}

function Find-CommandPath {
    param(
        [string[]]$CommandNames,
        [string]$PathValue = $env:Path
    )

    if (-not $PathValue) {
        return $null
    }

    $previousPath = $env:Path
    try {
        $env:Path = $PathValue
        foreach ($commandName in $CommandNames) {
            $command = Get-Command $commandName -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($command) {
                return Convert-ToForwardSlash $command.Source
            }
        }
    } finally {
        $env:Path = $previousPath
    }

    return $null
}

function Resolve-ToolPath {
    param(
        [string]$Name,
        [string[]]$CommandNames,
        [string[]]$FallbackPaths,
        [string]$PreferredPath,
        [string]$OverridePath,
        [bool]$Required
    )

    if ($OverridePath) {
        if (Test-Path -LiteralPath $OverridePath -PathType Leaf) {
            return Convert-ToForwardSlash (Resolve-Path -LiteralPath $OverridePath).Path
        }

        Raise-Error "Configured path for $Name does not exist: $OverridePath"
    }

    if ($PreferredPath) {
        $preferredCommandPath = Find-CommandPath $CommandNames $PreferredPath
        if ($preferredCommandPath) { return $preferredCommandPath }
    }

    $commandPath = Find-CommandPath $CommandNames
    if ($commandPath) { return $commandPath }

    $fallbackPath = Get-ExistingToolPath $FallbackPaths
    if ($fallbackPath) { return $fallbackPath }

    if ($Required) {
        $fallbackSummary = ($FallbackPaths | Where-Object { $_ }) -join ", "
        Raise-Error "Required tool not found: $Name. Searched PATH for $($CommandNames -join ', ') and fallback paths: $fallbackSummary"
    }

    return $null
}

function Resolve-ToolPaths {
    param(
        [object[]]$ToolSpecs,
        [string]$PreferredPath
    )

    $resolvedTools = @{}
    foreach ($toolSpec in $ToolSpecs) {
        $resolveToolArgs = @{
            Name          = $toolSpec.Name
            CommandNames  = $toolSpec.Commands
            FallbackPaths = $toolSpec.Fallbacks
            PreferredPath = $PreferredPath
            OverridePath  = $toolSpec.OverridePath
            Required      = $false
        }
        $resolvedTools[$toolSpec.Key] = Resolve-ToolPath @resolveToolArgs
    }

    $resolvedTools
}

function Get-MissingRequiredToolSpecs {
    param(
        [object[]]$ToolSpecs,
        [hashtable]$ResolvedTools
    )

    @($ToolSpecs | Where-Object { $_.Required -and -not $ResolvedTools[$_.Key] })
}

function Assert-RequiredToolsResolved {
    param(
        [object[]]$ToolSpecs,
        [hashtable]$ResolvedTools,
        [string]$Context
    )

    $missingTools = @(Get-MissingRequiredToolSpecs $ToolSpecs $ResolvedTools)
    if ($missingTools.Count -eq 0) {
        return
    }

    $message = @("Required tools were not found $Context.")
    foreach ($toolSpec in $missingTools) {
        $fallbackSummary = if ($toolSpec.Fallbacks.Count -gt 0) { $toolSpec.Fallbacks -join ", " } else { "<none>" }
        $overrideSummary = if ($toolSpec.OverridePath) { $toolSpec.OverridePath } else { "<none>" }
        $message += " - $($toolSpec.Name): commands=$($toolSpec.Commands -join ', '); override=$overrideSummary; fallbacks=$fallbackSummary"
    }

    Raise-Error ($message -join "`n")
}

function Get-RequiredToolPath {
    param(
        [hashtable]$Tools,
        [string]$Key,
        [string]$Context
    )

    if (-not $Tools.ContainsKey($Key)) {
        Raise-Error "Tool '$Key' was not registered before $Context."
    }

    Require-Value $Tools[$Key] "Tool '$Key' was not resolved before $Context."
}

function Get-RequiredFirstOutputLine {
    param(
        [AllowNull()][object]$Output,
        [string]$Context
    )

    foreach ($line in @($Output)) {
        if ($null -ne $line) {
            $trimmedLine = "$line".Trim()
            if ($trimmedLine) {
                return $trimmedLine
            }
        }
    }

    Raise-Error "Command produced no output for $Context."
}

function Get-VsWhereFindPaths {
    param([string]$VsWherePath, [string]$Pattern)

    $VsWherePath = Require-Value $VsWherePath "Cannot query Visual Studio tool paths because vswhere path is empty."

    @(& $VsWherePath -products '*' -latest -find $Pattern) |
        Where-Object { $_ } |
        ForEach-Object { Convert-ToForwardSlash $_.Trim() }
}

function Get-WindowsSdkToolPaths {
    param([string]$ToolName)

    $sdkRoots = @(
        "${env:ProgramFiles(x86)}/Windows Kits/10/bin",
        "${env:ProgramFiles(x86)}/Windows Kits/8.1/bin"
    )

    $paths = @()
    foreach ($sdkRoot in $sdkRoots) {
        if (-not (Test-Path -LiteralPath $sdkRoot -PathType Container)) { continue }

        $versionDirs = Get-ChildItem -LiteralPath $sdkRoot -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending

        foreach ($versionDir in $versionDirs) {
            $paths += Join-PathForward $versionDir.FullName "x64/$ToolName"
        }

        $paths += Join-PathForward $sdkRoot "x64/$ToolName"
    }

    $paths
}

function Test-ResourceCompilerNeedsVsEnvironment {
    param([hashtable]$ResolvedTools)

    $resourceCompiler = $ResolvedTools["RC"]
    if (-not $resourceCompiler) {
        return $false
    }

    ((Split-Path -Path $resourceCompiler -Leaf) -eq "rc.exe") -and -not $env:INCLUDE
}

function Add-ToolDirectoriesToPath {
    param([string[]]$ToolPaths)

    $pathSeparator = [System.IO.Path]::PathSeparator
    $pathDirs = $env:Path -split [regex]::Escape($pathSeparator) |
        Where-Object { $_ } |
        ForEach-Object { (Convert-ToForwardSlash $_.Trim()).TrimEnd('/') }

    foreach ($toolPath in $ToolPaths) {
        if (-not $toolPath) { continue }

        $toolDir = (Convert-ToForwardSlash (Split-Path -Path $toolPath -Parent)).TrimEnd('/')
        if ($toolDir -and -not ($pathDirs -contains $toolDir)) {
            $env:Path += "$pathSeparator$toolDir"
            $pathDirs += $toolDir
        }
    }
}

function Import-VisualStudioEnvironment {
    param([string]$VsDir)

    $VsDir = Require-Value $VsDir "Cannot import the Visual Studio environment because the Visual Studio installation path is empty."

    $vsDevCmd = Join-PathForward $VsDir "Common7/Tools/VsDevCmd.bat"
    if (-not (Test-Path -LiteralPath $vsDevCmd -PathType Leaf)) {
        Raise-Error "VsDevCmd.bat not found at $vsDevCmd"
    }

    $command = "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul && set"
    $environment = & cmd.exe /s /c $command
    if ($LASTEXITCODE -ne 0) {
        Raise-Error "Failed to load Visual Studio build environment."
    }

    $environment = Require-Value $environment "VsDevCmd.bat completed but did not return environment variables."
    foreach ($line in $environment) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
        }
    }
}

function New-ToolSpec {
    param(
        [string]$Key,
        [string]$Name,
        [string[]]$Commands,
        [string[]]$Fallbacks,
        [AllowNull()][string]$OverridePath,
        [bool]$Required,
        [bool]$VsDiscoverable = $false
    )

    [PSCustomObject]@{
        Key             = $Key
        Name            = $Name
        Commands        = $Commands
        Fallbacks       = @($Fallbacks | Where-Object { $_ })
        OverridePath    = $OverridePath
        Required        = $Required
        VsDiscoverable  = $VsDiscoverable
    }
}

function New-PlatformProfile {
    param(
        [object]$Options,
        [bool]$WindowsPlatform
    )

    if ($WindowsPlatform) {
        $needsWindowsBuildTools = $Options.EnableConfigure -or $Options.EnableBuild
        $baseLlvmToolDirs = @("${env:ProgramFiles}/LLVM/bin", "${env:ProgramFiles(x86)}/LLVM/bin")
        $cmakeFallbacks = @(
            "${env:ProgramFiles}/CMake/bin/cmake.exe",
            "${env:ProgramFiles(x86)}/CMake/bin/cmake.exe"
        )
        $gitFallbacks = @(
            "${env:ProgramFiles}/Git/cmd/git.exe",
            "${env:ProgramFiles}/Git/bin/git.exe",
            "${env:ProgramFiles(x86)}/Git/cmd/git.exe",
            "${env:ProgramFiles(x86)}/Git/bin/git.exe"
        )
        $sevenZipFallbacks = @(
            "${env:ProgramFiles}/7-Zip/7z.exe",
            "${env:ProgramFiles(x86)}/7-Zip/7z.exe"
        )
        $clangFallbacks = @($baseLlvmToolDirs | ForEach-Object { "$_/clang-cl.exe" })
        $rcFallbacks = @($baseLlvmToolDirs | ForEach-Object { "$_/llvm-rc.exe" }) + @(Get-WindowsSdkToolPaths "rc.exe")
        $mtFallbacks = @($baseLlvmToolDirs | ForEach-Object { "$_/llvm-mt.exe" }) + @(Get-WindowsSdkToolPaths "mt.exe")

        return [PSCustomObject]@{
            Name               = "Windows"
            NeedsVsEnvironment = $needsWindowsBuildTools
            ToolSpecs          = @(
                New-ToolSpec -Key "CMake"    -Name "CMake"             -Commands @("cmake.exe", "cmake")          -Fallbacks $cmakeFallbacks     -OverridePath $Options.CMakePath    -Required ($Options.EnableConfigure -or $Options.EnableBuild) -VsDiscoverable $true
                New-ToolSpec -Key "Ninja"    -Name "Ninja"             -Commands @("ninja.exe", "ninja")          -Fallbacks @()                 -OverridePath $Options.NinjaPath    -Required $Options.EnableConfigure                            -VsDiscoverable $true
                New-ToolSpec -Key "Git"      -Name "Git"               -Commands @("git.exe", "git")              -Fallbacks $gitFallbacks        -OverridePath $Options.GitPath      -Required $true                                               -VsDiscoverable $false
                New-ToolSpec -Key "SevenZip" -Name "7-Zip"             -Commands @("7z.exe", "7z")                -Fallbacks $sevenZipFallbacks   -OverridePath $Options.SevenZipPath -Required $Options.EnablePackage                               -VsDiscoverable $false
                New-ToolSpec -Key "Clang"    -Name "clang-cl"          -Commands @("clang-cl.exe")                -Fallbacks $clangFallbacks      -OverridePath $Options.ClangPath    -Required $needsWindowsBuildTools                              -VsDiscoverable $true
                New-ToolSpec -Key "RC"       -Name "resource compiler" -Commands @("llvm-rc.exe")                 -Fallbacks $rcFallbacks         -OverridePath $Options.RcPath       -Required $needsWindowsBuildTools                              -VsDiscoverable $true
                New-ToolSpec -Key "MT"       -Name "manifest tool"     -Commands @("llvm-mt.exe")                 -Fallbacks $mtFallbacks         -OverridePath $Options.MtPath       -Required $needsWindowsBuildTools                              -VsDiscoverable $true
            )
            ConfigureToolDefines = @(
                [PSCustomObject]@{ Name="CMAKE_MT";           ToolKey="MT"    }
                [PSCustomObject]@{ Name="CMAKE_C_COMPILER";   ToolKey="Clang" }
                [PSCustomObject]@{ Name="CMAKE_CXX_COMPILER"; ToolKey="Clang" }
                [PSCustomObject]@{ Name="CMAKE_RC_COMPILER";  ToolKey="RC"    }
            )
            EnvironmentTools = @(
                [PSCustomObject]@{ Name="CC";  ToolKey="Clang" }
                [PSCustomObject]@{ Name="CXX"; ToolKey="Clang" }
            )
            PackageSpec = [PSCustomObject]@{
                RuntimePatterns  = @("*.dll", "*.exe", "Resources")
                RequiredFiles    = @("OIViewer.exe", "Resources")
                SymbolPatterns   = @("*.pdb")
                ArchiveExtension = ".7z"
                ArchiveToolKey   = "SevenZip"
                ArchiveArgs      = @("a", "-mx9")
            }
        }
    }

    # Linux profile — package support not yet implemented.
    [PSCustomObject]@{
        Name               = "Linux"
        NeedsVsEnvironment = $false
        ToolSpecs          = @(
            New-ToolSpec -Key "CMake"  -Name "CMake"  -Commands @("cmake")  -Fallbacks @() -OverridePath $Options.CMakePath  -Required ($Options.EnableConfigure -or $Options.EnableBuild) -VsDiscoverable $false
            New-ToolSpec -Key "Ninja"  -Name "Ninja"  -Commands @("ninja")  -Fallbacks @() -OverridePath $Options.NinjaPath  -Required $Options.EnableConfigure                            -VsDiscoverable $false
            New-ToolSpec -Key "Git"    -Name "Git"    -Commands @("git")    -Fallbacks @() -OverridePath $Options.GitPath    -Required $true                                               -VsDiscoverable $false
        )
        ConfigureToolDefines = @()
        EnvironmentTools     = @()
        PackageSpec          = $null
    }
}

function Resolve-BuildTools {
    param(
        [object]$Profile,
        [string]$InitialPath,
        [bool]$EchoCommands
    )

    $toolSpecs = $Profile.ToolSpecs
    $resolvedTools = Resolve-ToolPaths $toolSpecs $InitialPath
    $missingTools = @(Get-MissingRequiredToolSpecs $toolSpecs $resolvedTools)
    $missingVsDiscoverableTools = @($missingTools | Where-Object { $_.VsDiscoverable })
    $needsVsEnvironment = $Profile.NeedsVsEnvironment -and (Test-ResourceCompilerNeedsVsEnvironment $resolvedTools)

    # VS discovery runs when required tools are missing or SDK rc.exe needs INCLUDE from VsDevCmd.
    # Standalone LLVM tools and an existing developer environment do not need this fallback.
    if ($Profile.Name -eq "Windows" -and ($missingVsDiscoverableTools.Count -gt 0 -or $needsVsEnvironment)) {
        $vsWherePath = Resolve-ToolPath -Name "vswhere" -CommandNames @("vswhere.exe") `
            -FallbackPaths @("${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe") `
            -PreferredPath $InitialPath -OverridePath $null -Required $true

        $vsDirOutput = Invoke-NativeCommand $vsWherePath @("-latest", "-requires", "Microsoft.Component.MSBuild", "-property", "installationPath") "Visual Studio discovery failed." $EchoCommands -PassThru
        $vsDir = Convert-ToForwardSlash (Get-RequiredFirstOutputLine $vsDirOutput "Visual Studio installation discovery")

        if ($Profile.NeedsVsEnvironment) {
            Write-Host "Importing Visual Studio build environment from: $vsDir"
            Import-VisualStudioEnvironment $vsDir
        }

        $vsCMakePaths = Get-VsWhereFindPaths $vsWherePath "**\cmake.exe"
        $vsNinjaPaths = Get-VsWhereFindPaths $vsWherePath "**\ninja.exe"
        $vsLlvmBinDir = "$vsDir/VC/Tools/Llvm/x64/bin"

        # Rebuild fallback paths for VS-discoverable specs: CMake/Ninja get vswhere-found paths;
        # other VS tools (Clang, RC, MT) get the VS LLVM bin directory prepended.
        $vsToolSpecs = @($toolSpecs | Where-Object { $_.VsDiscoverable } | ForEach-Object {
            $vsFallbacks = switch ($_.Key) {
                "CMake" { @($vsCMakePaths) + $_.Fallbacks }
                "Ninja" { @($vsNinjaPaths) + $_.Fallbacks }
                default { @("$vsLlvmBinDir/$($_.Commands[0])") + $_.Fallbacks }
            }
            New-ToolSpec -Key $_.Key -Name $_.Name -Commands $_.Commands -Fallbacks $vsFallbacks `
                -OverridePath $_.OverridePath -Required $_.Required -VsDiscoverable $_.VsDiscoverable
        })

        $vsResolved = Resolve-ToolPaths $vsToolSpecs $InitialPath
        $vsSpecsByKey = @{}; foreach ($spec in $vsToolSpecs) { $vsSpecsByKey[$spec.Key] = $spec }
        foreach ($spec in $vsToolSpecs) { $resolvedTools[$spec.Key] = $vsResolved[$spec.Key] }
        $toolSpecs = @($toolSpecs | ForEach-Object { if ($vsSpecsByKey.ContainsKey($_.Key)) { $vsSpecsByKey[$_.Key] } else { $_ } })
    }

    Assert-RequiredToolsResolved $toolSpecs $resolvedTools "after PATH and fallback discovery"
    Add-ToolDirectoriesToPath @($resolvedTools.Values)

    [PSCustomObject]@{
        Paths   = $resolvedTools
        Display = $toolSpecs | ForEach-Object {
            [PSCustomObject]@{
                Name = $_.Name
                Path = $resolvedTools[$_.Key]
            }
        }
    }
}

#---------------------------------------------------------------
# CMake Cache Helpers
#---------------------------------------------------------------

function Read-CMakeCache {
    param([string]$CachePath)

    $entries = @{}
    if (-not (Test-Path -LiteralPath $CachePath -PathType Leaf)) {
        return $entries
    }

    foreach ($line in Get-Content -Path $CachePath) {
        if ($line -match '^([^#/:][^:=]*)(?::[^=]*)?=(.*)$') {
            $entries[$matches[1]] = $matches[2]
        }
    }

    $entries
}

function Normalize-ComparablePath {
    param([AllowNull()][string]$Path)

    if ($null -eq $Path) { return $null }

    $normalizedPath = (Convert-ToForwardSlash $Path.Trim('"')).TrimEnd('/')
    if (Test-Path -LiteralPath $normalizedPath -PathType Leaf) {
        return (Convert-ToForwardSlash (Resolve-Path -LiteralPath $normalizedPath).Path).TrimEnd('/')
    }

    $normalizedPath
}

function Test-CMakeCacheValueEquals {
    param(
        [AllowNull()][string]$Actual,
        [AllowNull()][string]$Expected,
        [bool]$IsPath
    )

    if ($IsPath) {
        $Actual = Normalize-ComparablePath $Actual
        $Expected = Normalize-ComparablePath $Expected
    }

    [string]::Equals($Actual, $Expected, [System.StringComparison]::OrdinalIgnoreCase)
}

function Format-CMakeCacheValue {
    param([AllowNull()][string]$Value)

    if ($null -eq $Value) { return "<missing>" }
    if ($Value -eq "") { return "<empty>" }

    $Value
}

function Compare-CMakeCacheEntries {
    param(
        [hashtable]$Cache,
        [object[]]$Requirements
    )

    $mismatches = @($Requirements | ForEach-Object {
        $actual = $Cache[$_.Name]
        if (-not (Test-CMakeCacheValueEquals $actual $_.Value $_.IsPath)) {
            [PSCustomObject]@{ Name=$_.Name; Actual=$actual; Expected=$_.Value }
        }
    })

    $mismatches
}

function Clear-CMakeConfigureState {
    param(
        [string]$BuildDir,
        [string]$Reason
    )

    if (-not (Test-Path -LiteralPath $BuildDir -PathType Container)) {
        return
    }

    Write-Host $Reason

    $cachePath = Join-PathForward $BuildDir "CMakeCache.txt"
    if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
        Remove-Item -LiteralPath $cachePath -Force
    }

    $cmakeFilesDir = Join-PathForward $BuildDir "CMakeFiles"
    if (Test-Path -LiteralPath $cmakeFilesDir -PathType Container) {
        $resolvedBuildDir = (Resolve-Path -LiteralPath $BuildDir).Path.TrimEnd('\', '/')
        $resolvedCMakeFilesDir = (Resolve-Path -LiteralPath $cmakeFilesDir).Path
        $expectedPrefix = $resolvedBuildDir + [System.IO.Path]::DirectorySeparatorChar
        $isUnderBuildDir = $resolvedCMakeFilesDir.StartsWith($expectedPrefix, [System.StringComparison]::OrdinalIgnoreCase)
        if (-not $isUnderBuildDir) {
            Raise-Error "Refusing to remove unexpected CMakeFiles directory: $resolvedCMakeFilesDir"
        }

        Remove-Item -LiteralPath $cmakeFilesDir -Recurse -Force
    }
}

function Reset-CMakeConfigureStateIfNeeded {
    param(
        [string]$BuildDir,
        [object[]]$Requirements,
        [bool]$CleanConfigure,
        [bool]$CleanConfigureOnMismatch
    )

    if ($CleanConfigure) {
        Clear-CMakeConfigureState $BuildDir "Clearing CMake cache state because -CleanConfigure was requested."
        return
    }

    if (-not $CleanConfigureOnMismatch) {
        return
    }

    $cachePath = Join-PathForward $BuildDir "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
        return
    }

    $cache = Read-CMakeCache $cachePath
    $mismatches = @(Compare-CMakeCacheEntries $cache $Requirements)
    if ($mismatches.Count -eq 0) {
        return
    }

    Write-Host "Existing CMake cache does not match the requested publish configuration."
    foreach ($mismatch in $mismatches) {
        Write-Host (" - {0}: cached {1}, requested {2}" -f $mismatch.Name, (Format-CMakeCacheValue $mismatch.Actual), (Format-CMakeCacheValue $mismatch.Expected))
    }

    Clear-CMakeConfigureState $BuildDir "Clearing CMake cache state before reconfigure."
}

function Assert-CMakeCacheMatches {
    param(
        [string]$BuildDir,
        [object[]]$Requirements
    )

    $cachePath = Join-PathForward $BuildDir "CMakeCache.txt"
    $cache = Read-CMakeCache $cachePath
    $mismatches = @(Compare-CMakeCacheEntries $cache $Requirements)
    if ($mismatches.Count -eq 0) {
        return
    }

    foreach ($mismatch in $mismatches) {
        Write-Host ("Unexpected CMake cache value for {0}: configured {1}, requested {2}" -f $mismatch.Name, (Format-CMakeCacheValue $mismatch.Actual), (Format-CMakeCacheValue $mismatch.Expected)) -ForegroundColor Red
    }

    Raise-Error "CMake configuration did not produce the requested publish settings."
}

#---------------------------------------------------------------
# Publish Context
#---------------------------------------------------------------

function New-CMakeDefine {
    param(
        [string]$Name,
        [AllowNull()][string]$Value,
        [bool]$IsPath = $false,
        [bool]$Validate = $true
    )

    [PSCustomObject]@{
        Name     = $Name
        Value    = $Value
        IsPath   = $IsPath
        Validate = $Validate
    }
}

function New-CMakeConfigureSpec {
    param([object]$Context)

    $platformDefines = @($Context.Profile.ConfigureToolDefines | ForEach-Object {
        New-CMakeDefine -Name $_.Name -Value $Context.Tools[$_.ToolKey] -IsPath $true
    })
    $defines = @(
        New-CMakeDefine -Name "CMAKE_BUILD_TYPE"                -Value $Context.BuildType
        New-CMakeDefine -Name "CMAKE_MAKE_PROGRAM"              -Value $Context.Tools["Ninja"]              -IsPath $true
        New-CMakeDefine -Name "OIV_OFFICIAL_BUILD"              -Value "$($Context.OfficialBuild)"
        New-CMakeDefine -Name "OIV_OFFICIAL_RELEASE"            -Value "$($Context.OfficialRelease)"
        New-CMakeDefine -Name "OIV_VERSION_BUILD"               -Value "$($Context.VersionBuild)"
        New-CMakeDefine -Name "OIV_RELEASE_SUFFIX"              -Value "$($Context.ReleaseSuffix)"
    ) + $platformDefines

    $configureArgs = @("-S", $Context.RootDir, "-B", $Context.BuildDir, "-G", $Context.Generator) +
        @($defines | ForEach-Object { "-D$($_.Name)=$($_.Value)" })

    $cacheRequirements = @(
        [PSCustomObject]@{ Name="CMAKE_GENERATOR"; Value=$Context.Generator; IsPath=$false }
    ) + @($defines | Where-Object { $_.Validate })

    [PSCustomObject]@{
        Defines           = $defines
        ConfigureArgs     = $configureArgs
        CacheRequirements = $cacheRequirements
    }
}

function Get-VersionValue {
    param([string[]]$Content, [string]$Token)

    $line = $Content | Where-Object { $_ -match "$Token\s*=" }
    if ($line -and "$line" -match "\d+") { return [int]$matches[0] }

    Raise-Error "Missing version token: $Token"
}

function Get-GitOutput {
    param([object]$Context, [string[]]$Arguments, [string]$ErrorMessage)

    $commandArgs = @{
        FilePath     = Get-RequiredToolPath $Context.Tools "Git" "reading Git metadata"
        Arguments    = $Arguments
        ErrorMessage = $ErrorMessage
        EchoCommands = $Context.EchoCommands
        PassThru     = $true
    }
    $output = Invoke-NativeCommand @commandArgs

    Get-RequiredFirstOutputLine $output $ErrorMessage
}

function Convert-ToPackageToken {
    param([AllowNull()][string]$Value)

    if (-not $Value) { return "unknown" }

    $token = $Value.Trim() -replace '[^A-Za-z0-9]+', ''
    if (-not $token) { return "unknown" }

    $token.ToLowerInvariant()
}

function Read-CMakeSetFile {
    param([string]$PathPattern)

    $cmakeFiles = @(Resolve-Path -Path $PathPattern -ErrorAction SilentlyContinue)
    if ($cmakeFiles.Count -eq 0) {
        return @{}
    }

    $entries = @{}
    foreach ($line in Get-Content -LiteralPath $cmakeFiles[0].Path) {
        if ($line -match '^set\(([A-Za-z0-9_]+)\s+"?([^"\)]*)"?\)') {
            $entries[$matches[1]] = $matches[2]
        }
    }

    $entries
}

function Get-FirstTextValue {
    param([AllowNull()][string[]]$Values)

    foreach ($value in $Values) {
        if ($value) {
            $trimmedValue = $value.Trim()
            if ($trimmedValue) {
                return $trimmedValue
            }
        }
    }

    $null
}

function Get-RequiredFirstTextValue {
    param(
        [AllowNull()][string[]]$Values,
        [string]$Context
    )

    $value = Get-FirstTextValue -Values $Values
    Require-Value $value "Missing required value for $Context."
}

function Convert-ToPackageTargetSystem {
    param([string]$SystemName)

    $targetSystem = ([string](Require-Value $SystemName "Cannot convert package target system because CMake system name is empty.")).Trim()

    switch -Regex ($targetSystem) {
        '^(Windows|WindowsStore|MSYS|MINGW)$' { return "Win32" }
        '^Linux$'                            { return "linux" }
        default                              { return Convert-ToPackageToken $targetSystem }
    }
}

function Convert-ToPackagePlatform {
    param([string]$Architecture)

    $packageArchitecture = [string](Require-Value $Architecture "Cannot convert package platform because CMake architecture is empty.")
    switch -Regex ($packageArchitecture) {
        '^(AMD64|x86_64|x64)$'     { return "x64" }
        '^(i[3-6]86|x86|Win32)$'   { return "x86" }
        '^(ARM64|AArch64|arm64)$'  { return "arm64" }
        default                    { return Convert-ToPackageToken $packageArchitecture }
    }
}

function Convert-ToPackageToolchain {
    param([string]$ToolchainSource)

    $source = [string](Require-Value $ToolchainSource "Cannot convert package toolchain because the compiler identifier is empty.")
    switch -Regex ($source) {
        'Clang|clang'        { return "LLVM" }
        'MSVC|(^|/)cl\.exe$' { return "VC" }
        'GNU|GCC|gcc|g\+\+'  { return "gnu" }
        default              { return Convert-ToPackageToken $source }
    }
}

function Get-PackageBuildInfo {
    param([object]$Context)

    $cachePath = Join-PathForward $Context.BuildDir "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
        Raise-Error "CMake cache not found. Run configure before reading package target information: $cachePath"
    }

    $cache = Read-CMakeCache $cachePath
    $systemInfo = Read-CMakeSetFile (Join-PathForward $Context.BuildDir "CMakeFiles/*/CMakeSystem.cmake")
    $cxxCompilerInfo = Read-CMakeSetFile (Join-PathForward $Context.BuildDir "CMakeFiles/*/CMakeCXXCompiler.cmake")

    $systemName = Get-RequiredFirstTextValue @(
        $cache["CMAKE_SYSTEM_NAME"],
        $systemInfo["CMAKE_SYSTEM_NAME"],
        $cxxCompilerInfo["CMAKE_SYSTEM_NAME"]
    ) "package target system metadata from CMake cache/system files"
    $architecture = Get-RequiredFirstTextValue @(
        $cache["CMAKE_SYSTEM_PROCESSOR"],
        $systemInfo["CMAKE_SYSTEM_PROCESSOR"],
        $cxxCompilerInfo["CMAKE_CXX_COMPILER_ARCHITECTURE_ID"],
        $cache["CMAKE_VS_PLATFORM_NAME"],
        $cache["CMAKE_GENERATOR_PLATFORM"]
    ) "package architecture metadata from CMake cache/system files"
    $compilerId = Get-FirstTextValue @(
        $cache["CMAKE_CXX_COMPILER_ID"],
        $cxxCompilerInfo["CMAKE_CXX_COMPILER_ID"]
    )
    $compilerPath = Get-FirstTextValue @(
        $cache["CMAKE_CXX_COMPILER"],
        $cxxCompilerInfo["CMAKE_CXX_COMPILER"]
    )
    $compilerName = if ($compilerPath) { Split-Path -Path $compilerPath -Leaf } else { $null }
    $toolchainSource = Get-RequiredFirstTextValue @($compilerId, $compilerName) "package compiler metadata from CMake cache/compiler files"

    [PSCustomObject]@{
        TargetSystem      = Convert-ToPackageTargetSystem $systemName
        Platform          = Convert-ToPackagePlatform $architecture
        Toolchain         = Convert-ToPackageToolchain $toolchainSource
        SourceSystemName  = $systemName
        SourceArchitecture = $architecture
        SourceCompilerId  = $compilerId
        SourceCompilerPath = $compilerPath
    }
}

function Format-PackageNameSuffix {
    param([object]$BuildInfo)

    "$($BuildInfo.TargetSystem)$($BuildInfo.Platform)$($BuildInfo.Toolchain)"
}

function Write-PackageBuildInfo {
    param([object]$BuildInfo)

    Write-Host ""
    Write-Host "Package Target"
    Write-Host "-----------------------------------------------"
    Write-Host ("Target System : {0} ({1})" -f $BuildInfo.TargetSystem, (Format-CMakeCacheValue $BuildInfo.SourceSystemName))
    Write-Host ("Architecture  : {0} ({1})" -f $BuildInfo.Platform, (Format-CMakeCacheValue $BuildInfo.SourceArchitecture))
    Write-Host ("Tool Set      : {0} ({1})" -f $BuildInfo.Toolchain, (Format-CMakeCacheValue $BuildInfo.SourceCompilerId))
    Write-Host ("Compiler      : {0}" -f (Format-CMakeCacheValue $BuildInfo.SourceCompilerPath))
    Write-Host ("Name Suffix   : {0}" -f (Format-PackageNameSuffix $BuildInfo))
    Write-Host "==============================================="
}

function Update-PackageBuildInfo {
    param([object]$Context)

    $packageBuildInfo = Get-PackageBuildInfo $Context
    $Context.PackageBuildInfo = $packageBuildInfo
    $Context.PackageNameSuffix = Format-PackageNameSuffix $packageBuildInfo
}

function New-PublishContext {
    param(
        [object]$Options,
        [object]$ToolInfo,
        [bool]$WindowsPlatform,
        [object]$Profile
    )

    $rootDir = Convert-ToForwardSlash $PSScriptRoot
    $resolvedBuildDir = Resolve-BuildPath $rootDir $Options.BuildDir
    $binDir = Join-PathForward $resolvedBuildDir "bin"
    $versionHeader = Join-PathForward $rootDir "OIVLib/OIV/Include/Version.h"
    $gitDir = Join-PathForward $rootDir ".git"

    $context = [PSCustomObject]@{
        RootDir                  = $rootDir
        BuildDir                 = $resolvedBuildDir
        BinDir                   = $binDir
        VersionHeader            = $versionHeader
        GitDir                   = $gitDir
        BuildType                = $Options.BuildType
        EnableConfigure          = $Options.EnableConfigure
        EnableBuild              = $Options.EnableBuild
        EnablePackage            = $Options.EnablePackage
        CleanConfigure           = $Options.CleanConfigure
        CleanConfigureOnMismatch = $Options.CleanConfigureOnMismatch
        EchoCommands             = $Options.EchoCommands
        Generator                = "Ninja"
        WindowsPlatform          = $WindowsPlatform
        Profile                  = $Profile
        Tools                    = $ToolInfo.Paths
        ToolDisplay              = $ToolInfo.Display
        OfficialBuild            = if ($Options.OfficialBuild) { 1 } else { 0 }
        OfficialRelease          = if ($Options.OfficialRelease) { 1 } else { 0 }
        ReleaseSuffix            = ""
        VersionBuild             = 0
        DateShort                = $null
        DateLong                 = $null
        VersionShort             = $null
        VersionFull              = $null
        PackageBuildInfo         = $null
        PackageNameSuffix        = $null
    }

    $fileContent = Get-Content -Path $context.VersionHeader
    $major = Get-VersionValue $fileContent "OIV_VERSION_MAJOR"
    $minor = Get-VersionValue $fileContent "OIV_VERSION_MINOR"
    $gitDirArg = "--git-dir=$($context.GitDir)"
    $revision = Get-GitOutput $context @($gitDirArg, "rev-list", "HEAD", "--count") "Failed to read Git revision count."
    $now = Get-Date

    $context.DateShort = $now.ToString("yyyy-MM-dd")
    $context.DateLong = $now.ToString("yyyy-MM-dd_HH-mm-ss")
    $context.VersionShort = "$major.$minor.$revision.$($context.VersionBuild)"
    $context.VersionFull = if ($context.OfficialRelease -eq 0) {
        $shortHash = Get-GitOutput $context @($gitDirArg, "rev-parse", "--short", "HEAD") "Failed to read Git short hash."
        "$($context.VersionShort)-$shortHash"
    } else {
        $context.VersionShort
    }

    $context
}

#---------------------------------------------------------------
# Configure, Build, Package
#---------------------------------------------------------------

function Write-BuildInfo {
    param([object]$Context)

    Write-Host "`nFound Tools"
    $Context.ToolDisplay | Format-Table Name, Path -AutoSize
    Write-Host ""

    Write-Host "Build Info"
    Write-Host "-----------------------------------------------"
    Write-Host ("Root Dir      : {0}" -f $Context.RootDir)
    Write-Host ("Build Dir     : {0}" -f $Context.BuildDir)
    Write-Host ("Bin Dir       : {0}" -f $Context.BinDir)
    Write-Host ("Official Build: {0}" -f [bool]$Context.OfficialBuild)
    Write-Host ("Official Rel. : {0}" -f [bool]$Context.OfficialRelease)
    Write-Host ("Version Short : {0}" -f $Context.VersionShort)
    Write-Host ("Version Full  : {0}" -f $Context.VersionFull)
    Write-Host ("Date          : {0}" -f $Context.DateShort)
    Write-Host ("Time          : {0}" -f $Context.DateLong)
    Write-Host "==============================================="
}

function Invoke-CMakeConfigure {
    param([object]$Context, [object]$ConfigureSpec)

    Write-Host "Running CMake configure..."
    $resetArgs = @{
        BuildDir                 = $Context.BuildDir
        Requirements             = $ConfigureSpec.CacheRequirements
        CleanConfigure           = $Context.CleanConfigure
        CleanConfigureOnMismatch = $Context.CleanConfigureOnMismatch
    }
    Reset-CMakeConfigureStateIfNeeded @resetArgs

    $commandArgs = @{
        FilePath     = Get-RequiredToolPath $Context.Tools "CMake" "CMake configure"
        Arguments    = $ConfigureSpec.ConfigureArgs
        ErrorMessage = "CMake configuration failed."
        EchoCommands = $Context.EchoCommands
    }
    Invoke-NativeCommand @commandArgs

    Assert-CMakeCacheMatches $Context.BuildDir $ConfigureSpec.CacheRequirements
}

function Invoke-CMakeBuild {
    param([object]$Context)

    Write-Host "Building OIViewer..."
    $buildArgs = @("--build", $Context.BuildDir, "--config", $Context.BuildType, "--target", "OIViewer")
    $commandArgs = @{
        FilePath     = Get-RequiredToolPath $Context.Tools "CMake" "CMake build"
        Arguments    = $buildArgs
        ErrorMessage = "Build failed."
        EchoCommands = $Context.EchoCommands
    }
    Invoke-NativeCommand @commandArgs
}

function Set-BuildToolEnvironment {
    param([object]$Context)

    foreach ($entry in $Context.Profile.EnvironmentTools) {
        $toolPath = Get-RequiredToolPath $Context.Tools $entry.ToolKey "setting $($entry.Name)"
        [Environment]::SetEnvironmentVariable($entry.Name, $toolPath, 'Process')
    }
}

function Test-PathPattern {
    param([string]$Path)

    if ([System.Management.Automation.WildcardPattern]::ContainsWildcardCharacters($Path)) {
        return [bool](Resolve-Path -Path $Path -ErrorAction SilentlyContinue)
    }

    Test-Path -LiteralPath $Path
}

function Assert-PackageInputs {
    param([string[]]$Inputs, [string]$Description)

    foreach ($inputPath in $Inputs) {
        if (-not (Test-PathPattern $inputPath)) {
            Raise-Error "Missing $Description package input: $inputPath"
        }
    }
}

function New-Archive {
    param(
        [object]$Context,
        [string]$OutputPath,
        [string[]]$Inputs
    )

    $spec = $Context.Profile.PackageSpec
    $arguments = $spec.ArchiveArgs + @($OutputPath) + $Inputs
    $commandArgs = @{
        FilePath     = Get-RequiredToolPath $Context.Tools $spec.ArchiveToolKey "packaging archive creation"
        Arguments    = $arguments
        ErrorMessage = "Packaging failed while creating $OutputPath."
        EchoCommands = $Context.EchoCommands
    }
    Invoke-NativeCommand @commandArgs
}

function Invoke-Package {
    param([object]$Context)

    $packageSpec = $Context.Profile.PackageSpec
    if ($null -eq $packageSpec) {
        Raise-Error "Packaging is not implemented for the $($Context.Profile.Name) platform."
    }

    Write-Host "Packaging..."

    $outputDir = Join-PathForward $Context.BuildDir "$($Context.DateLong)-v$($Context.VersionShort)"
    # Defensive guard: callers that bypass Run-OIVBuild may invoke Invoke-Package directly
    # without first calling Update-PackageBuildInfo.
    if (-not $Context.PackageNameSuffix) {
        Update-PackageBuildInfo $Context
    }

    $baseName = Join-PathForward $outputDir "$($Context.DateShort)-OIV-$($Context.VersionFull)-$($Context.PackageNameSuffix)"

    Ensure-Directory $outputDir

    $runtimePackageInputs = @($packageSpec.RuntimePatterns | ForEach-Object { Join-PathForward $Context.BinDir $_ })
    $symbolsPackageInputs  = @($packageSpec.SymbolPatterns  | ForEach-Object { Join-PathForward $Context.BinDir $_ })
    $requiredRuntimeInputs = @($packageSpec.RequiredFiles   | ForEach-Object { Join-PathForward $Context.BinDir $_ })

    # Verify specific required named files exist.
    Assert-PackageInputs $requiredRuntimeInputs "runtime"
    # Pre-flight wildcard check: ensures archive inputs are never silently empty.
    Assert-PackageInputs $runtimePackageInputs "runtime"
    Assert-PackageInputs $symbolsPackageInputs "symbols"

    New-Archive $Context "$baseName-Symbols$($packageSpec.ArchiveExtension)" $symbolsPackageInputs
    New-Archive $Context "$baseName$($packageSpec.ArchiveExtension)" $runtimePackageInputs
}

#---------------------------------------------------------------
# Main Build Function
#---------------------------------------------------------------

function Run-OIVBuild {
    $options = [PSCustomObject]@{
        BuildType                = $BuildType
        BuildDir                 = $BuildDir
        EnableConfigure          = $EnableConfigure
        EnableBuild              = $EnableBuild
        EnablePackage            = $EnablePackage
        OfficialBuild            = $OfficialBuild
        OfficialRelease          = $OfficialRelease
        CleanConfigure           = [bool]$CleanConfigure
        CleanConfigureOnMismatch = $CleanConfigureOnMismatch
        EchoCommands             = [bool]$EchoCommands
        CMakePath                = $CMakePath
        NinjaPath                = $NinjaPath
        SevenZipPath             = $SevenZipPath
        GitPath                  = $GitPath
        ClangPath                = $ClangPath
        RcPath                   = $RcPath
        MtPath                   = $MtPath
    }

    $initialPath = $env:Path
    $windowsPlatform = Test-IsWindowsPlatform
    $profile = New-PlatformProfile $options $windowsPlatform
    $toolInfo = Resolve-BuildTools $profile $initialPath $options.EchoCommands
    $context = New-PublishContext $options $toolInfo $windowsPlatform $profile

    Set-BuildToolEnvironment $context
    Write-BuildInfo $context

    if ($context.EnableConfigure) {
        Invoke-CMakeConfigure $context (New-CMakeConfigureSpec $context)
    }

    if ($context.EnableConfigure -or $context.EnableBuild -or $context.EnablePackage) {
        Update-PackageBuildInfo $context
        Write-PackageBuildInfo $context.PackageBuildInfo
    }

    if ($context.EnableBuild) {
        Invoke-CMakeBuild $context
    }

    if ($context.EnablePackage) {
        Invoke-Package $context
    }

    Write-Host "`nBuild & Packaging Complete!" -ForegroundColor Green
}

#---------------------------------------------------------------
# Top-Level Script Wrapper
#---------------------------------------------------------------

$NotifyBuildFailed = $true

try {
    Run-OIVBuild
    $NotifyBuildFailed = $false
} catch {
    Write-Host "`nScript failed." -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red

    if ($_.InvocationInfo) {
        Write-Host "`nLocation:"
        Write-Host $_.InvocationInfo.PositionMessage
    }

    if ($_.ScriptStackTrace) {
        Write-Host "`nScript stack trace:"
        Write-Host $_.ScriptStackTrace
    }

    Write-Host "`nException:"
    Write-Host $_.Exception.ToString()

    $NotifyBuildFailed = $false
    exit 1
} finally {
    if ($NotifyBuildFailed) {
        Write-Host "`nScript failed or cancelled by the user" -ForegroundColor Red
    }
}
