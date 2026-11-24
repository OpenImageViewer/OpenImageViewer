@{
    RootModule = 'OIV.Build.Helpers.psm1'
    ModuleVersion = '1.1.0'
    Description = 'Shared build and packaging helpers for OIV'
    GUID = 'a1b2c3d4-e5f6-7890-abcd-ef1234567890'
    FunctionsToExport = @(
        'Join-PathForward',
        'Convert-ToForwardSlash',
        'Resolve-BuildPath',
        'Ensure-Directory',
        'Raise-Error',
        'Require-Value',
        'Quote-CommandArgument',
        'Format-CommandLine',
        'Invoke-NativeCommand',
        'Get-RequiredFirstOutputLine',
        'Get-ExistingToolPath',
        'Find-CommandPath',
        'Resolve-ToolPath',
        'Resolve-ToolPaths',
        'Get-MissingRequiredToolSpecs',
        'Assert-RequiredToolsResolved',
        'Get-RequiredToolPath',
        'Add-ToolDirectoriesToPath',
        'New-ToolSpec',
        'Read-CMakeCache',
        'Normalize-ComparablePath',
        'Test-CMakeCacheValueEquals',
        'Format-CMakeCacheValue',
        'Compare-CMakeCacheEntries',
        'Clear-CMakeConfigureState',
        'Reset-CMakeConfigureStateIfNeeded',
        'Assert-CMakeCacheMatches',
        'New-CMakeDefine',
        'Get-VersionValue',
        'Get-GitOutput',
        'Get-FirstTextValue',
        'Get-RequiredFirstTextValue',
        'Convert-ToPackageToken',
        'Convert-ToPackageTargetSystem',
        'Convert-ToPackagePlatform',
        'Convert-ToPackageToolchain',
        'Get-PackageBuildInfo',
        'Read-CMakeSetFile',
        'Format-PackageNameSuffix',
        'Write-PackageBuildInfo',
        'Update-PackageBuildInfo',
        'Get-PublishPlatform',
        'Resolve-ProjectRoot',
        'Load-PlatformTools',
        'Expand-EnvironmentVariables'
    )
    VariablesToExport = @()
}
