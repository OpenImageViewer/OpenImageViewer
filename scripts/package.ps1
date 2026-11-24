#===============================================================
# PowerShell Package Script for OIV
#===============================================================

[CmdletBinding()]
param(
    [hashtable]$Config = @{},
    [string]$ManifestPath,
    [string]$Root
)

#---------------------------------------------------------------
# Import shared helpers
#---------------------------------------------------------------
Import-Module (Join-Path $PSScriptRoot "OIV.Build.Helpers/OIV.Build.Helpers.psd1") -Force -DisableNameChecking -ErrorAction Stop

#---------------------------------------------------------------
# Load platform tools
#---------------------------------------------------------------
$platformData = Load-PlatformTools $PSScriptRoot

#---------------------------------------------------------------
# Default configuration
#---------------------------------------------------------------
$defaultConfig = @{
    BuildDir        = 'publish'
    EchoCommands    = $false
    ArchiveToolPath = $null
    GitPath         = $null
}
$configKeys = @($defaultConfig.Keys)
foreach ($key in $configKeys) { if ($Config.ContainsKey($key)) { $defaultConfig[$key] = $Config[$key] } }
$options = [pscustomobject]$defaultConfig

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
# Resolve project root and manifest
#---------------------------------------------------------------
$rootDir = Resolve-ProjectRoot $Root

if (-not $ManifestPath) {
    $ManifestPath = Join-PathForward (Resolve-BuildPath $rootDir $options.BuildDir) "manifest/build-manifest.json"
}
if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    Raise-Error "Build manifest not found: $ManifestPath. Run build.ps1 first."
}

$manifest = Get-Content -Path $ManifestPath -Raw | ConvertFrom-Json

#---------------------------------------------------------------
# Reconstruct publish context from manifest
#---------------------------------------------------------------
$platformName = $manifest.Platform.TargetSystem
$windowsPlatform = $platformName -eq "Win32"
$publishPlatform = if ($windowsPlatform) { [PublishPlatform]::Windows } else { [PublishPlatform]::Linux }
$platformJsonKey = if ($windowsPlatform) { "Windows" } else { "Linux" }

$buildDir = Resolve-BuildPath $rootDir $options.BuildDir
$binDir = Join-PathForward $buildDir "bin"
$platformDataEntry = $platformData.$platformJsonKey
$packageSpec = $platformDataEntry.PackageSpec

#---------------------------------------------------------------
# Tool discovery (package only needs ArchiveTool, plus Git for metadata if needed)
#---------------------------------------------------------------
function Resolve-PackageTools {
    param([string]$ArchiveToolPath, [string]$GitPath, [bool]$RequiredArchiveTool)
    $archiveToolDef = $platformDataEntry.Tools | Where-Object { $_.Key -eq "ArchiveTool" } | Select-Object -First 1
    $archiveSpec = New-ToolSpec -Key "ArchiveTool" -Name $archiveToolDef.Name -Commands $archiveToolDef.Commands `
        -Fallbacks $archiveToolDef.Fallbacks -OverridePath $ArchiveToolPath -Required $RequiredArchiveTool
    $toolSpecs = @($archiveSpec)
    if ($GitPath) {
        $gitToolDef = $platformDataEntry.Tools | Where-Object { $_.Key -eq "Git" } | Select-Object -First 1
        $gitSpec = New-ToolSpec -Key "Git" -Name $gitToolDef.Name -Commands $gitToolDef.Commands `
            -Fallbacks $gitToolDef.Fallbacks -OverridePath $GitPath -Required $true
        $toolSpecs += $gitSpec
    }
    $resolvedTools = Resolve-ToolPaths $toolSpecs $null
    if ($RequiredArchiveTool -and -not $resolvedTools["ArchiveTool"]) {
        Raise-Error "Archive tool is required for packaging but was not found."
    }
    Add-ToolDirectoriesToPath @($resolvedTools.Values)
    $resolvedTools
}

$initialPath = $env:PATH
$resolvedTools = Resolve-PackageTools $options.ArchiveToolPath $options.GitPath $true

#---------------------------------------------------------------
# Build context for packaging
#---------------------------------------------------------------
$context = [pscustomobject]@{
    RootDir           = $rootDir
    BuildDir          = $buildDir
    BinDir            = $binDir
    BuildType         = $manifest.Build.Type
    BuildTarget       = $manifest.Build.Target
    EnablePackage     = $true
    EchoCommands      = $options.EchoCommands
    Profile           = [pscustomobject]@{ Platform = $publishPlatform; PackageSpec = $packageSpec }
    Tools             = $resolvedTools
    PackageBuildInfo  = [pscustomobject]@{
        TargetSystem       = $manifest.Platform.TargetSystem
        Platform           = $manifest.Platform.Architecture
        Toolchain          = $manifest.Platform.Toolchain
        SourceSystemName   = $manifest.Platform.SourceSystemName
        SourceArchitecture = $manifest.Platform.SourceArchitecture
        SourceCompilerId   = $manifest.Platform.SourceCompilerId
        SourceCompilerPath = $manifest.Platform.SourceCompilerPath
    }
    PackageNameSuffix = $manifest.Package.NameSuffix
    PackageVersion    = $manifest.Version.PackageVersion
    VersionNumber     = $manifest.Version.VersionNumber
    DateShort         = $manifest.Dates.Short
    DateLong          = $manifest.Dates.Long
}

