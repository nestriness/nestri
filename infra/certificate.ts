import { relayDomain } from "./domain";
//Open until this issue is fixed https://github.com/pulumiverse/pulumi-acme/issues/79
export const zone = aws.route53.getZone({ name: "fst.so", privateZone: false });

export const privateKey = new tls.PrivateKey("privateKey", { algorithm: "ECDSA", ecdsaCurve: "P256" });

const provider = new acme.Provider("provider", {
    serverUrl: "https://acme-staging-v02.api.letsencrypt.org/directory",
});

export const reg = new acme.Registration("reg", {
    accountKeyPem: privateKey.privateKeyPem,
    emailAddress: "elviswanjohi47@gmail.com",
}, { provider });


export const certificate = new acme.Certificate("certificate", {
    accountKeyPem: reg.accountKeyPem,
    commonName: "fst.so",
    subjectAlternativeNames: ["*.fst.so"],
    keyType: "P256",
    recursiveNameservers: ["8.8.8.8:53"],
    dnsChallenges: [{
        provider: "route53",
        config: {
            AWS_HOSTED_ZONE_ID: zone.then(zone => zone.id),
        }
    }],
}, { provider });

export const output = {
    cert: $interpolate`${certificate.certificatePem}${certificate.issuerPem}`,
    key: $interpolate`${privateKey.privateKeyPem}`,
}