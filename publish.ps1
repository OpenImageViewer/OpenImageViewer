#===============================================================
# PowerShell Build & Packaging Script for OIV
#===============================================================

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

#---------------------------------------------------------------
# Helper Functions
#---------------------------------------------------------------

function Join-PathForward {
    [CmdletBinding(DefaultParameterSetName = 'PathChild')]
    param (
        [Parameter(Position = 0, Mandatory)][string]$Path,
        [Parameter(Position = 1, ParameterSetName = 'PathChild')][string]$ChildPath,
        [Parameter(Position = 1, ParameterSetName = 'AdditionalChildren')][string[]]$AdditionalChildPaths,
        [Parameter(ParameterSetName = 'Resolve')][switch]$Resolve,
        [Parameter(ParameterSetName = 'Relative')][string]$Relative
    )
    process {
        $result = switch ($PSCmdlet.ParameterSetName) {
            'PathChild'          { Join-Path -Path $Path -ChildPath $ChildPath }
            'AdditionalChildren' { Join-Path -Path $Path -AdditionalChildPaths $AdditionalChildPaths }
            'Resolve'            { Join-Path -Path $Path -Resolve }
            'Relative'           { Join-Path -Path $Path -Relative $Relative }
        }
        $result -replace '\\', '/'
    }
}

function Raise-Error {
    param([string]$Message)
    Write-Host "`nERROR: $Message" -ForegroundColor Red
    throw $Message
}

