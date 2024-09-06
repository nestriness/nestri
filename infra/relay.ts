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

// import * as aws from "@pulumi/aws";
// import * as pulumi from "@pulumi/pulumi";

// // ... existing code ...

// // Define your relay instances
// const relays = [
//     { name: "relay-1", region: "us-east-1" },
//     { name: "relay-2", region: "eu-west-1" },
//     { name: "relay-3", region: "ap-southeast-1" },
//     // Add more relays as needed
// ];

// // Create Elastic IPs and EC2 instances for each relay
// const relayResources = relays.map(relay => {
//     const provider = new aws.Provider(`aws-${relay.region}`, { region: relay.region });
    
//     const eip = new aws.ec2.Eip(`relay-eip-${relay.name}`, {
//         vpc: true,
//         tags: { Name: `relay-${relay.name}` },
//     }, { provider });

//     // You'll need to define your EC2 instance configuration
//     const instance = new aws.ec2.Instance(`relay-instance-${relay.name}`, {
//         // ... instance configuration ...
//         tags: { Name: `relay-${relay.name}` },
//     }, { provider });

//     new aws.ec2.EipAssociation(`relay-eip-assoc-${relay.name}`, {
//         instanceId: instance.id,
//         allocationId: eip.id,
//     }, { provider });

//     return { eip, instance };
// });

// // Create health check for each relay
// const healthChecks = relayResources.map((resources, index) => 
//     new aws.route53.HealthCheck(`relay-health-${relays[index].name}`, {
//         fqdn: resources.eip.publicIp,
//         port: 443, // Adjust as needed
//         type: "HTTP",
//         resourcePath: "/health", // Adjust based on your health check endpoint
//         failureThreshold: 3,
//         requestInterval: 30,
//         measureLatency: true,
//         enableSni: true,
//         searchString: "OK", // Adjust based on your health check response
//     })
// );

// // Create latency-based routing records
// relayResources.forEach((resources, index) => {
//     new aws.route53.Record(`relay-latency-${relays[index].name}`, {
//         zoneId: hostedZone.zoneId,
//         name: `relay.${domain}`,
//         type: "A",
//         latencyRoutingPolicy: {
//             region: relays[index].region,
//         },
//         healthCheckId: healthChecks[index].id,
//         setIdentifier: `relay-${relays[index].region}`,
//         records: [resources.eip.publicIp],
//     });
// });

// const monitor = new cloudflare.LoadBalancerMonitor("relay-monitor", {
//     type: "http",
//     method: "GET",
//     path: "/health",
//     interval: 60,
//     timeout: 5,
//     retries: 2,
//     expectedCodes: "200",
//     description: "Health check for relay servers",
// });

// const pool = new cloudflare.LoadBalancerPool("relay-pool", {
//     // ... other configuration
//     monitor: monitor.id,
//     // ... rest of the configuration
// });

// Create a single Cloudflare Load Balancer Pool
// const pool = new cloudflare.LoadBalancerPool("relay-pool", {
//     name: "relay-pool",
//     origins: relays.map((relay, index) => ({
//         name: relay.name,
//         address: eips[index].publicIp,
//         enabled: true,
//         weight: 1,
//     })),
//     description: "Pool for all relay servers",
//     enabled: true,
//     minimumOrigins: 1,
//     monitor: "http://relay.example.com/health", // Replace with your health check endpoint
//     notificationEmail: "admin@example.com", // Replace with your email
// });

// // Create Cloudflare Load Balancer
// const loadBalancer = new cloudflare.LoadBalancer("relay-load-balancer", {
//     zoneId: cloudflareZoneId,
//     name: `relay.${domain}`,
//     defaultPoolIds: [pool.id],
//     fallbackPoolId: pool.id,
//     description: "Global load balancer for relay servers",
//     proxied: true,
//     steeringPolicy: "dynamic_latency",
//     // ... rest of the configuration
// });

// // ... existing code ...

// import * as aws from "@pulumi/aws";
// import * as pulumi from "@pulumi/pulumi";

// // ... existing code ...

// // Define your relay instances
// const relays = [
//     { name: "relay-1", region: "us-east-1" },
//     { name: "relay-2", region: "eu-west-1" },
//     { name: "relay-3", region: "ap-southeast-1" },
//     // Add more relays as needed
// ];

// // Create Elastic IPs for each relay
// const eips = relays.map(relay => 
//     new aws.ec2.Eip(`relay-eip-${relay.name}`, {
//         vpc: true,
//         tags: { Name: `relay-${relay.name}` },
//     }, { provider: new aws.Provider(`aws-${relay.region}`, { region: relay.region }) })
// );

// // Create a Route 53 Traffic Policy for Geoproximity routing
// const policy = new aws.route53.TrafficPolicy("relay-geoproximity-policy", {
//     name: "relay-geoproximity-policy",
//     document: pulumi.jsonStringify({
//         AWSPolicyFormatVersion: "2015-10-01",
//         RecordType: "A",
//         Endpoints: Object.fromEntries(relays.map((relay, index) => [
//             `relay-${relay.name}`,
//             {
//                 Type: "value",
//                 Value: eips[index].publicIp,
//             }
//         ])),
//         Rules: {
//             RuleType: "geoproximity",
//             GeoproximityLocations: relays.map((relay, index) => ({
//                 Region: relay.region,
//                 Bias: 0, // Adjust bias if needed
//                 EvaluateTargetHealth: false,
//                 EndpointReference: `relay-${relay.name}`,
//             })),
//         },
//     }),
// });

// // Create a Route 53 Traffic Policy Instance to apply the policy
// new aws.route53.TrafficPolicyInstance("relay-geoproximity-instance", {
//     name: `relay.${domain}`,
//     trafficPolicyId: policy.id,
//     trafficPolicyVersion: 1,
//     hostedZoneId: hostedZone.zoneId,
//     ttl: 60,
// });

// // ... existing code ...