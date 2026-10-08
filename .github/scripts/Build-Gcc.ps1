#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^\d+\.\d+\.\d+$')][string] $Version,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{64}$')][string] $Sha256,
    [Parameter(Mandatory)][string] $InstallPrefix,
    [Parameter(Mandatory)][string] $WorkDirectory
)

$ErrorActionPreference = 'Stop'

function Invoke-BuildTool {
    param([string] $Tool, [string[]] $Arguments)
    "+ $Tool $($Arguments -join ' ')" | Tee-Object -FilePath $logPath -Append | Out-Host
    & $Tool @Arguments 2>&1 | Tee-Object -FilePath $logPath -Append | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "$Tool failed with exit code $LASTEXITCODE"
    }
}

# Build in the caller's baseline distribution so the installed runtimes retain its glibc requirements.
$InstallPrefix = [System.IO.Path]::GetFullPath($InstallPrefix)
$WorkDirectory = [System.IO.Path]::GetFullPath($WorkDirectory)
$null = New-Item -ItemType Directory -Path $WorkDirectory -Force
$logPath = Join-Path $WorkDirectory 'bootstrap.log'
$archive = Join-Path $WorkDirectory "gcc-$Version.tar.xz"
Invoke-BuildTool curl @('--proto', '=https', '--tlsv1.2', '-fsSL', '--retry', '3',
    "https://ftp.gnu.org/gnu/gcc/gcc-$Version/gcc-$Version.tar.xz", '-o', $archive)
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $Sha256) {
    throw 'GCC source checksum does not match'
}
Invoke-BuildTool tar @('-xf', $archive, '-C', $WorkDirectory)
$buildDirectory = Join-Path $WorkDirectory 'build'
$null = New-Item -ItemType Directory -Path $buildDirectory, $InstallPrefix -Force
Push-Location $buildDirectory
try {
    @(
        "Build directory: $buildDirectory"
        foreach ($name in @('GCC_ROOT', 'OIV_GCC_ROOT', 'CC', 'CXX', 'CFLAGS', 'CXXFLAGS',
                             'CPPFLAGS', 'GCC_EXEC_PREFIX', 'COMPILER_PATH', 'CPATH', 'LD_LIBRARY_PATH')) {
            "$name=$([Environment]::GetEnvironmentVariable($name))"
        }
    ) | Tee-Object -FilePath $logPath -Append | Out-Host
    Invoke-BuildTool (Join-Path $WorkDirectory "gcc-$Version/configure") @(
        "--prefix=$InstallPrefix", '--enable-languages=c,c++', '--disable-multilib',
        '--disable-bootstrap', '--disable-libsanitizer')
    Invoke-BuildTool make @("-j$([Environment]::ProcessorCount)")
    Invoke-BuildTool make @('install')
} catch {
    # Autoconf records the underlying compiler and probe execution errors here.
    $configLog = Join-Path $buildDirectory 'config.log'
    if (Test-Path -LiteralPath $configLog) {
        Get-Content -LiteralPath $configLog | Write-Host
    }
    throw
} finally {
    Pop-Location
}
