#===============================================================
# OIV.Build.Helpers - Shared build and packaging helpers
#===============================================================

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

#---------------------------------------------------------------
# Path utilities
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

function Convert-ToForwardSlash {
    param([AllowNull()][string]$Path)
    if (-not $Path) { return $Path }
    $Path -replace '\\', '/'
}

function Resolve-BuildPath {
    param([string]$RootDir, [string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) { return Convert-ToForwardSlash $Path }
    Join-PathForward $RootDir $Path
}

function Ensure-Directory {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

#---------------------------------------------------------------
# Error handling
#---------------------------------------------------------------

function Raise-Error {
    param([string]$Message)
    throw [System.InvalidOperationException]::new($Message)
}

function Require-Value {
    param(
        [AllowNull()][object]$Value,
        [string]$Message
    )
    if ($null -eq $Value) { Raise-Error $Message }
    if ($Value -is [string] -and [string]::IsNullOrWhiteSpace($Value)) { Raise-Error $Message }
    if ($Value -is [array] -and $Value.Count -eq 0) { Raise-Error $Message }
    $Value
}

#---------------------------------------------------------------
# Command execution
#---------------------------------------------------------------

function Quote-CommandArgument {
    param([AllowNull()][string]$Argument)
    if ($null -eq $Argument -or $Argument -eq "") { return '""' }
    if ($Argument -match '[\s"]') { return '"' + ($Argument -replace '"', '\"') + '"' }
    $Argument
}

function Format-CommandLine {
    param([string]$FilePath, [string[]]$Arguments)
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
    if ($EchoCommands) { Write-Host ("+ {0}" -f $commandLine) }
    try {
        if ($PassThru) {
            $output = & $FilePath @Arguments
            if ($LASTEXITCODE -ne 0) { Raise-Error "$ErrorMessage`nCommand: $commandLine`nExit code: $LASTEXITCODE" }
            return $output
        }
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) { Raise-Error "$ErrorMessage`nCommand: $commandLine`nExit code: $LASTEXITCODE" }
    } catch {
        if ($_.Exception -is [System.InvalidOperationException]) { throw }
        throw [System.InvalidOperationException]::new("$ErrorMessage`nCommand: $commandLine", $_.Exception)
    }
}

function Get-RequiredFirstOutputLine {
    param([AllowNull()][object]$Output, [string]$Context)
    foreach ($line in @($Output)) {
        if ($null -ne $line) {
            $trimmedLine = "$line".Trim()
            if ($trimmedLine) { return $trimmedLine }
        }
    }
    Raise-Error "Command produced no output for $Context."
}

#---------------------------------------------------------------
# Tool resolution
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
    param([string[]]$CommandNames, [string]$PathValue = $env:PATH)
    if (-not $PathValue) { return $null }
    $previousPath = $env:PATH
    try {
        $env:PATH = $PathValue
        foreach ($commandName in $CommandNames) {
            $command = Get-Command $commandName -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($command) { return Convert-ToForwardSlash $command.Source }
        }
    } finally { $env:PATH = $previousPath }
    return $null
}

