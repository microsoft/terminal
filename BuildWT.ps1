param(
    [switch]$Clean,
    [switch]$Debug,
    [switch]$Release,
    [switch]$x64,
    [switch]$x86,
    [switch]$ARM64,
    [switch]$NoPackage,
    [int]$Zm = 1500,
    [switch]$Format,
    [switch]$Bundle,
    [switch]$DistClean, # Added DistClean parameter
    [switch]$Restore  # Added DistClean parameter
)

Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment

if ($DistClean) {
    Write-Host "Performing DistClean operation..."

    # Define the folders to delete
    $foldersToDelete = @("obj", "bin", "AppPackages", "Generated Files")

    # Define the folders to exclude
    $foldersToExclude = @("packages", ".PowershellModules")

    # Get the root path of the solution
    $solutionRoot = Get-Location

    # Find all directories recursively and filter by folder names, excluding specified directories
    Write-Host "Searching for directories to delete..."
    $directoriesToDelete = Get-ChildItem -Path $solutionRoot -Directory -Recurse -ErrorAction SilentlyContinue | Where-Object {
        # Folder name is in the list of folders to delete
        ($_.Name -in $foldersToDelete) -and
        # Folder name is not in the list of folders to exclude
        ($_.Name -notin $foldersToExclude) -and
        # Folder name does not start with a dot
        ($_.Name -notlike '.*') -and
        # Path does not contain any directory starting with a dot
        ($_.FullName -notmatch '(\\\.[^\\]*)')
    }

    if ($directoriesToDelete.Count -eq 0) {
        Write-Host "No matching directories found to delete."
    }
    else {
        foreach ($dir in $directoriesToDelete) {
            try {
                Write-Host "Deleting folder: $($dir.FullName)"
                Remove-Item -LiteralPath $dir.FullName -Force -Recurse
            }
            catch {
                Write-Warning "Failed to delete $($dir.FullName): $_"
            }
        }
    }

    Write-Host "DistClean operation completed."
    # Exit the script after cleaning
    return
}

# Set default configurations if none are specified
if (-not $Debug -and -not $Release) {
    $Release = $true
}

$Configurations = @()
if ($Release) { $Configurations += "Release" }
if ($Debug) { $Configurations += "Debug" }

# Set default platforms if none are specified
if (-not $x64 -and -not $x86 -and -not $ARM64) {
    $x64 = $true
}

$Platforms = @()
if ($x64) { $Platforms += "x64" }
if ($x86) { $Platforms += "x86" }
if ($ARM64) { $Platforms += "ARM64" }

# Clean builds if the Clean switch is specified
if ($Clean) {
    foreach ($Configuration in $Configurations) {
        foreach ($Platform in $Platforms) {
            Invoke-OpenConsoleBuild /t:Clean /p:Configuration=$Configuration /p:Platform=$Platform
        }
    }
}

if ($Restore) {
    # Check if nuget exists
    if (Get-Command nuget -ErrorAction SilentlyContinue) {
        Write-Host "NuGet exists, using nuget restore..."
        nuget restore OpenConsole.sln
    }
    else {
        Write-Host "NuGet not found, falling back to dotnet restore..."
        dotnet restore OpenConsole.sln
    }
}

# Call Get-Format only if Format is specified
if ($Format) {
    Get-Format
}

# Build commands
foreach ($Configuration in $Configurations) {
    foreach ($Platform in $Platforms) {
        Invoke-OpenConsoleBuild /p:Configuration=$Configuration /p:Platform=$Platform /p:AdditionalOptions="/Zm$Zm" /maxcpucount:4

        if (-not $NoPackage) {
            Invoke-OpenConsoleBuild /p:Configuration=$Configuration /p:Platform=$Platform /p:AdditionalOptions="/Zm$Zm" /t:Terminal\CascadiaPackage /m
        }
    }
}

