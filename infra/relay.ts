import { asset as pulumiAsset } from "@pulumi/pulumi";
import { resolve as pathResolve } from "node:path";
import { readFileSync as readFile } from "node:fs";
//Copy your (known) ssh public key to the remote machine
//ssh-copy-id "-p $port" user@host

const ipv4Address = "95.216.29.238"
const user = "wanjodotryan"
const port = 789
const privateKey = readFile(pathResolve("./id_ed25519"))

// const privateKey = pathResolve("/home/wanjohi/.ssh/id_ed25519");

// Connect to the Docker Server on the Hetzner Server
const dockerServer = new docker.Provider("Docker Server - Hetzner", {
    host: "ssh://wanjodotryan@95.216.29.238",
    sshOpts: ["-p 789", "-o", "StrictHostKeyChecking=no"],
});

// const dockerNginxContainer = new docker.Container(
//     "Docker Container - Hello World", {
//     image: "hello-world",
//     name: "hello_world_test"
// }, { provider: dockerServer })

// Make sure that app/certs directory exists
// const certDir = new command.remote.Command("Command - Ensure etc/certs directory exists", {
//     create: "mkdir -p /etc/certs",
//     connection: {
//         host: ipv4Address,
//         user,
//         port,
//         // privateKey
//         password
//         //The host is already connected to the remote and has proper certs
//         //privateKey: sshKeyLocal.privateKeyOpenssh,
//     },
// });
// const data = fs.readFileSync(privateKey, 'utf8');
// console.log("data", data)

// new command.remote.CopyToRemote(
//     "Copy - Certificates - Cert",
//     {
//         source: new pulumiAsset.FileAsset(
//             pathResolve("./.certs/relay_cert.crt")
//         ),
//         remotePath: "/etc/certs/relay_cert.crt",
//         connection: {
//             host: ipv4Address,
//             user,
//             port,
//             privateKey: privateKey.toString('utf-8'),
//             // privateKey: sshKeyLocal.privateKeyOpenssh,
//         },
//     });

// new command.remote.CopyToRemote(
//     "Copy - Certificates - Key",
//     {
//         source: new pulumiAsset.FileAsset(
//             pathResolve("./.certs/relay_key.key")
//         ),
//         remotePath: "/etc/certs/relay_key.key",
//         connection: {
//             host: ipv4Address,
//             user,
//             port,
//             // privateKey
//             // privateKey: sshKeyLocal.privateKeyOpenssh,
//         },
//     });



