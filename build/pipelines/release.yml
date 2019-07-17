trigger: none
pr: none

variables:
  baseYearForVersioning: 2019 # Used by build-console-int
  versionMajor: 0
  versionMinor: 1

# When we move off PackageES for Versioning, we'll need to switch
# name to this format. For now, though, we need to use DayOfYear.Rev
# to unique our builds, as mandated by PackageES's Setup task.
# name: '$(versionMajor).$(versionMinor).$(DayOfYear)$(Rev:r).0'
#
# Build name/version number above must end with .0 to make the
# store publication machinery happy.
name: 'Terminal_$(date:yyMM).$(date:dd)$(rev:rrr)'

jobs:
  - template: ./templates/build-console-audit-job.yml
    parameters:
      platform: x64

  - template: ./templates/build-console-int.yml
    parameters:
      platform: x64
      additionalBuildArguments: /p:WindowsTerminalReleaseBuild=true

  - template: ./templates/build-console-int.yml
    parameters:
      platform: x86
      additionalBuildArguments: /p:WindowsTerminalReleaseBuild=true

  - template: ./templates/build-console-int.yml
    parameters:
      platform: arm64
      additionalBuildArguments: /p:WindowsTerminalReleaseBuild=true

  - template: ./templates/check-formatting.yml

  - template: ./templates/release-sign-and-bundle.yml
