import { isPermanentStage } from "./stage";

export const vpc = isPermanentStage
  ? new sst.aws.Vpc("Vpc", {
      az: 1,
      nat: "managed",
    })
  : sst.aws.Vpc.get("Vpc", "vpc-086ccc4d99dcb1931");

export const cluster = new sst.aws.Cluster("Cluster", {
  vpc,
});