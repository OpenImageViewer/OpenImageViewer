#===============================================================
# PowerShell Build Script for OIV
#===============================================================

[CmdletBinding()]
param(
    [hashtable]$Config = @{},
    [string]$Root
)

#---------------------------------------------------------------
# Import shared helpers
#---------------------------------------------------------------
Import-Module (Join-Path $PSScriptRoot "OIV.Build.Helpers/OIV.Build.Helpers.psd1") -Force -DisableNameChecking -ErrorAction Stop
Import-Module (Join-Path $PSScriptRoot "OIV.Build.Helpers/VSDiscovery.psm1") -Force -DisableNameChecking -ErrorAction Stop

#---------------------------------------------------------------
# Load platform tools
#---------------------------------------------------------------
$platformData = Load-PlatformTools $PSScriptRoot

#---------------------------------------------------------------
# Default configuration
#---------------------------------------------------------------
$defaultConfig = @{
    BuildType                 = 'RelWithDebInfo'
    BuildDir                  = 'publish'
    BuildTarget               = 'OIViewer'
    EnableConfigure           = $true
    EnableBuild               = $true
    EnablePackage             = $false
    OfficialBuild             = $false
    OfficialRelease           = $false
    CleanConfigureOnMismatch  = $true
    CleanConfigure            = $false
    EchoCommands              = $false
    VerboseOutput             = $false
    CMakeDefinitions          = @{}
    BuildGeneratorPath        = $null
    BuildToolPath             = $null
    ArchiveToolPath           = $null
    GitPath                   = $null
    CPPCompilerPath           = $null
    RcPath                    = $null
    MtPath                    = $null
    Root                      = $null
}

$configKeys = @($defaultConfig.Keys)
foreach ($key in $configKeys) {
    if ($Config.ContainsKey($key)) { $defaultConfig[$key] = $Config[$key] }
}
if ($PSBoundParameters.ContainsKey('Root')) { $defaultConfig.Root = $Root }
$options = [pscustomobject]$defaultConfig
#---------------------------------------------------------------
# Strict mode
#---------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

#---------------------------------------------------------------
# Platform enum (defined locally since script modules don't export enums)
#---------------------------------------------------------------

enum PublishPlatform {
    Windows
    Linux
}

