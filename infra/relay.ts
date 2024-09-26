import { resolve as pathResolve } from "node:path";
import { readFileSync as readFile } from "node:fs";
//Copy your (known) ssh public key to the remote machine
//ssh-copy-id "-p $port" user@host

const domain = "fst.so"
const ips = ["95.216.29.238"]

// Get the hosted zone
const zone = aws.route53.getZone({ name: domain });

// Create an A record
const record = new aws.route53.Record("Relay DNS Records", {
    zoneId: zone.then(zone => zone.zoneId),
    type: "A",
    name: `relay.${domain}`,
    ttl: 300,
    records: ips,
});

// Export the URL
export const url = $interpolate`https://${record.name}`;