# Copy .msix packages to .\msix folder
if (-not $NoPackage -and $Bundle) {
    $msixFolder = ".\msix"
    # Clean the msix folder before copying
    if (Test-Path $msixFolder) {
        Remove-Item -Path "$msixFolder\*" -Recurse -Force
    }
    else {
        New-Item -ItemType Directory -Path $msixFolder | Out-Null
    }

    $version = $null  # Initialize version variable

    foreach ($Platform in $Platforms) {
        # Build output path
        $appPackagesPath = ".\src\cascadia\CascadiaPackage\AppPackages"

        # Get the latest package folder for the platform
        $packageFolders = Get-ChildItem -Path $appPackagesPath -Directory | Where-Object { $_.Name -like "*_$Platform*" }
        if ($packageFolders.Count -eq 0) {
            Write-Warning "No package folders found for platform $Platform."
            continue
        }

        # Assuming the latest folder is the one we just built
        $latestPackageFolder = $packageFolders | Sort-Object LastWriteTime -Descending | Select-Object -First 1

        # Find the .msix file inside the package folder
        $msixFiles = Get-ChildItem -Path $latestPackageFolder.FullName -Filter "*.msix" -File
        if ($msixFiles.Count -eq 0) {
            Write-Warning "No .msix files found in $($latestPackageFolder.FullName) for platform $Platform."
            continue
        }

        # Copy the .msix files to the .\msix folder
        foreach ($msixFile in $msixFiles) {
            Copy-Item -Path $msixFile.FullName -Destination $msixFolder -Force
            Write-Host "Copied $($msixFile.Name) to $msixFolder"

            # Extract version from the msix filename if not already set
            if (-not $version) {
                if ($msixFile.Name -match 'CascadiaPackage_(?<version>[0-9\.]+)') {
                    $version = $matches['version']
                    Write-Host "Extracted version: $version"
                }
            }
        }
    }

    # Proceed to bundle, sign, and verify if version is found
    if ($version) {
        # Path to the output bundle
        $bundlePath = ".\WindowsTerminal_$version.msixbundle"

        # Create the bundle using makeappx
        $makeappxCmd = "makeappx bundle /d $msixFolder /p $bundlePath"
        Write-Host "Creating bundle with makeappx..."
        Invoke-Expression $makeappxCmd

        # Sign the bundle using signtool
        $certPath = ".\WindowsTerminal.pfx"
        if (Test-Path $certPath) {
            # Prompt for the certificate password securely
            $certPassword = Read-Host -AsSecureString "Enter the password for the certificate"

            # Convert the secure string to an encrypted standard string for signtool
            $ptr = [System.Runtime.InteropServices.Marshal]::SecureStringToBSTR($certPassword)
            $password = [System.Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)

            # Sign the bundle
            $signtoolCmd = "signtool sign /fd SHA256 /td SHA256 /a /f `"$certPath`" /p `"$password`" /tr http://timestamp.digicert.com `"$bundlePath`""
            Write-Host "Signing the bundle with signtool..."
            Invoke-Expression $signtoolCmd

            # Verify the signature
            $verifyCmd = "signtool verify /pa /v `"$bundlePath`""
            Write-Host "Verifying the bundle signature..."
            Invoke-Expression $verifyCmd

            # Zero out and dispose of the password for security
            [System.Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)
            $certPassword.Dispose()
        }
        else {
            Write-Warning "Certificate file $certPath not found. Skipping signing."
        }
    }
    else {
        Write-Warning "Version could not be determined. Skipping bundle creation and signing."
    }
}

# makeappx bundle /d .\msix\ /p WindowsTerminal_1.23.3101.1.msixbundle
# signtool sign /fd SHA256 /td SHA256 /a /f .\WindowsTerminal.pfx /p zlb36bpx /tr http://timestamp.digicert.com .\WindowsTerminal_1.23.3101.1.msixbundle
# signtool verify /pa /v .\WindowsTerminal_1.23.3101.1.msixbundle

# Import-Certificate -FilePath .\Dm17tryK.cer -CertStoreLocation Cert:\LocalMachine\Root