#---------------------------------------------------------------
# Windows-specific helpers
#---------------------------------------------------------------
function Get-WindowsSdkToolPaths {
    param([string]$ToolName)
    if (-not [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform(
            [System.Runtime.InteropServices.OSPlatform]::Windows)) { return @() }
    $sdkRoots = @("${env:ProgramFiles(x86)}/Windows Kits/10/bin", "${env:ProgramFiles(x86)}/Windows Kits/8.1/bin")
    $paths = @()
    foreach ($sdkRoot in $sdkRoots) {
        if (-not (Test-Path -LiteralPath $sdkRoot -PathType Container)) { continue }
        Get-ChildItem -LiteralPath $sdkRoot -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending | ForEach-Object { $paths += Join-PathForward $_.FullName "x64/$ToolName" }
        $paths += Join-PathForward $sdkRoot "x64/$ToolName"
    }
    $paths
}

function Test-ToolchainNeedsVsEnvironment {
    <#
    .SYNOPSIS
    Returns $true when the selected compiler or tools require the Visual Studio environment.
    #>
    param([hashtable]$ResolvedTools)

    $cppCompiler = $ResolvedTools["CPPCompiler"]
    if ($cppCompiler -and (Split-Path -Path $cppCompiler -Leaf) -eq 'cl.exe') { return $true }

    foreach ($key in @("CPPCompiler", "RC", "MT")) {
        $path = $ResolvedTools[$key]
        if ($path -and $path -match '[/\\](Microsoft Visual Studio|VC[/\\]Tools)[/\\]') {
            return $true
        }
    }
    return $false
}

# (VS discovery moved to VSDiscovery module)
#---------------------------------------------------------------
# Platform profile
#---------------------------------------------------------------
function New-PlatformProfile {
    param([object]$Options, [string]$Platform)
    $platformName = $Platform
    $windowsPlatform = $platformName -eq "Windows"
    $needsWindowsBuildTools = $windowsPlatform -and ($Options.EnableConfigure -or $Options.EnableBuild)
    $platformDataEntry = $platformData.$platformName

    $overrideMap = @{
        BuildGenerator = $Options.BuildGeneratorPath; BuildTool = $Options.BuildToolPath
        Git = $Options.GitPath; ArchiveTool = $Options.ArchiveToolPath
        CPPCompiler = $Options.CPPCompilerPath; RC = $Options.RcPath; MT = $Options.MtPath
    }
    $requiredMap = @{
        BuildGenerator = $Options.EnableConfigure -or $Options.EnableBuild
        BuildTool      = $Options.EnableConfigure
        Git            = $true
        ArchiveTool    = if ($windowsPlatform) { $Options.EnablePackage } else { $true }
        CPPCompiler    = $needsWindowsBuildTools
        RC             = $needsWindowsBuildTools
        MT             = $needsWindowsBuildTools
    }

    $toolSpecs = @($platformDataEntry.Tools | ForEach-Object {
        $tool = $_
        $fallbacks = @($tool.Fallbacks)
        if ($windowsPlatform -and $tool.Key -eq "RC") { $fallbacks += Get-WindowsSdkToolPaths "rc.exe" }
        if ($windowsPlatform -and $tool.Key -eq "MT") { $fallbacks += Get-WindowsSdkToolPaths "mt.exe" }
        New-ToolSpec -Key $tool.Key -Name $tool.Name -Commands $tool.Commands -Fallbacks $fallbacks `
            -OverridePath $overrideMap[$tool.Key] -Required $requiredMap[$tool.Key] -VsDiscoverable $tool.VsDiscoverable
    })

    if ($windowsPlatform) {
        [pscustomobject]@{
            Platform = [PublishPlatform]::Windows
            ToolSpecs = $toolSpecs
            ConfigureToolDefines = @(
                [pscustomobject]@{ Name="CMAKE_MT";           ToolKey="MT"    }
                [pscustomobject]@{ Name="CMAKE_C_COMPILER";   ToolKey="CPPCompiler" }
                [pscustomobject]@{ Name="CMAKE_CXX_COMPILER"; ToolKey="CPPCompiler" }
                [pscustomobject]@{ Name="CMAKE_RC_COMPILER";  ToolKey="RC"    }
            )
            EnvironmentTools = @(
                [pscustomobject]@{ Name="CC";  ToolKey="CPPCompiler" }
                [pscustomobject]@{ Name="CXX"; ToolKey="CPPCompiler" }
            )
            PackageSpec = $platformDataEntry.PackageSpec
        }
    } else {
        [pscustomobject]@{
            Platform = [PublishPlatform]::Linux
            ToolSpecs = $toolSpecs
            ConfigureToolDefines = @()
            EnvironmentTools = @()
            PackageSpec = $platformDataEntry.PackageSpec
        }
    }
}

function Resolve-BuildTools {
    param([object]$PlatformProfile, [string]$InitialPath, [bool]$EchoCommands)
    $toolSpecs = $PlatformProfile.ToolSpecs
    $resolvedTools = Resolve-ToolPaths $toolSpecs $InitialPath
    $requiredVsTools = @($toolSpecs | Where-Object { $_.Required -and $_.VsDiscoverable })
    $missingVsDiscoverableTools = @($requiredVsTools | Where-Object { -not $resolvedTools[$_.Key] })
    $needsVsEnvironment = Test-ToolchainNeedsVsEnvironment $resolvedTools
    $shouldImportVsEnvironment = $PlatformProfile.Platform -eq [PublishPlatform]::Windows -and (
        $missingVsDiscoverableTools.Count -gt 0 -or
        ($requiredVsTools.Count -gt 0 -and $needsVsEnvironment)
    )
    if ($shouldImportVsEnvironment) {
        Import-VSEnvironment
        $resolvedTools = Resolve-ToolPaths $toolSpecs $InitialPath
        $needsVsEnvironment = Test-ToolchainNeedsVsEnvironment $resolvedTools
        if ($needsVsEnvironment) {
            foreach ($key in @("RC", "MT")) {
                $toolSpec = $toolSpecs | Where-Object { $_.Key -eq $key -and $_.Required -and -not $_.OverridePath } | Select-Object -First 1
                if (-not $toolSpec) { continue }
                $vsToolPath = Fetch-VSTool $key
                if ($vsToolPath) { $resolvedTools[$key] = $vsToolPath }
            }
        }
    }
    Assert-RequiredToolsResolved $toolSpecs $resolvedTools "after PATH and fallback discovery"
    Add-ToolDirectoriesToPath @($resolvedTools.Values)
    [pscustomobject]@{
        Paths   = $resolvedTools
        Display = $toolSpecs | ForEach-Object { [pscustomobject]@{ Name = $_.Name; Path = $resolvedTools[$_.Key] } }
    }
}

#---------------------------------------------------------------
# CMake configure spec
#---------------------------------------------------------------
function New-CMakeConfigureSpec {
    param([object]$Context)
    $platformDefines = @($Context.Profile.ConfigureToolDefines | ForEach-Object {
        New-CMakeDefine -Name $_.Name -Value $Context.Tools[$_.ToolKey] -IsPath $true
    })
    $customDefines = @($Context.CMakeDefinitions.GetEnumerator() | ForEach-Object {
        New-CMakeDefine -Name $_.Key -Value "$($_.Value)"
    })
    $defines = @(
        New-CMakeDefine -Name "CMAKE_BUILD_TYPE"                -Value $Context.BuildType
        New-CMakeDefine -Name "CMAKE_MAKE_PROGRAM"              -Value $Context.Tools["BuildTool"]              -IsPath $true
        New-CMakeDefine -Name "OIV_OFFICIAL_BUILD"              -Value "$($Context.OfficialBuild)"
        New-CMakeDefine -Name "OIV_OFFICIAL_RELEASE"            -Value "$($Context.OfficialRelease)"
        New-CMakeDefine -Name "OIV_VERSION_REVISION"            -Value "$($Context.VersionRevision)"
        New-CMakeDefine -Name "OIV_RELEASE_SUFFIX"              -Value "$($Context.ReleaseSuffix)"
    ) + $platformDefines + $customDefines

    $configureArgs = @("-S", $Context.RootDir, "-B", $Context.BuildDir, "-G", $Context.Generator) +
        @($defines | ForEach-Object { "-D$($_.Name)=$($_.Value)" })

    $cacheRequirements = @(
        [pscustomobject]@{ Name="CMAKE_GENERATOR"; Value=$Context.Generator; IsPath=$false }
    ) + @($defines | Where-Object { $_.Validate })

    [pscustomobject]@{ Defines = $defines; ConfigureArgs = $configureArgs; CacheRequirements = $cacheRequirements }
}

#---------------------------------------------------------------
# Publish context
#---------------------------------------------------------------
function New-PublishContext {
    param([object]$Options, [object]$ToolInfo, [object]$PlatformProfile)
    $rootDir = Resolve-ProjectRoot $Options.Root
    $resolvedBuildDir = Resolve-BuildPath $rootDir $Options.BuildDir
    $binDir = Join-PathForward $resolvedBuildDir "bin"
    $versionHeader = Join-PathForward $rootDir "OIVAppCore/Source/Include/Version.h"
    $gitDir = Join-PathForward $rootDir ".git"

    $context = [pscustomobject]@{
        RootDir                  = $rootDir
        BuildDir                 = $resolvedBuildDir
        BinDir                   = $binDir
        VersionHeader            = $versionHeader
        GitDir                   = $gitDir
        BuildType                = $Options.BuildType
        BuildTarget              = $Options.BuildTarget
        EnableConfigure          = $Options.EnableConfigure
        EnableBuild              = $Options.EnableBuild
        EnablePackage            = $Options.EnablePackage
        CleanConfigure           = $Options.CleanConfigure
        CleanConfigureOnMismatch = $Options.CleanConfigureOnMismatch
        EchoCommands             = $Options.EchoCommands
        VerboseOutput            = $Options.VerboseOutput
        CMakeDefinitions         = $Options.CMakeDefinitions
        Generator                = "Ninja"
        Profile                  = $PlatformProfile
        Tools                    = $ToolInfo.Paths
        ToolDisplay              = $ToolInfo.Display
        OfficialBuild            = if ($Options.OfficialBuild) { 1 } else { 0 }
        OfficialRelease          = if ($Options.OfficialRelease) { 1 } else { 0 }
        ReleaseSuffix            = ""
        VersionRevision          = $null
        ReleaseVersion           = $null
        VersionNumber            = $null
        PackageVersion           = $null
        DateShort                = $null
        DateLong                 = $null
        PackageBuildInfo         = $null
        PackageNameSuffix        = $null
    }

    $fileContent = Get-Content -Path $context.VersionHeader
    $major = Get-VersionValue $fileContent "OIV_VERSION_MAJOR"
    $minor = Get-VersionValue $fileContent "OIV_VERSION_MINOR"
    $patch = Get-VersionValue $fileContent "OIV_VERSION_PATCH"
    $gitDirArg = "--git-dir=$($context.GitDir)"
    $revision = Get-GitOutput $context @($gitDirArg, "rev-list", "HEAD", "--count") "Failed to read Git revision count."
    $now = Get-Date

    $context.VersionRevision = $revision
    $context.ReleaseVersion = "$major.$minor.$patch"
    $context.VersionNumber = "$($context.ReleaseVersion).$revision"
    $context.PackageVersion = if ($context.OfficialRelease -eq 0) {
        $shortHash = Get-GitOutput $context @($gitDirArg, "rev-parse", "--short=8", "HEAD") "Failed to read Git short hash."
        "$($context.VersionNumber)-$shortHash"
    } else { $context.ReleaseVersion }
    $context.DateShort = $now.ToString("yyyy-MM-dd")
    $context.DateLong = $now.ToString("yyyy-MM-dd_HH-mm-ss")
    $context
}

#---------------------------------------------------------------
# Build operations
#---------------------------------------------------------------
function Write-BuildInfo {
    param([object]$Context)
    Write-Host "`nBuild Tools Info"
    Write-Host "-----------------------------------------------"
    foreach ($tool in $Context.ToolDisplay | Where-Object { $_.Path }) {
        Write-Host ("{0,-20}: {1}" -f $tool.Name, $tool.Path)
    }
    Write-Host "`nBuild Info"
    Write-Host "-----------------------------------------------"
    Write-Host ("Platform      : {0}" -f $Context.Profile.Platform)
    Write-Host ("Root Dir      : {0}" -f $Context.RootDir)
    Write-Host ("Build Dir     : {0}" -f $Context.BuildDir)
    Write-Host ("Bin Dir       : {0}" -f $Context.BinDir)
    Write-Host ("Official Build: {0}" -f [bool]$Context.OfficialBuild)
    Write-Host ("Official Rel. : {0}" -f [bool]$Context.OfficialRelease)
    Write-Host ("Release Ver.  : {0}" -f $Context.ReleaseVersion)
    Write-Host ("Version       : {0}" -f $Context.VersionNumber)
    Write-Host ("Package Ver.  : {0}" -f $Context.PackageVersion)
    Write-Host ("Date          : {0}" -f $Context.DateShort)
    Write-Host ("Time          : {0}" -f $Context.DateLong)
    Write-Host "==============================================="
}

function Invoke-CMakeConfigure {
    param([object]$Context, [object]$ConfigureSpec)
    Write-Host "Running CMake configure..."
    Reset-CMakeConfigureStateIfNeeded `
        -BuildDir $Context.BuildDir `
        -Requirements $ConfigureSpec.CacheRequirements `
        -CleanConfigure $Context.CleanConfigure `
        -CleanConfigureOnMismatch $Context.CleanConfigureOnMismatch

    $commandArgs = @{
        FilePath     = Get-RequiredToolPath $Context.Tools "BuildGenerator" "CMake configure"
        Arguments    = $ConfigureSpec.ConfigureArgs
        ErrorMessage = "CMake configuration failed."
        EchoCommands = $Context.EchoCommands
        PassThru     = $Context.VerboseOutput
    }
    $output = Invoke-NativeCommand @commandArgs
    if ($Context.VerboseOutput) { $output | ForEach-Object { Write-Host $_ } }
    Assert-CMakeCacheMatches $Context.BuildDir $ConfigureSpec.CacheRequirements
}

function Invoke-CMakeBuild {
    param([object]$Context)
    Write-Host "Building $($Context.BuildTarget)..."
    $buildArgs = @("--build", $Context.BuildDir, "--config", $Context.BuildType, "--target", $Context.BuildTarget)
    $commandArgs = @{
        FilePath     = Get-RequiredToolPath $Context.Tools "BuildGenerator" "CMake build"
        Arguments    = $buildArgs
        ErrorMessage = "Build failed."
        EchoCommands = $Context.EchoCommands
        PassThru     = $Context.VerboseOutput
    }
    $output = Invoke-NativeCommand @commandArgs
    if ($Context.VerboseOutput) { $output | ForEach-Object { Write-Host $_ } }
}

function Set-BuildToolEnvironment {
    param([object]$Context)
    foreach ($entry in $Context.Profile.EnvironmentTools) {
        $toolPath = Get-RequiredToolPath $Context.Tools $entry.ToolKey "setting $($entry.Name)"
        [Environment]::SetEnvironmentVariable($entry.Name, $toolPath, 'Process')
    }
}

#---------------------------------------------------------------
# Build manifest
#---------------------------------------------------------------
function Write-BuildManifest {
    param([object]$Context, [object]$PlatformProfile)
    $manifestDir = Join-PathForward $Context.BuildDir "manifest"
    Ensure-Directory $manifestDir
    $manifestPath = Join-PathForward $manifestDir "build-manifest.json"

    $versionParts = $Context.VersionNumber -split '\.'
    $manifest = [ordered]@{
        Version = [ordered]@{
            Major          = if ($versionParts.Count -ge 1) { [int]$versionParts[0] } else { $null }
            Minor          = if ($versionParts.Count -ge 2) { [int]$versionParts[1] } else { $null }
            Patch          = if ($versionParts.Count -ge 3) { [int]$versionParts[2] } else { $null }
            Revision       = $Context.VersionRevision
            VersionNumber  = $Context.VersionNumber
            PackageVersion = $Context.PackageVersion
        }
        Dates = [ordered]@{ Short = $Context.DateShort; Long = $Context.DateLong }
        Build = [ordered]@{
            Type       = $Context.BuildType
            Target     = $Context.BuildTarget
            Dir        = $Context.BuildDir
            BinDir     = $Context.BinDir
        }
        Platform = [ordered]@{
            TargetSystem       = $Context.PackageBuildInfo.TargetSystem
            Architecture       = $Context.PackageBuildInfo.Platform
            Toolchain          = $Context.PackageBuildInfo.Toolchain
            SourceSystemName   = $Context.PackageBuildInfo.SourceSystemName
            SourceArchitecture = $Context.PackageBuildInfo.SourceArchitecture
            SourceCompilerId   = $Context.PackageBuildInfo.SourceCompilerId
            SourceCompilerPath = $Context.PackageBuildInfo.SourceCompilerPath
        }
        Package = [ordered]@{
            NameSuffix       = $Context.PackageNameSuffix
            ArchiveExtension = $PlatformProfile.PackageSpec.ArchiveExtension
            ArchiveArgs      = $PlatformProfile.PackageSpec.ArchiveArgs
            ArchiveToolKey   = $PlatformProfile.PackageSpec.ArchiveToolKey
        }
        Tools = $Context.Tools
        OfficialBuild   = [bool]$Context.OfficialBuild
        OfficialRelease = [bool]$Context.OfficialRelease
    }

    $json = $manifest | ConvertTo-Json -Depth 10
    Set-Content -Path $manifestPath -Value $json -Encoding UTF8
    Write-Host "Build manifest written to: $manifestPath"
    $manifestPath
}

#---------------------------------------------------------------
# Main build
#---------------------------------------------------------------
function Run-OIVBuild {
    $initialPath = $env:PATH
    $platform = Get-PublishPlatform
    $platformProfile = New-PlatformProfile $options $platform
    $toolInfo = Resolve-BuildTools $platformProfile $initialPath $options.EchoCommands
    $context = New-PublishContext $options $toolInfo $platformProfile

    if ($context.EnableConfigure -or $context.EnableBuild) { Set-BuildToolEnvironment $context }
    Write-BuildInfo $context

    if ($context.EnableConfigure) {
        Invoke-CMakeConfigure $context (New-CMakeConfigureSpec $context)
    }

    if ($context.EnableConfigure -or $context.EnableBuild -or $context.EnablePackage) {
        Update-PackageBuildInfo $context
        Write-PackageBuildInfo $context.PackageBuildInfo
    }

    if ($context.EnableBuild) { Invoke-CMakeBuild $context }

    $manifestPath = if ($context.EnableConfigure -or $context.EnableBuild) {
        Write-BuildManifest $context $platformProfile
    } else {
        $path = Join-PathForward $context.BuildDir "manifest/build-manifest.json"
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { Raise-Error "No build manifest found at $path. Run build.ps1 first." }
        $path
    }

    Write-Host "`nBuild Complete!" -ForegroundColor Green
    $manifestPath
}

#---------------------------------------------------------------
# Dot-source guard
#---------------------------------------------------------------
if ($MyInvocation.InvocationName -eq ".") { return }

#---------------------------------------------------------------
# Entry point
#---------------------------------------------------------------
$notifyFailed = $true
try {
    $manifestPath = Run-OIVBuild
    $notifyFailed = $false
    Write-Host "Build manifest: $manifestPath"
} catch {
    Write-Host "`nBuild failed." -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    if ($_.InvocationInfo) { Write-Host "`nLocation:"; Write-Host $_.InvocationInfo.PositionMessage }
    if ($_.ScriptStackTrace) { Write-Host "`nScript stack trace:"; Write-Host $_.ScriptStackTrace }
    Write-Host "`nException:"; Write-Host $_.Exception.ToString()
    $notifyFailed = $false
    exit 1
} finally {
    if ($notifyFailed) { Write-Host "`nBuild failed or cancelled by the user" -ForegroundColor Red }
}
