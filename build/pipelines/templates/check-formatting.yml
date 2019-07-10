
jobs:
- job: CodeFormatCheck
  displayName: Proper Code Formatting Check
  pool: { vmImage: windows-2019 }

  steps:
  - checkout: self
    submodules: false
    clean: true

  - task: PowerShell@2
    displayName: 'Code Formattting Check'
    inputs:
      targetType: filePath
      filePath: '.\build\scripts\Invoke-FormattingCheck.ps1'
