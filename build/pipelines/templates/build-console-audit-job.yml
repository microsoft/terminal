parameters:
  platform: ''
  additionalBuildArguments: ''

jobs:
- job: Build${{ parameters.platform }}AuditMode
  displayName: Static Analysis Build ${{ parameters.platform }}
  variables:
    BuildConfiguration: AuditMode
    BuildPlatform: ${{ parameters.platform }}
  pool: { vmImage: windows-2019 }

  steps:
  - checkout: self
    submodules: true
    clean: true

  - task: NuGetToolInstaller@0
    displayName: Ensure NuGet 4.8.1
    inputs:
      versionSpec: 4.8.1

  # In the Microsoft Azure DevOps tenant, NuGetCommand is ambiguous.
  # This should be `task: NuGetCommand@2`
  - task: 333b11bd-d341-40d9-afcf-b32d5ce6f23b@2
    displayName: Restore NuGet packages
    inputs:
      command: restore
      feedsToUse: config
      configPath: NuGet.config
      restoreSolution: OpenConsole.sln
      restoreDirectory: '$(Build.SourcesDirectory)\packages'

  - task: 333b11bd-d341-40d9-afcf-b32d5ce6f23b@2
    displayName: 'NuGet restore packages for CI'
    inputs:
      command: restore
      restoreSolution: build/.nuget/packages.config
      feedsToUse: config
      externalFeedCredentials: 'TAEF NuGet Feed'
      nugetConfigPath: build/config/NuGet.config
      restoreDirectory: '$(Build.SourcesDirectory)/packages'

  - task: VSBuild@1
    displayName: 'Build solution **\OpenConsole.sln'
    inputs:
      solution: '**\OpenConsole.sln'
      vsVersion: 16.0
      platform: '$(BuildPlatform)'
      configuration: '$(BuildConfiguration)'
      msbuildArgs: ${{ parameters.additionalBuildArguments }}
      clean: true
      maximumCpuCount: true
