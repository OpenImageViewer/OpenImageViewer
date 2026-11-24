#===============================================================
# VSDiscovery - Visual Studio tool discovery
#===============================================================

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Import-Module (Join-Path $PSScriptRoot "OIV.Build.Helpers.psd1") -DisableNameChecking -ErrorAction Stop

$script:vsCache = @{
    Instance = $null
    Tools    = @{}
    EnvDone  = $false
}

function Get-IsWindows {
    [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Windows)
}

function Resolve-VsWherePath {
    param([string]$PreferredPath)
    if ($PreferredPath) {
        $previousPath = $env:PATH
        try {
            $env:PATH = $PreferredPath
            $cmd = Get-Command vswhere.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($cmd) { return Convert-ToForwardSlash $cmd.Source }
        } finally {
            $env:PATH = $previousPath
        }
    }
    $cmd = Get-Command vswhere.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd) { return Convert-ToForwardSlash $cmd.Source }
    $fallback = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $fallback -PathType Leaf) {
        return Convert-ToForwardSlash (Resolve-Path -LiteralPath $fallback).Path
    }
    return $null
}

function Get-VSCachedInstance {
    if (-not (Get-IsWindows)) { return $null }
    if ($script:vsCache.Instance) { return $script:vsCache.Instance }
    $vsWherePath = Resolve-VsWherePath
    if (-not $vsWherePath) { Raise-Error "vswhere.exe not found." }
    $output = @(& $vsWherePath -products '*' -latest -requires "Microsoft.VisualStudio.Component.VC.Tools.x86.x64" -property "installationPath" 2>$null)
    $instPath = ($output | Where-Object { $_ } | Select-Object -First 1)
    if (-not $instPath) {
        Raise-Error "Visual Studio with the C++ x86/x64 tools component is not installed. Install it via Visual Studio Installer -> Individual components."
    }
    $script:vsCache.Instance = Convert-ToForwardSlash $instPath.Trim()
    return $script:vsCache.Instance
}

function Resolve-ToolInInstance {
    param([string]$InstancePath, [string]$RelativePath)
    $fullPath = Join-PathForward $InstancePath $RelativePath
    if ($RelativePath -match '\*') {
        $matches = @(Resolve-Path -Path $fullPath -ErrorAction SilentlyContinue)
        foreach ($m in $matches) {
            if (Test-Path -LiteralPath $m.Path -PathType Leaf) { return Convert-ToForwardSlash $m.Path }
        }
        return $null
    }
    if (Test-Path -LiteralPath $fullPath -PathType Leaf) {
        return Convert-ToForwardSlash (Resolve-Path -LiteralPath $fullPath).Path
    }
    return $null
}

$script:toolMapping = @{
    CPPCompiler    = "VC\Tools\Llvm\x64\bin\clang-cl.exe"
    RC             = "VC\Tools\Llvm\x64\bin\llvm-rc.exe"
    MT             = "VC\Tools\Llvm\x64\bin\llvm-mt.exe"
    BuildGenerator = "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    BuildTool      = "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    vcvars         = "Common7\Tools\VsDevCmd.bat"
}

function Fetch-VSTool {
    param([string]$Name)
    if (-not (Get-IsWindows)) { return $null }
    if ($script:vsCache.Tools.ContainsKey($Name)) { return $script:vsCache.Tools[$Name] }
    $instance = Get-VSCachedInstance
    if (-not $instance) { return $null }
    $relative = $script:toolMapping[$Name]
    if (-not $relative) { return $null }
    $path = Resolve-ToolInInstance $instance $relative
    $script:vsCache.Tools[$Name] = $path
    return $path
}

function Import-VSEnvironment {
    if (-not (Get-IsWindows)) { return }
    if ($script:vsCache.EnvDone) { return }
    $instance = Get-VSCachedInstance
    $vsDevCmd = Fetch-VSTool "vcvars"
    if (-not $vsDevCmd) { Raise-Error "VsDevCmd.bat not found in VS instance." }
    $command = "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul && set"
    $originalPath = $env:PATH
    $systemRoot = $env:SystemRoot
    $minimalPath = @(
        (Join-Path $systemRoot "System32")
        (Join-Path $systemRoot "System32/Wbem")
        (Join-Path $systemRoot "System32/WindowsPowerShell/v1.0")
    ) -join ';'
    try {
        $env:PATH = $minimalPath
        $cmdPath = Join-Path $systemRoot "System32/cmd.exe"
        $environment = & $cmdPath /d /s /c $command
        $exitCode = $LASTEXITCODE
    } finally {
        $env:PATH = $originalPath
    }
    if ($exitCode -ne 0) { Raise-Error "Failed to load Visual Studio build environment (exit $exitCode)." }

    $visualStudioPath = $null
    foreach ($line in $environment) {
        if ($line -match '^([^=]+)=(.*)$') {
            if ($matches[1] -ieq 'PATH') { $visualStudioPath = $matches[2]; continue }
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
        }
    }
    if ($visualStudioPath) {
        $seenPaths = @{}
        $mergedPath = @(
            @($visualStudioPath -split ';') + @($originalPath -split ';') | Where-Object {
                $entry = $_.Trim()
                if ($entry -and -not $seenPaths.ContainsKey($entry)) {
                    $seenPaths[$entry] = $true
                    $entry
                }
            }
        )
        [Environment]::SetEnvironmentVariable('PATH', ($mergedPath -join ';'), 'Process')
    }
    $script:vsCache.EnvDone = $true
}

Export-ModuleMember -Function @('Fetch-VSTool', 'Import-VSEnvironment', 'Get-VSCachedInstance', 'Resolve-VsWherePath', 'Resolve-ToolInInstance')
