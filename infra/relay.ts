// How it works:
// - The relay is a moq-rs docker server hosted on AWS that is used to transmit media to the user
// - The relay has a side car written in JS that pings cloudflare our API to "register" itself every 1 hour
// - The API has a route that returns the nearest relay to the user (we use KD trees to get nearest relays based on the user's location)
// - The API returns the relay URL to the user with the session ID, the expiry time of the session and the auth token to use to connect to the relay
// - The user connects to the relay and starts transmitting media or receiving media

// The user connects to the relay with the following URL:
// moq://{relay_url}/?token={auth_token}&session_id={session_id}&origin={origin}
import { cluster } from "./cluster";
import { execSync } from "child_process";
import { domain } from "./dns";

// sst.Linkable.wrap(tls.PrivateKey, (resource) => ({
//     properties: {
//         private: resource.privateKeyOpenssh,
//         public: resource.publicKeyOpenssh,
//     },
// }));

const privateKey = new tls.PrivateKey("privateKey", {
    algorithm: "ECDSA",
    ecdsaCurve: "P256",
});

//TODO: Generate a cert for each region

const reg = new acme.Registration("reg", {
    accountKeyPem: privateKey.privateKeyPem,
    emailAddress: "wanjohiryan33@gmail.com",
});

// const certificate = new acme.Certificate("certificate", {
//     accountKeyPem: reg.accountKeyPem,
//     commonName: $interpolate`relay.${domain}`,
//     subjectAlternativeNames: [$interpolate`*.relay.${domain}`],
//     keyType: "P256",
//     recursiveNameservers: ["8.8.8.8:53"],
//     dnsChallenges: [{
//     //TODO: Move this domain AGAIN, to AWS as this will be used specifically for the relay
//         provider: "cloudflare",
//         config: {
//             "CLOUDFLARE_API_TOKEN": process.env.CLOUDFLARE_API_TOKEN,
//         },
//     }],
// });


cluster.addService("Relay", {
    cpu: "1 vCPU",  
    memory: "2 GB",
    image: {
        context: "./apps/relay",
    },
    // link: [api, auth, secret.StripePublic, authFingerprintKey, key],
    environment: {
        VERSION: !$dev ? execSync("git rev-parse HEAD").toString().trim() : "",
        RUST_LOG: "info",
        RUST_BACKTRACE: "1",

    },
    public: {
        domain:
            $app.stage === "production"
                ? undefined
                : {
                    name: domain,
                    dns: sst.aws.dns(),
                },
        ports: [
            //We just need to expose a single port open, would you look at that :D
            { listen: "443/udp", forward: "4443/udp" },
        ],
    },
    scaling:
        $app.stage === "production"
            ? {
                min: 0,
                max: 10,
            }
            : undefined,
    transform: {
        service: {
            desiredCount: $app.stage === "production" ? 0 : 1,
        },
    },
    // This won't have a dev environment because it's a pain in the ass to set up
    // dev: {
    //     command: "cargo run --bin moq-relay",
    // },
});

//TODO: Add lambda function to ping cloudflare with the relay's public IP to register it
//TODO: Automatically get a certificate for the relay's domain from cloudflare or Let's Encrypt and pass it to the relay

new sst.aws.Cron("PingCloudflare", {
    schedule: "rate(1 hour)",
    // link: [relay],
    job: {
        handler: "./apps/relay/functions/ping-cloudflare.handler",
    }
});