function Ensure-Directory {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Extract-FirstNumber {
    param([string]$Line)
    if ($Line -match "\d+") { return [int]$matches[0] }
    Raise-Error "Cannot extract number from: $Line"
}

function Get-VersionValue {
    param([string[]]$Content, [string]$Token)
    $line = $Content | Where-Object { $_ -match "$Token\s*=" }
    if ($line) { Extract-FirstNumber $line }
    else { Raise-Error "Missing version token: $Token" }
}

function Get-GitRevisionCount { (& git --git-dir=$GitDir rev-list HEAD --count).Trim() }
function Get-GitShortHash     { (& git --git-dir=$GitDir rev-parse --short HEAD).Trim() }

#---------------------------------------------------------------
# Main Build Function
#---------------------------------------------------------------

function Run-OIVBuild {
    # Settings
    $EnableCMake   = $true
    $EnableBuild   = $true
    $EnablePackage = $true

    $RootDir          = $PSScriptRoot -replace '\\', '/'
    $SevenZipPath     = "C:/Program Files/7-Zip/7z.exe"
    $GitPath          = "C:/Program Files/Git/bin"
    $DependenciesPath = "$RootDir/oiv/Dependencies"
    $VersionHeader    = Join-PathForward $RootDir "oivlib/oiv/Include/Version.h"
    $GitDir           = Join-PathForward $RootDir ".git"
    $BuildDir         = Join-PathForward $RootDir "publish"
    $BinDir           = Join-PathForward $BuildDir "bin"

    $BuildType            = "RelWithDebInfo"
    $OIV_OFFICIAL_BUILD   = 1
    $OIV_OFFICIAL_RELEASE = 1
    $OIV_RELEASE_SUFFIX   = ""
    $OIV_VERSION_BUILD    = 0

    # Tool Discovery
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    if (-not (Test-Path $vswhere)) { Raise-Error "vswhere.exe not found at $vswhere" }

    $vsDir = (& $vswhere -latest -requires Microsoft.Component.MSBuild -property installationPath).Trim() -replace '\\', '/'
    if (-not $vsDir) { Raise-Error "Visual Studio installation not found." }

    $CMakePath = (& $vswhere -products * -latest -find **\cmake.exe).Trim() -replace '\\', '/'
    $NinjaPath = (& $vswhere -products * -latest -find **\ninja.exe).Trim() -replace '\\', '/'

    $LLVMDir = "$vsDir/VC/Tools/Llvm/x64/bin"
    $Clang   = "$LLVMDir/clang-cl.exe"
    $RC      = "$LLVMDir/llvm-rc.exe"
    $MT      = "$LLVMDir/llvm-mt.exe"

    $Tools = @(
        [PSCustomObject]@{ Name="CMake";     Path=$CMakePath },
        [PSCustomObject]@{ Name="Ninja";     Path=$NinjaPath },
        [PSCustomObject]@{ Name="7-Zip";     Path=$SevenZipPath },
        [PSCustomObject]@{ Name="clang-cl";  Path=$Clang },
        [PSCustomObject]@{ Name="llvm-rc";   Path=$RC },
        [PSCustomObject]@{ Name="llvm-mt";   Path=$MT },
        [PSCustomObject]@{ Name="Git (PATH)";Path=$GitPath }
    )

    foreach ($tool in $Tools) {
        if (-not (Test-Path $tool.Path)) {
            Raise-Error "Required tool not found: $($tool.Name) at $($tool.Path)"
        }
    }

    $env:Path += ";$GitPath;$SevenZipPath;$CMakePath;$NinjaPath"

    Write-Host "`nFound Tools"
    $Tools | Format-Table Name, Path -AutoSize
    Write-Host ""

    # Version Info
    $fileContent = Get-Content -Path $VersionHeader
    $major       = Get-VersionValue $fileContent "OIV_VERSION_MAJOR"
    $minor       = Get-VersionValue $fileContent "OIV_VERSION_MINOR"
    $revision    = Get-GitRevisionCount
    $now         = Get-Date
    $DateShort   = $now.ToString("yyyy-MM-dd")
    $DateLong    = $now.ToString("yyyy-MM-dd_HH-mm-ss")
    $versionShort= "$major.$minor.$revision.$OIV_VERSION_BUILD"
    $versionFull = if ($OIV_OFFICIAL_RELEASE -eq 0) {
        "$versionShort-$(Get-GitShortHash)-Nightly"
    } else { $versionShort }

    Write-Host "Build Info"
    Write-Host "-----------------------------------------------"
    Write-Host ("Root Dir      : {0}" -f $RootDir)
    Write-Host ("Build Dir     : {0}" -f $BuildDir)
    Write-Host ("Bin Dir       : {0}" -f $BinDir)
    Write-Host ("Version Short : {0}" -f $versionShort)
    Write-Host ("Version Full  : {0}" -f $versionFull)
    Write-Host ("Date          : {0}" -f $DateShort)
    Write-Host ("Time          : {0}" -f $DateLong)
    Write-Host "==============================================="

    # Run CMake
    if ($EnableCMake) {
        Push-Location $RootDir
        try {
            Write-Host "🛠️  Running CMake..."
            & "$CMakePath" -S . `
                -B "$BuildDir" `
                -G Ninja `
                -DCMAKE_BUILD_TYPE="$BuildType" `
                -DCMAKE_MT="$MT" `
                -DCMAKE_C_COMPILER="$Clang" `
                -DCMAKE_CXX_COMPILER="$Clang" `
                -DCMAKE_MAKE_PROGRAM="$NinjaPath" `
                -DCMAKE_RC_COMPILER="$RC" `
                -DIMCODEC_BUILD_CODEC_FREEIMAGE=ON `
                -DOIV_OFFICIAL_BUILD="$OIV_OFFICIAL_BUILD" `
                -DOIV_OFFICIAL_RELEASE="$OIV_OFFICIAL_RELEASE" `
                -DOIV_VERSION_BUILD="$OIV_VERSION_BUILD" `
                -DOIV_RELEASE_SUFFIX="$OIV_RELEASE_SUFFIX"

            if ($LASTEXITCODE -ne 0) {
                Raise-Error "CMake configuration failed."
            }
        } finally {
            Pop-Location
        }
    }

    # Build
    if ($EnableBuild) {
        Push-Location $BuildDir
        try {
            Write-Host "🔨 Building Project..."
            & $NinjaPath
            if ($LASTEXITCODE -ne 0) {
                Raise-Error "Build failed."
            }
        } finally {
            Pop-Location
        }
    }

    # Package
    if ($EnablePackage) {
        Write-Host "📦 Packaging..."
        $OutputDir = "$BuildDir/$DateLong-v$versionShort"
        $BaseName  = "$OutputDir/$DateShort-OIV-$versionFull-Win32x64VC-LLVM"

        Ensure-Directory $OutputDir
        Copy-Item "$DependenciesPath\*.dll" -Destination $BinDir -Force

        & $SevenZipPath a -mx9 "$BaseName-Symbols.7z" "$BinDir\*.pdb"
        & $SevenZipPath a -mx9 "$BaseName.7z" "$BinDir\*.dll" "$BinDir\*.exe" "$BinDir\Resources"

        if ($LASTEXITCODE -ne 0) {
            Raise-Error "Packaging failed."
        }
    }

    Write-Host "`nBuild & Packaging Complete!" -ForegroundColor Green
}

#---------------------------------------------------------------
# Top-Level Script Wrapper
#---------------------------------------------------------------

$NotifyBuildFailed=$true

try 
{
    Run-OIVBuild
    $NotifyBuildFailed=$false
} 
catch 
{
    Write-Host "`nScript failed: $_" -ForegroundColor Red
    $NotifyBuildFailed=$false
    exit 1
} 
finally 
{
        if ($NotifyBuildFailed)
        {
            Write-Host "`nScript failed or cancelled by the user" -ForegroundColor Red
        }
}
