parameters:
  configuration: 'Release'

jobs:
- job: SignDeploy${{ parameters.configuration }}
  displayName: Sign and Deploy for ${{ parameters.configuration }}

  dependsOn:
    - Buildx64AuditMode
    - Buildx64Release
    - Buildx86Release
    - Buildarm64Release
    - CodeFormatCheck
  condition: |
    and
    (
      in(dependencies.Buildx64AuditMode.result, 'Succeeded', 'SucceededWithIssues', 'Skipped'),
      in(dependencies.Buildx64Release.result,   'Succeeded', 'SucceededWithIssues', 'Skipped'),
      in(dependencies.Buildx86Release.result,   'Succeeded', 'SucceededWithIssues', 'Skipped'),
      in(dependencies.Buildarm64Release.result, 'Succeeded', 'SucceededWithIssues', 'Skipped'),
      in(dependencies.CodeFormatCheck.result,   'Succeeded', 'SucceededWithIssues', 'Skipped')
    )

  variables:
    BuildConfiguration: ${{ parameters.configuration }}
    AppxProjectName: CascadiaPackage
    AppxBundleName: Microsoft.WindowsTerminal_8wekyb3d8bbwe.msixbundle

  pool:
    name: Package ES Lab E

  steps:
  - checkout: self
    clean: true

  - task: PkgESSetupBuild@10
    displayName: 'Package ES - Setup Build'
    inputs:
      useDfs: false
      productName: WindowsTerminal
      disableOutputRedirect: true

  - task: ms.vss-governance-buildtask.governance-build-task-component-detection.ComponentGovernanceComponentDetection@0
    displayName: 'Component Detection'

  - task: DownloadBuildArtifacts@0
    displayName: Download AppX artifacts
    inputs:
      artifactName: 'appx-$(BuildConfiguration)'
      itemPattern: |
        **/*.appx
        **/*.msix
      downloadPath: '$(Build.ArtifactStagingDirectory)\appx'

  - task: PowerShell@2
    displayName: 'Create $(AppxBundleName)'
    inputs:
      targetType: filePath
      filePath: '.\build\scripts\Create-AppxBundle.ps1'
      arguments: |
        -InputPath "$(Build.ArtifactStagingDirectory)\appx" -ProjectName $(AppxProjectName) -BundleVersion 0.0.0.0 -OutputPath "$(Build.ArtifactStagingDirectory)\$(AppxBundleName)"

  - task: PkgESCodeSign@10
    displayName: 'Package ES - SignConfig.WindowsTerminal.xml'
    inputs:
      signConfigXml: 'build\config\SignConfig.WindowsTerminal.xml'
      inPathRoot: '$(Build.ArtifactStagingDirectory)'
      outPathRoot: '$(Build.ArtifactStagingDirectory)\signed'

  - task: PublishBuildArtifacts@1
    displayName: 'Publish Signed AppX'
    inputs:
      PathtoPublish: '$(Build.ArtifactStagingDirectory)\signed'
      ArtifactName: 'appxbundle-signed-$(BuildConfiguration)'
