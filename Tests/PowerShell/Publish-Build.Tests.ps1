# Loading helpers must not configure, build or package anything.
$buildScript = Join-Path $PSScriptRoot "../../publish.ps1"
. $buildScript
$defaultBuildTarget = $BuildTarget

Describe "Publish build target selection" {
    BeforeEach {
        Mock Invoke-NativeCommand {}
        Mock Get-GitOutput { "123" }
        $options = [PSCustomObject]@{
            BuildType = "RelWithDebInfo"
            BuildDir = "publish-wlx"
            BuildTarget = $defaultBuildTarget
            EnableConfigure = $false
            EnableBuild = $true
            EnablePackage = $false
            CleanConfigure = $false
            CleanConfigureOnMismatch = $true
            EchoCommands = $false
            CMakeDefinitions = @{}
            OfficialBuild = $true
            OfficialRelease = $true
        }
        $tools = [PSCustomObject]@{ Paths = @{ CMake = "cmake" }; Display = @() }
    }

    It "keeps OIViewer as the default target" {
        $defaultBuildTarget | Should Be "OIViewer"
        $context = New-PublishContext -Options $options -ToolInfo $tools -Profile $null
        Invoke-CMakeBuild $context
        Assert-MockCalled Invoke-NativeCommand 1 -ParameterFilter {
            $Arguments[-2] -eq "--target" -and $Arguments[-1] -eq "OIViewer"
        }
    }

    It "passes package_wlx through the build context as one native argument" {
        $options.BuildTarget = "package_wlx"
        $context = New-PublishContext -Options $options -ToolInfo $tools -Profile $null
        $context.BuildTarget | Should Be "package_wlx"
        Invoke-CMakeBuild $context
        Assert-MockCalled Invoke-NativeCommand 1 -ParameterFilter {
            $Arguments[-2] -eq "--target" -and $Arguments[-1] -eq "package_wlx"
        }
    }
}
