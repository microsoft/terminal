# Check if running as administrator
if (-not ([Security.Principal.WindowsPrincipal]([Security.Principal.WindowsIdentity]::GetCurrent())).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "Please run this script as Administrator."
    exit 1
}

# Import the certificate
Write-Host "Importing certificate..."
Import-Certificate -FilePath .\Dm17tryK.cer -CertStoreLocation Cert:\LocalMachine\Root

Write-Host "Certificate installed successfully!"
