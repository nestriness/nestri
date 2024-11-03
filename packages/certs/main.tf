terraform {
  required_providers {
    acme = {
      source  = "vancluever/acme"
      version = "~> 2.0"
    }
  }
}

provider "acme" {
  server_url = "https://acme-v02.api.letsencrypt.org/directory"
}

resource "acme_registration" "reg" {
  email_address = "wanjohiryan33@gmail.com"
}

resource "tls_private_key" "relay" {
  algorithm   = "ECDSA"
  ecdsa_curve = "P256"
}

resource "acme_registration" "relay" {
  account_key_pem = tls_private_key.relay.private_key_pem
  email_address   = var.email
}

resource "acme_certificate" "relay" {
  account_key_pem           = acme_registration.relay.account_key_pem
  common_name               = "relay.${var.domain}"
  subject_alternative_names = ["*.relay.${var.domain}"]
  key_type                  = tls_private_key.relay.ecdsa_curve

  recursive_nameservers = ["8.8.8.8:53"]

  dns_challenge {
    provider = "route53"
  }
}

# New resources to save certificate and private key
resource "local_file" "cert_file" {
  content              = "${acme_certificate.relay.certificate_pem}${acme_certificate.relay.issuer_pem}"
  filename             = "${path.module}/relay_cert.crt"
  file_permission      = "0644"
  directory_permission = "0755"
}

resource "local_file" "key_file" {
  content              = acme_certificate.relay.private_key_pem
  filename             = "${path.module}/relay_key.key"
  file_permission      = "0600"
  directory_permission = "0755"
}

# Outputs for certificate and private key
output "certificate_pem" {
  value     = "${acme_certificate.relay.certificate_pem}${acme_certificate.relay.issuer_pem}"
  sensitive = true
}

output "private_key_pem" {
  value     = acme_certificate.relay.private_key_pem
  sensitive = true
}
