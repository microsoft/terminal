parameters:
  additionalBuildArguments: ''

steps:
- checkout: self
  submodules: true
  clean: true

- task: NuGetToolInstaller@0
  displayName: Ensure NuGet 4.8.1
  inputs:
    versionSpec: 4.8.1

- task: VisualStudioTestPlatformInstaller@1
  displayName: Ensure VSTest Platform

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
  displayName: 'Build solution **\OpenConsole.sln (no packages)'
  inputs:
    solution: '**\OpenConsole.sln'
    vsVersion: 16.0
    platform: '$(BuildPlatform)'
    configuration: '$(BuildConfiguration)'
    # Until there is a servicing release of Visual Studio 2019 Update 3, we must force the values of:
    # BuildingInsideVisualStudio
    # _WapBuildingInsideVisualStudio
    # GenerateAppxPackageOnBuild
    # because otherwise, they will cause a build instability where MSBuild considers all projects
    # to always be out-of-date.
    msbuildArgs: "${{ parameters.additionalBuildArguments }} /p:BuildingInsideVisualStudio=false;_WapBuildingInsideVisualStudio=false;GenerateAppxPackageOnBuild=false"
    clean: true
    maximumCpuCount: true

- task: VSBuild@1
  displayName: 'Build solution **\OpenConsole.sln (CascadiaPackage only)'
  inputs:
    solution: '**\OpenConsole.sln'
    vsVersion: 16.0
    platform: '$(BuildPlatform)'
    configuration: '$(BuildConfiguration)'
    msbuildArgs: "${{ parameters.additionalBuildArguments }} /p:BuildingInsideVisualStudio=false;_WapBuildingInsideVisualStudio=false;GenerateAppxPackageOnBuild=true /t:Terminal\\CascadiaPackage"
    clean: false # we're relying on build output fropm the previous run
    maximumCpuCount: true

- task: PowerShell@2
  displayName: 'Check MSIX for common regressions'
  inputs:
    targetType: inline
    script: |
      $Package = Get-ChildItem -Recurse -Filter "CascadiaPackage_*.msix"
      .\build\scripts\Test-WindowsTerminalPackage.ps1 -Verbose -Path $Package.FullName

- task: powershell@2
  displayName: 'Source Index PDBs'
  inputs:
    targetType: filePath
    filePath: build\scripts\Index-Pdbs.ps1
    arguments: -SearchDir '$(Build.SourcesDirectory)' -SourceRoot '$(Build.SourcesDirectory)' -recursive -Verbose -CommitId $(Build.SourceVersion)
    errorActionPreference: silentlyContinue

- task: VSTest@2
  displayName: 'Run Unit Tests'
  inputs:
    testAssemblyVer2: |
      $(BUILD.SOURCESDIRECTORY)\**\*unit.test*.dll
      !**\obj\**
    runSettingsFile: '$(BUILD.SOURCESDIRECTORY)\src\unit.tests.$(BuildPlatform).runsettings'
    codeCoverageEnabled: true
    runInParallel: False
    testRunTitle: 'Console Unit Tests'
    platform: '$(BuildPlatform)'
    configuration: '$(BuildConfiguration)'
  condition: and(succeeded(), or(eq(variables['BuildPlatform'], 'x64'), eq(variables['BuildPlatform'], 'x86')))

- task: VSTest@2
  displayName: 'Run Feature Tests (x64 only)'
  inputs:
    testAssemblyVer2: |
      $(BUILD.SOURCESDIRECTORY)\**\*feature.test*.dll
      !**\obj\**
    runSettingsFile: '$(BUILD.SOURCESDIRECTORY)\src\unit.tests.$(BuildPlatform).runsettings'
    codeCoverageEnabled: true
    runInParallel: False
    testRunTitle: 'Console Feature Tests'
    platform: '$(BuildPlatform)'
    configuration: '$(BuildConfiguration)'
  condition: and(succeeded(), eq(variables['BuildPlatform'], 'x64'))

- task: CopyFiles@2
  displayName: 'Copy *.appx/*.msix to Artifacts (Non-PR builds only)'
  inputs:
    Contents: |
     **/*.appx
     **/*.msix
     **/*.appxsym
     !**/Microsoft.VCLibs*.appx
    TargetFolder: '$(Build.ArtifactStagingDirectory)/appx'
    OverWrite: true
    flattenFolders: true
  condition: and(succeeded(), ne(variables['Build.Reason'], 'PullRequest'))

- task: PublishBuildArtifacts@1
  displayName: 'Publish Artifact (appx) (Non-PR builds only)'
  inputs:
    PathtoPublish: '$(Build.ArtifactStagingDirectory)/appx'
    ArtifactName: 'appx-$(BuildConfiguration)'
  condition: and(succeeded(), ne(variables['Build.Reason'], 'PullRequest'))