function Resolve-ToolPath {
    param(
        [string]$Name,
        [string]$Key,
        [string[]]$CommandNames,
        [string[]]$FallbackPaths,
        [string]$PreferredPath,
        [string]$OverridePath,
        [bool]$Required,
        [bool]$VsDiscoverable = $false
    )
    if ($OverridePath) {
        if (Test-Path -LiteralPath $OverridePath -PathType Leaf) { return Convert-ToForwardSlash (Resolve-Path -LiteralPath $OverridePath).Path }
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
    if ($Required -and $VsDiscoverable -and $Key) {
        $vsModule = Get-Module VSDiscovery | Select-Object -First 1
        if (-not $vsModule) {
            $vsModule = Import-Module (Join-Path $PSScriptRoot "VSDiscovery.psm1") -PassThru -DisableNameChecking -ErrorAction Stop
        }
        $vsPath = & $vsModule { param($k) Fetch-VSTool $k } $Key
        if ($vsPath) { return $vsPath }
    }
    if ($Required) {
        $fallbackSummary = ($FallbackPaths | Where-Object { $_ }) -join ", "
        Raise-Error "Required tool not found: $Name. Searched PATH for $($CommandNames -join ', ') and fallback paths: $fallbackSummary"
    }
    return $null
}

function Resolve-ToolPaths {
    param([object[]]$ToolSpecs, [string]$PreferredPath)
    $resolvedTools = @{}
    foreach ($toolSpec in $ToolSpecs) {
        $resolvedTools[$toolSpec.Key] = Resolve-ToolPath `
            -Name $toolSpec.Name `
            -Key $toolSpec.Key `
            -CommandNames $toolSpec.Commands `
            -FallbackPaths $toolSpec.Fallbacks `
            -PreferredPath $PreferredPath `
            -OverridePath $toolSpec.OverridePath `
            -Required $toolSpec.Required `
            -VsDiscoverable $toolSpec.VsDiscoverable
    }
    $resolvedTools
}

function Get-MissingRequiredToolSpecs {
    param([object[]]$ToolSpecs, [hashtable]$ResolvedTools)
    @($ToolSpecs | Where-Object { $_.Required -and -not $ResolvedTools[$_.Key] })
}

function Assert-RequiredToolsResolved {
    param([object[]]$ToolSpecs, [hashtable]$ResolvedTools, [string]$Context)
    $missingTools = @(Get-MissingRequiredToolSpecs $ToolSpecs $ResolvedTools)
    if ($missingTools.Count -eq 0) { return }
    $message = @("Required tools were not found $Context.")
    foreach ($toolSpec in $missingTools) {
        $fallbackSummary = if ($toolSpec.Fallbacks.Count -gt 0) { $toolSpec.Fallbacks -join ", " } else { "<none>" }
        $overrideSummary = if ($toolSpec.OverridePath) { $toolSpec.OverridePath } else { "<none>" }
        $message += " - $($toolSpec.Name): commands=$($toolSpec.Commands -join ', '); override=$overrideSummary; fallbacks=$fallbackSummary"
    }
    Raise-Error ($message -join "`n")
}

function Get-RequiredToolPath {
    param([hashtable]$Tools, [string]$Key, [string]$Context)
    if (-not $Tools.ContainsKey($Key)) { Raise-Error "Tool '$Key' was not registered before $Context." }
    Require-Value $Tools[$Key] "Tool '$Key' was not resolved before $Context."
}

function Add-ToolDirectoriesToPath {
    param([string[]]$ToolPaths)
    $pathSeparator = [System.IO.Path]::PathSeparator
    $pathDirs = $env:PATH -split [regex]::Escape($pathSeparator) |
        Where-Object { $_ } | ForEach-Object { (Convert-ToForwardSlash $_.Trim()).TrimEnd('/') }
    foreach ($toolPath in $ToolPaths) {
        if (-not $toolPath) { continue }
        $toolDir = (Convert-ToForwardSlash (Split-Path -Path $toolPath -Parent)).TrimEnd('/')
        if ($toolDir -and -not ($pathDirs -contains $toolDir)) {
            $env:PATH += "$pathSeparator$toolDir"
            $pathDirs += $toolDir
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
        Key            = $Key
        Name           = $Name
        Commands       = $Commands
        Fallbacks      = @($Fallbacks | Where-Object { $_ })
        OverridePath   = $OverridePath
        Required       = $Required
        VsDiscoverable = $VsDiscoverable
    }
}

#---------------------------------------------------------------
# CMake cache
#---------------------------------------------------------------

function Read-CMakeCache {
    param([string]$CachePath)
    $entries = @{}
    if (-not (Test-Path -LiteralPath $CachePath -PathType Leaf)) { return $entries }
    foreach ($line in Get-Content -Path $CachePath) {
        if ($line -match '^([^#:=]+)(?::[^=]+)?=(.*)$') {
            $entries[$matches[1].Trim()] = $matches[2].Trim()
        }
    }
    $entries
}

function Normalize-ComparablePath {
    param([AllowNull()][string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return $null }
    $normalizedPath = (Convert-ToForwardSlash $Path.Trim('"')).TrimEnd('/')
    if (Test-Path -LiteralPath $normalizedPath -PathType Leaf) {
        return (Convert-ToForwardSlash (Resolve-Path -LiteralPath $normalizedPath).Path).TrimEnd('/')
    }
    $normalizedPath
}

function Test-CMakeCacheValueEquals {
    param([AllowNull()][string]$Actual, [AllowNull()][string]$Expected, [bool]$IsPath)
    if ($IsPath) { $Actual = Normalize-ComparablePath $Actual; $Expected = Normalize-ComparablePath $Expected }
    $comparison = [System.StringComparison]::Ordinal
    if ($IsPath -and [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform(
            [System.Runtime.InteropServices.OSPlatform]::Windows)) {
        $comparison = [System.StringComparison]::OrdinalIgnoreCase
    }
    [string]::Equals($Actual, $Expected, $comparison)
}

function Format-CMakeCacheValue {
    param([AllowNull()][string]$Value)
    if ($null -eq $Value) { return "<missing>" }
    if ($Value -eq "") { return "<empty>" }
    $Value
}

function Compare-CMakeCacheEntries {
    param([hashtable]$Cache, [object[]]$Requirements)
    @($Requirements | ForEach-Object {
        $actual = $Cache[$_.Name]
        if (-not (Test-CMakeCacheValueEquals $actual $_.Value $_.IsPath)) {
            [PSCustomObject]@{ Name=$_.Name; Actual=$actual; Expected=$_.Value }
        }
    })
}

function Clear-CMakeConfigureState {
    param([string]$BuildDir, [string]$Reason)
    if (-not (Test-Path -LiteralPath $BuildDir -PathType Container)) { return }
    Write-Host $Reason
    $cachePath = Join-PathForward $BuildDir "CMakeCache.txt"
    if (Test-Path -LiteralPath $cachePath -PathType Leaf) { Remove-Item -LiteralPath $cachePath -Force }
    $cmakeFilesDir = Join-PathForward $BuildDir "CMakeFiles"
    if (Test-Path -LiteralPath $cmakeFilesDir -PathType Container) {
        $resolvedBuildDir = (Resolve-Path -LiteralPath $BuildDir).Path.TrimEnd('\', '/')
        $resolvedCMakeFilesDir = (Resolve-Path -LiteralPath $cmakeFilesDir).Path
        $expectedPrefix = $resolvedBuildDir + [System.IO.Path]::DirectorySeparatorChar
        if (-not $resolvedCMakeFilesDir.StartsWith($expectedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            Raise-Error "Refusing to remove unexpected CMakeFiles directory: $resolvedCMakeFilesDir"
        }
        Remove-Item -LiteralPath $cmakeFilesDir -Recurse -Force
    }
}

function Reset-CMakeConfigureStateIfNeeded {
    param([string]$BuildDir, [object[]]$Requirements, [bool]$CleanConfigure, [bool]$CleanConfigureOnMismatch)
    if ($CleanConfigure) { Clear-CMakeConfigureState $BuildDir "Clearing CMake cache state because -CleanConfigure was requested."; return }
    if (-not $CleanConfigureOnMismatch) { return }
    $cachePath = Join-PathForward $BuildDir "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) { return }
    $cache = Read-CMakeCache $cachePath
    $mismatches = @(Compare-CMakeCacheEntries $cache $Requirements)
    if ($mismatches.Count -eq 0) { return }
    Write-Host "Existing CMake cache does not match the requested publish configuration."
    foreach ($mismatch in $mismatches) {
        Write-Host (" - {0}: cached {1}, requested {2}" -f $mismatch.Name, (Format-CMakeCacheValue $mismatch.Actual), (Format-CMakeCacheValue $mismatch.Expected))
    }
    Clear-CMakeConfigureState $BuildDir "Clearing CMake cache state before reconfigure."
}

function Assert-CMakeCacheMatches {
    param([string]$BuildDir, [object[]]$Requirements)
    $cachePath = Join-PathForward $BuildDir "CMakeCache.txt"
    $cache = Read-CMakeCache $cachePath
    $mismatches = @(Compare-CMakeCacheEntries $cache $Requirements)
    if ($mismatches.Count -eq 0) { return }
    foreach ($mismatch in $mismatches) {
        Write-Host ("Unexpected CMake cache value for {0}: configured {1}, requested {2}" -f $mismatch.Name, (Format-CMakeCacheValue $mismatch.Actual), (Format-CMakeCacheValue $mismatch.Expected)) -ForegroundColor Red
    }
    Raise-Error "CMake configuration did not produce the requested publish settings."
}

function New-CMakeDefine {
    param([string]$Name, [AllowNull()][string]$Value, [bool]$IsPath = $false, [bool]$Validate = $true)
    [PSCustomObject]@{ Name = $Name; Value = $Value; IsPath = $IsPath; Validate = $Validate }
}

#---------------------------------------------------------------
# Version / Git helpers
#---------------------------------------------------------------

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

function Get-FirstTextValue {
    param([AllowNull()][string[]]$Values)
    foreach ($value in $Values) {
        if ($value) { $trimmedValue = $value.Trim(); if ($trimmedValue) { return $trimmedValue } }
    }
    $null
}

function Get-RequiredFirstTextValue {
    param([AllowNull()][string[]]$Values, [string]$Context)
    Require-Value (Get-FirstTextValue $Values) "Missing required value for $Context."
}

#---------------------------------------------------------------
# Package target conversion
#---------------------------------------------------------------

function Convert-ToPackageToken {
    param([AllowNull()][string]$Value)
    if (-not $Value) { return "unknown" }
    $token = $Value.Trim() -replace '[^A-Za-z0-9]+', ''
    if (-not $token) { return "unknown" }
    $token.ToLowerInvariant()
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
    $compilerId = Get-FirstTextValue @($cache["CMAKE_CXX_COMPILER_ID"], $cxxCompilerInfo["CMAKE_CXX_COMPILER_ID"])
    $compilerPath = Get-FirstTextValue @($cache["CMAKE_CXX_COMPILER"], $cxxCompilerInfo["CMAKE_CXX_COMPILER"])
    $compilerName = if ($compilerPath) { Split-Path -Path $compilerPath -Leaf } else { $null }
    $toolchainSource = Get-RequiredFirstTextValue @($compilerId, $compilerName) "package compiler metadata from CMake cache/compiler files"
    [PSCustomObject]@{
        TargetSystem       = Convert-ToPackageTargetSystem $systemName
        Platform           = Convert-ToPackagePlatform $architecture
        Toolchain          = Convert-ToPackageToolchain $toolchainSource
        SourceSystemName   = $systemName
        SourceArchitecture = $architecture
        SourceCompilerId   = $compilerId
        SourceCompilerPath = $compilerPath
    }
}

function Read-CMakeSetFile {
    param([string]$PathPattern)
    $cmakeFiles = @(Resolve-Path -Path $PathPattern -ErrorAction SilentlyContinue)
    if ($cmakeFiles.Count -eq 0) { return @{} }
    $entries = @{}
    foreach ($line in Get-Content -LiteralPath $cmakeFiles[0].Path) {
        if ($line -match '^set\(([A-Za-z0-9_]+)\s+"?([^"\)]*)"?\)') {
            $entries[$matches[1]] = $matches[2]
        }
    }
    $entries
}

function Format-PackageNameSuffix {
    param([object]$BuildInfo)
    @($BuildInfo.TargetSystem, $BuildInfo.Platform, $BuildInfo.Toolchain) -join "-"
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

#---------------------------------------------------------------
# Platform detection (returns string)
#---------------------------------------------------------------

function Get-PublishPlatform {
    if ([System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Windows)) { return "Windows" }
    if ([System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Linux)) { return "Linux" }
    Raise-Error "Unsupported publish platform: $([System.Runtime.InteropServices.RuntimeInformation]::OSDescription)"
}

#---------------------------------------------------------------
# Project root resolution
#---------------------------------------------------------------

function Resolve-ProjectRoot {
    param([string]$ProvidedRoot)
    if ($ProvidedRoot) { return Convert-ToForwardSlash (Resolve-Path -LiteralPath $ProvidedRoot).Path }
    try {
        $gitRoot = & git rev-parse --show-toplevel 2>$null
        if ($gitRoot) { return Convert-ToForwardSlash $gitRoot.Trim() }
    } catch { }
    $fallbackRoot = Join-Path $PSScriptRoot "../.."
    return Convert-ToForwardSlash (Resolve-Path -LiteralPath $fallbackRoot).Path
}

#---------------------------------------------------------------
# Platform tool definitions loader
#---------------------------------------------------------------

function Load-PlatformTools {
    param([string]$ScriptRoot = $PSScriptRoot)
    $platformData = Get-Content -Path (Join-Path $ScriptRoot "PlatformTools.json") -Raw | ConvertFrom-Json
    foreach ($platform in $platformData.PSObject.Properties) {
        if ($platform.Name -eq "Description") { continue }
        foreach ($tool in $platform.Value.Tools) {
            $tool.Commands = @($tool.Commands | ForEach-Object { Expand-EnvironmentVariables $_ })
            $fallbackProperty = $tool.PSObject.Properties['Fallbacks']
            $fallbacks = if ($fallbackProperty) { @($fallbackProperty.Value) } else { @() }
            $tool | Add-Member -NotePropertyName Fallbacks -NotePropertyValue @(
                $fallbacks | ForEach-Object { Expand-EnvironmentVariables $_ }
            ) -Force
        }
    }
    $platformData
}

function Expand-EnvironmentVariables {
    param([string]$Value)
    if ($Value -match '\$\{env:([^}]+)\}') {
        $varName = $matches[1]
        $envVal = [System.Environment]::GetEnvironmentVariable($varName)
        if ($envVal) {
            $placeholder = '${env:' + $varName + '}'
            return $Value.Replace($placeholder, $envVal)
        }
    }
    return $Value
}

Export-ModuleMember -Function @(
    'Join-PathForward', 'Convert-ToForwardSlash', 'Resolve-BuildPath', 'Ensure-Directory',
    'Raise-Error', 'Require-Value',
    'Quote-CommandArgument', 'Format-CommandLine', 'Invoke-NativeCommand', 'Get-RequiredFirstOutputLine',
    'Get-ExistingToolPath', 'Find-CommandPath', 'Resolve-ToolPath', 'Resolve-ToolPaths',
    'Get-MissingRequiredToolSpecs', 'Assert-RequiredToolsResolved', 'Get-RequiredToolPath', 'Add-ToolDirectoriesToPath', 'New-ToolSpec',
    'Read-CMakeCache', 'Normalize-ComparablePath', 'Test-CMakeCacheValueEquals', 'Format-CMakeCacheValue',
    'Compare-CMakeCacheEntries', 'Clear-CMakeConfigureState', 'Reset-CMakeConfigureStateIfNeeded', 'Assert-CMakeCacheMatches', 'New-CMakeDefine',
    'Get-VersionValue', 'Get-GitOutput', 'Get-FirstTextValue', 'Get-RequiredFirstTextValue',
    'Convert-ToPackageToken', 'Convert-ToPackageTargetSystem', 'Convert-ToPackagePlatform', 'Convert-ToPackageToolchain',
    'Get-PackageBuildInfo', 'Read-CMakeSetFile', 'Format-PackageNameSuffix', 'Write-PackageBuildInfo', 'Update-PackageBuildInfo',
    'Get-PublishPlatform', 'Resolve-ProjectRoot', 'Load-PlatformTools', 'Expand-EnvironmentVariables'
)
