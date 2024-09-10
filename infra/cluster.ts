import { isPermanentStage } from "./stage";

// export const vpc = isPermanentStage
//     ? new sst.aws.Vpc("Vpc", {
//         az: 1,
//         nat: "managed",
//     })
// : 

// export const vpc = new awsx.ec2.Vpc("nestri-vpc", {
//     numberOfAvailabilityZones: 1,
// });

// : sst.aws.Vpc.get("Vpc", "vpc-0ad569823770584ef");


// export const cluster2 = new sst.aws.Cluster("Cluster", {
//     vpc,
// });