# 1. Create a Self-Signed Certificate in CurrentUser to avoid Admin requirements
$cert = New-SelfSignedCertificate -Subject "CN=Alu Aegis Kernel Test" -Type CodeSigningCert -CertStoreLocation "Cert:\CurrentUser\My"

# 2. Export the certificate
$certPath = "AegisTestCert.cer"
Export-Certificate -Cert $cert -FilePath $certPath -Force

# 3. Find signtool.exe from the Windows SDK (explicitly x64)
$signtool = (Get-ChildItem -Path "C:\Program Files (x86)\Windows Kits\10\bin\" -Filter "signtool.exe" -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match "\\x64\\" } | Select-Object -First 1).FullName

if (-not $signtool) {
    Write-Error "signtool.exe (x64) not found! Please install the Windows SDK."
    exit 1
}

# 4. Sign the driver using the certificate's thumbprint from the store
Write-Host "Signing aegis_kernel.sys with thumbprint $($cert.Thumbprint)... using $signtool"
& $signtool sign /fd SHA256 /sha1 $cert.Thumbprint /v "aegis_kernel.sys"

if ($LASTEXITCODE -eq 0) {
    Write-Host "Success! aegis_kernel.sys is now digitally signed."
} else {
    Write-Error "Signing failed."
}
