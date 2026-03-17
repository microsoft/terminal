#
# Module manifest for module 'OpenConsole'
#
@{

# Script module or binary module file associated with this manifest.
RootModule = '.\OpenConsole.psm1'

# Version number of this module.
ModuleVersion = '0.0.1'

# Supported PSEditions
# CompatiblePSEditions = @()

# ID used to uniquely identify this module
GUID = '54190442-b882-4aea-b8a6-a65f9d56026c'

# Author of this module
Author = 'duhowett'

# Company or vendor of this module
CompanyName = 'Microsoft Corporation'

# Copyright statement for this module
Copyright = '(c) Microsoft Corporation.'

# Modules that must be imported into the global environment prior to importing this module
# RequiredModules = @()

# Assemblies that must be loaded prior to importing this module
# RequiredAssemblies = @()

# Script files (.ps1) that are run in the caller's environment prior to importing this module.
# ScriptsToProcess = @()

# Type files (.ps1xml) to be loaded when importing this module
# TypesToProcess = @()

# Format files (.ps1xml) to be loaded when importing this module
# FormatsToProcess = @()

# Modules to import as nested modules of the module specified in RootModule/ModuleToProcess
# NestedModules = @()

CmdletsToExport = @()
VariablesToExport = @()
AliasesToExport = @()
FunctionsToExport = @(
    'Set-MsbuildDevEnvironment',
    'Invoke-OpenConsoleTests',
    'Invoke-OpenConsoleBuild',
    'Start-OpenConsole',
    'Debug-OpenConsole',
    'Invoke-CodeFormat',
    'Invoke-XamlFormat',
    'Test-XamlFormat',
    'Find-OpenConsoleRoot',
    'Get-Git2GitIgnoresAsExcludes',
    'New-ConhostIngestionArchive'
)

# DSC resources to export from this module
# DscResourcesToExport = @()

# List of all modules packaged with this module
# ModuleList = @()

# List of all files packaged with this module
# FileList = @()

# Private data to pass to the module specified in RootModule/ModuleToProcess. This may also contain a PSData hashtable with additional module metadata used by PowerShell.
PrivateData = @{

    PSData = @{

    } # End of PSData hashtable

} # End of PrivateData hashtable

}

