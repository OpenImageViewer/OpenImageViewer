# Loading helpers must not configure, build or package anything.
$buildScript = Join-Path $PSScriptRoot "../../scripts/build.ps1"
. $buildScript
$defaultBuildTarget = "OIViewer"

Describe "Publish build target selection" {
    BeforeEach {
        Mock Invoke-NativeCommand {}
        Mock Get-GitOutput { "123" }
        $options = [PSCustomObject]@{
            BuildType                = "RelWithDebInfo"
            BuildDir                 = "publish-wlx"
            Root                     = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
            BuildTarget              = $defaultBuildTarget
            EnableConfigure          = $false
            EnableBuild              = $true
            EnablePackage            = $false
            CleanConfigure           = $false
            CleanConfigureOnMismatch = $true
            EchoCommands             = $false
            VerboseOutput            = $false
            CMakeDefinitions         = @{}
            OfficialBuild            = $true
            OfficialRelease          = $true
            BuildGeneratorPath       = $null
            BuildToolPath            = $null
            ArchiveToolPath          = $null
            GitPath                  = $null
            CPPCompilerPath          = $null
            RcPath                   = $null
            MtPath                   = $null
        }
        $tools = [PSCustomObject]@{ Paths = @{ BuildGenerator = "cmake" }; Display = @() }
    }

    It "keeps OIViewer as the default target" {
        $defaultBuildTarget | Should Be "OIViewer"
        $context = New-PublishContext -Options $options -ToolInfo $tools -PlatformProfile $null
        $context.RootDir | Should Be (Convert-ToForwardSlash $options.Root)
        Invoke-CMakeBuild $context
        Assert-MockCalled Invoke-NativeCommand 1 -ParameterFilter {
            $Arguments[-2] -eq "--target" -and $Arguments[-1] -eq "OIViewer"
        }
    }

    It "passes package_wlx through the build context as one native argument" {
        $options.BuildTarget = "package_wlx"
        $context = New-PublishContext -Options $options -ToolInfo $tools -PlatformProfile $null
        $context.BuildTarget | Should Be "package_wlx"
        Invoke-CMakeBuild $context
        Assert-MockCalled Invoke-NativeCommand 1 -ParameterFilter {
            $Arguments[-2] -eq "--target" -and $Arguments[-1] -eq "package_wlx"
        }
    }

    It "falls back to the project root when run outside a Git worktree" {
        Push-Location $TestDrive
        try {
            $root = Resolve-ProjectRoot $null
        } finally {
            Pop-Location
        }
        $expectedRoot = Convert-ToForwardSlash (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
        $root | Should Be $expectedRoot
    }

    It "loads tool definitions without an optional Fallbacks property" {
        $platformFile = Join-Path $TestDrive "PlatformTools.json"
        Set-Content -Path $platformFile -Value '{"Linux":{"Tools":[{"Key":"Git","Name":"Git","Commands":["git"],"VsDiscoverable":false}],"PackageSpec":{}}}'
        $platformData = Load-PlatformTools $TestDrive
        $tool = $platformData.Linux.Tools[0]
        @($tool.Fallbacks).Count | Should Be 0
    }

    It "reads typed CMake cache entries" {
        $cachePath = Join-Path $TestDrive "CMakeCache.txt"
        Set-Content -Path $cachePath -Value @(
            "CMAKE_GENERATOR:INTERNAL=Ninja"
            "CMAKE_BUILD_TYPE:STRING=RelWithDebInfo"
            "CMAKE_MAKE_PROGRAM:FILEPATH=C:/Tools/ninja.exe"
            "EMPTY_VALUE:STRING="
            "UNTYPED=value"
        )
        $cache = Read-CMakeCache $cachePath
        $requirements = @(
            [pscustomobject]@{ Name="CMAKE_GENERATOR"; Value="Ninja"; IsPath=$false }
            [pscustomobject]@{ Name="CMAKE_BUILD_TYPE"; Value="RelWithDebInfo"; IsPath=$false }
            [pscustomobject]@{ Name="CMAKE_MAKE_PROGRAM"; Value="C:/Tools/ninja.exe"; IsPath=$true }
            [pscustomobject]@{ Name="EMPTY_VALUE"; Value=""; IsPath=$false }
            [pscustomobject]@{ Name="UNTYPED"; Value="value"; IsPath=$false }
        )
        @(Compare-CMakeCacheEntries $cache $requirements).Count | Should Be 0
    }

    It "compares non-path CMake cache values case-sensitively" {
        (Test-CMakeCacheValueEquals "RelWithDebInfo" "relwithdebinfo" $false) | Should Be $false
    }

    It "compares CMake paths using the host platform's case rules" {
        $samePath = Test-CMakeCacheValueEquals "/tmp/OIVCase" "/tmp/oivcase" $true
        if ((Get-PublishPlatform) -eq "Windows") {
            $samePath | Should Be $true
        } else {
            $samePath | Should Be $false
        }
        (Normalize-ComparablePath "") | Should Be $null
    }

    It "detects the host platform even if OS is inherited from Windows" {
        $previousOs = $env:OS
        try {
            $env:OS = "Windows_NT"
            $expectedPlatform = if ([System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform(
                    [System.Runtime.InteropServices.OSPlatform]::Windows)) { "Windows" } else { "Linux" }
            (Get-PublishPlatform) | Should Be $expectedPlatform
        } finally {
            $env:OS = $previousOs
        }
    }

    It "does not require Visual Studio to probe an unused tool" {
        $path = Resolve-ToolPath -Name "optional test tool" -Key "Unmapped" `
            -CommandNames @("__oiv_missing_tool_28941__") -FallbackPaths @() `
            -PreferredPath $null -OverridePath $null -Required $false -VsDiscoverable $true
        $path | Should Be $null
    }

    It "adds Windows SDK resource tools to the Windows profile" {
        Mock Get-WindowsSdkToolPaths { @("C:/Windows Kits/10/bin/test/x64/$ToolName") }
        $profile = New-PlatformProfile -Options $options -Platform "Windows"
        $rc = $profile.ToolSpecs | Where-Object { $_.Key -eq "RC" }
        $mt = $profile.ToolSpecs | Where-Object { $_.Key -eq "MT" }
        ($rc.Fallbacks -contains "C:/Windows Kits/10/bin/test/x64/rc.exe") | Should Be $true
        ($mt.Fallbacks -contains "C:/Windows Kits/10/bin/test/x64/mt.exe") | Should Be $true
    }

    It "does not require a Visual Studio environment for standalone Windows SDK tools" {
        $tools = @{
            CPPCompiler = "C:/LLVM/bin/clang-cl.exe"
            RC = "C:/Windows Kits/10/bin/test/x64/rc.exe"
            MT = "C:/Windows Kits/10/bin/test/x64/mt.exe"
        }
        (Test-ToolchainNeedsVsEnvironment $tools) | Should Be $false
    }

    It "requires a Visual Studio environment for its bundled compiler" {
        $tools = @{
            CPPCompiler = "C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/Llvm/bin/clang-cl.exe"
            RC = "C:/Windows Kits/10/bin/test/x64/rc.exe"
            MT = "C:/Windows Kits/10/bin/test/x64/mt.exe"
        }
        (Test-ToolchainNeedsVsEnvironment $tools) | Should Be $true
    }

    It "re-resolves tools after importing the Visual Studio environment" {
        Mock Resolve-ToolPaths {
            @{
                CPPCompiler = "C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/Llvm/x64/bin/clang-cl.exe"
                RC = "C:/Windows Kits/10/bin/test/x64/rc.exe"
                MT = "C:/Windows Kits/10/bin/test/x64/mt.exe"
            }
        }
        Mock Import-VSEnvironment {}
        Mock Fetch-VSTool {
            if ($Name -eq "RC") { return "C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/Llvm/x64/bin/llvm-rc.exe" }
            if ($Name -eq "MT") { return "C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/Llvm/x64/bin/llvm-mt.exe" }
            return $null
        }
        Mock Add-ToolDirectoriesToPath {}
        $profile = [pscustomobject]@{
            Platform = [PublishPlatform]::Windows
            ToolSpecs = @(
                [pscustomobject]@{ Key="CPPCompiler"; Name="clang-cl"; Required=$true; VsDiscoverable=$true; OverridePath=$null }
                [pscustomobject]@{ Key="RC"; Name="resource compiler"; Required=$true; VsDiscoverable=$true; OverridePath=$null }
                [pscustomobject]@{ Key="MT"; Name="manifest tool"; Required=$true; VsDiscoverable=$true; OverridePath=$null }
            )
        }
        $toolInfo = Resolve-BuildTools $profile $env:PATH $false
        $toolInfo.Paths.RC | Should Be "C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/Llvm/x64/bin/llvm-rc.exe"
        $toolInfo.Paths.MT | Should Be "C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/Llvm/x64/bin/llvm-mt.exe"
        Assert-MockCalled Resolve-ToolPaths 2
        Assert-MockCalled Import-VSEnvironment 1
        Assert-MockCalled Fetch-VSTool 2
    }

    It "uses the common module path helpers in VSDiscovery" {
        $module = Get-Module VSDiscovery | Select-Object -First 1
        $converted = & $module { Convert-ToForwardSlash 'C:\VS\Tools' }
        $root = Join-Path $TestDrive "VS"
        $child = "Tools/Llvm"
        $joined = & $module { param($base, $relative) Join-PathForward $base $relative } $root $child
        $converted | Should Be "C:/VS/Tools"
        $joined | Should Be (Convert-ToForwardSlash (Join-Path $root $child))
    }

    It "does not return formatted build info as a build result" {
        $context = [pscustomobject]@{
            Profile = [pscustomobject]@{ Platform="Windows" }
            ToolDisplay = @([pscustomobject]@{ Name="CMake"; Path="cmake.exe" })
            RootDir="C:/OIV"
            BuildDir="C:/OIV/publish"
            BinDir="C:/OIV/publish/bin"
            OfficialBuild=1
            OfficialRelease=0
            ReleaseVersion="0.18.0"
            VersionNumber="0.18.0.1"
            PackageVersion="0.18.0.1-dev"
            DateShort="2026-11-22"
            DateLong="2026-11-22_00-00-00"
        }
        @(Write-BuildInfo $context).Count | Should Be 0
    }

    It "restores PATH after a preferred Visual Studio lookup" {
        $previousPath = $env:PATH
        Resolve-VsWherePath -PreferredPath $TestDrive | Out-Null
        $env:PATH | Should Be $previousPath
    }
}
