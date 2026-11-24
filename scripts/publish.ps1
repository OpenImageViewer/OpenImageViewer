#===============================================================
# PowerShell Build & Packaging Orchestrator for OIV
#===============================================================

[CmdletBinding()]
param (
    [string]$BuildType = "RelWithDebInfo",
    [string]$BuildDir = "publish",
    [ValidateNotNullOrEmpty()][string]$BuildTarget = "OIViewer",
    [bool]$EnableConfigure = $true,
    [bool]$EnableBuild = $true,
    [bool]$EnablePackage = $true,
    [bool]$OfficialBuild = $false,
    [bool]$OfficialRelease = $false,
    [bool]$CleanConfigureOnMismatch = $true,
    [switch]$CleanConfigure,
    [switch]$EchoCommands,
    [hashtable]$CMakeDefinitions = @{},
    [string]$BuildGeneratorPath,
    [string]$BuildToolPath,
    [string]$ArchiveToolPath,
    [string]$GitPath,
    [string]$CPPCompilerPath,
    [string]$RcPath,
    [string]$MtPath,
    [string]$Root
)

#---------------------------------------------------------------
# Build config hashtable
#---------------------------------------------------------------
$config = @{
    BuildType                 = $BuildType
    BuildDir                  = $BuildDir
    BuildTarget               = $BuildTarget
    EnableConfigure           = $EnableConfigure
    EnableBuild               = $EnableBuild
    EnablePackage             = $EnablePackage
    OfficialBuild             = $OfficialBuild
    OfficialRelease           = $OfficialRelease
    CleanConfigureOnMismatch  = $CleanConfigureOnMismatch
    CleanConfigure            = [bool]$CleanConfigure
    EchoCommands              = [bool]$EchoCommands
    CMakeDefinitions          = $CMakeDefinitions
    BuildGeneratorPath        = $BuildGeneratorPath
    BuildToolPath             = $BuildToolPath
    ArchiveToolPath           = $ArchiveToolPath
    GitPath                   = $GitPath
    CPPCompilerPath           = $CPPCompilerPath
    RcPath                    = $RcPath
    MtPath                    = $MtPath
    Root                      = $Root
}

#---------------------------------------------------------------
# Locate build scripts
#---------------------------------------------------------------
$buildScript = Join-Path $PSScriptRoot "build.ps1"
$packageScript = Join-Path $PSScriptRoot "package.ps1"

if (-not (Test-Path -LiteralPath $buildScript -PathType Leaf)) { throw "build.ps1 not found at $buildScript" }
if (-not (Test-Path -LiteralPath $packageScript -PathType Leaf)) { throw "package.ps1 not found at $packageScript" }

#---------------------------------------------------------------
# Run build stage
#---------------------------------------------------------------
if ($config.EnableConfigure -or $config.EnableBuild) {
    $buildArgs = @{ Config = $config; Root = $Root }
    & $buildScript @buildArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

#---------------------------------------------------------------
# Run package stage
#---------------------------------------------------------------
if ($config.EnablePackage) {
    $packageArgs = @{ Config = $config; Root = $Root }
    & $packageScript @packageArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "`nBuild & Packaging Complete!" -ForegroundColor Green
