# generate_certs.ps1
# Generates dummy self-signed certificates for gRPC mTLS testing

Write-Host "Generating CA Certificate..."
openssl req -x509 -newkey rsa:4096 -days 365 -nodes -keyout ca_key.pem -out ca_cert.pem -subj "/CN=AluTestCA"

Write-Host "Generating Server Key and CSR..."
openssl req -newkey rsa:4096 -nodes -keyout server_key.pem -out server_req.pem -subj "/CN=localhost"

Write-Host "Signing Server Certificate..."
openssl x509 -req -in server_req.pem -days 365 -CA ca_cert.pem -CAkey ca_key.pem -CAcreateserial -out server_cert.pem

Write-Host "Generating Client Key and CSR..."
openssl req -newkey rsa:4096 -nodes -keyout client_key.pem -out client_req.pem -subj "/CN=AluClient"

Write-Host "Signing Client Certificate..."
openssl x509 -req -in client_req.pem -days 365 -CA ca_cert.pem -CAkey ca_key.pem -CAcreateserial -out client_cert.pem

Write-Host "Cleaning up CSRs..."
Remove-Item server_req.pem
Remove-Item client_req.pem

Write-Host "mTLS Certificates Generated Successfully!"
