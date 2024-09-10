// // import { cluster, vpc } from "./cluster";
// // import { execSync } from "child_process";
// import { relayDomain } from "./domain";

// //TODO: Use VPC peering to connect to clusters in different regions
// export const cluster = new aws.ecs.Cluster("relayCluster", {});

// export const lb = new awsx.lb.NetworkLoadBalancer("relayLoadBalancer", {});

// //TODO: Use Fargate Spot Instances
// export const service = new awsx.ecs.FargateService("relayFargateService", {
//     cluster: cluster.arn,
//     assignPublicIp: true,
//     desiredCount: 1,
//     taskDefinitionArgs: {
//         runtimePlatform: {
//             operatingSystemFamily: "LINUX",
//             cpuArchitecture: "ARM64", //Cheaper to run on ARM64
//         },
//         container: {
//             image: "nginx:latest",
//             name: "nginx",
//             cpu: 128,
//             memory: 512,
//             essential: true,
//             portMappings: [{
//                 containerPort: 80,
//                 protocol: "tcp",
//                 targetGroup: lb.defaultTargetGroup,
//             }],
//         },
//     },
// });

// export const zone = aws.route53.getZone({ name: "fst.so", privateZone: false });

// export const record = new aws.route53.Record("relayRecord", {
//     zoneId: zone.then(zone => zone.zoneId),
//     type: "A",
//     name: $interpolate`relay.${relayDomain}`,
//     aliases: [
//         {
//             name: lb.loadBalancer.dnsName,
//             zoneId: lb.loadBalancer.zoneId,
//             evaluateTargetHealth: true,
//         }
//     ]
// });

// export const outputs = {
//     relay: $interpolate`relay.${relayDomain}`,
// };
