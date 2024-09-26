## Usage
1. Update the terraform.tfvars file with your domain and email.

2. Run `terraform init` to initialize the Terraform working directory.

3. Run `terraform plan` to see the planned changes.

4. Run `terraform apply` to create the resources and obtain the certificate.
Outputs

The configuration provides two sensitive outputs:
```bash
certificate_pem: The full certificate chain
private_key_pem: The private key for the certificate
```

These can be then be used in your `moq-relay` as it requires SSL/TLS certificates.

## Note
The generated certificate and key files are saved locally and ignored by git:
```git
.terraform
relay_*
```