#---------------------------------------------------------------
# Package helpers
#---------------------------------------------------------------
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
    param([object]$Context, [string]$OutputPath, [string[]]$Inputs)
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

#---------------------------------------------------------------
# Package
#---------------------------------------------------------------
function Invoke-Package {
    param([object]$Context)
    $packageSpec = $Context.Profile.PackageSpec
    if ($null -eq $packageSpec) { Raise-Error "Packaging is not implemented for the $($Context.Profile.Platform) platform." }

    Write-Host "Packaging..."
    Write-Host "`nPackage Info"
    Write-Host "-----------------------------------------------"
    Write-Host ("Platform      : {0}" -f $Context.Profile.Platform)
    Write-Host ("Root Dir      : {0}" -f $Context.RootDir)
    Write-Host ("Build Dir     : {0}" -f $Context.BuildDir)
    Write-Host ("Bin Dir       : {0}" -f $Context.BinDir)
    Write-Host ("Version       : {0}" -f $Context.VersionNumber)
    Write-Host ("Package Ver.  : {0}" -f $Context.PackageVersion)
    Write-Host ("Target System : {0}" -f $Context.PackageBuildInfo.TargetSystem)
    Write-Host ("Architecture  : {0}" -f $Context.PackageBuildInfo.Platform)
    Write-Host ("Tool Set      : {0}" -f $Context.PackageBuildInfo.Toolchain)
    Write-Host ("Date          : {0}" -f $Context.DateShort)
    Write-Host ("Time          : {0}" -f $Context.DateLong)
    Write-Host ("Name Suffix   : {0}" -f $Context.PackageNameSuffix)
    Write-Host "==============================================="

    $outputDir = Join-PathForward $Context.BuildDir "$($Context.DateLong)-v$($Context.VersionNumber)"
    $baseName = Join-PathForward $outputDir "OIV-$($Context.PackageVersion)-$($Context.PackageNameSuffix)"
    Ensure-Directory $outputDir

    $runtimePatterns = @($packageSpec.RuntimePatterns)
    $requiredFiles = @($packageSpec.RequiredFiles)
    $cache = Read-CMakeCache (Join-PathForward $Context.BuildDir "CMakeCache.txt")
    if ($cache["OIV_BUILD_RENDERER_VK"] -match '^(1|ON|TRUE|YES|Y)$') {
        $runtimePatterns += "ShaderCache"
        $requiredFiles += @("ShaderCache/QuadVP.vert.spv", "ShaderCache/QuadFP.frag.spv",
            "ShaderCache/QuadSelectionFP.frag.spv", "ShaderCache/QuadSimpleFP.frag.spv")
    }

    $runtimePackageInputs = @($runtimePatterns | ForEach-Object { Join-PathForward $Context.BinDir $_ })
    $symbolsPackageInputs  = @($packageSpec.SymbolPatterns  | ForEach-Object { Join-PathForward $Context.BinDir $_ })
    $requiredRuntimeInputs = @($requiredFiles | ForEach-Object { Join-PathForward $Context.BinDir $_ })

    Assert-PackageInputs $requiredRuntimeInputs "runtime"
    Assert-PackageInputs $runtimePackageInputs "runtime"
    if ($symbolsPackageInputs.Count -gt 0) {
        Assert-PackageInputs $symbolsPackageInputs "symbols"
        New-Archive $Context "$baseName-Symbols$($packageSpec.ArchiveExtension)" $symbolsPackageInputs
    }
    New-Archive $Context "$baseName$($packageSpec.ArchiveExtension)" $runtimePackageInputs
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
    Invoke-Package $context
    $notifyFailed = $false
    Write-Host "`nPackage Complete!" -ForegroundColor Green
} catch {
    Write-Host "`nPackage failed." -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    if ($_.InvocationInfo) { Write-Host "`nLocation:"; Write-Host $_.InvocationInfo.PositionMessage }
    if ($_.ScriptStackTrace) { Write-Host "`nScript stack trace:"; Write-Host $_.ScriptStackTrace }
    Write-Host "`nException:"; Write-Host $_.Exception.ToString()
    $notifyFailed = $false
    exit 1
} finally {
    if ($notifyFailed) { Write-Host "`nPackage failed or cancelled by the user" -ForegroundColor Red }
}
