import { resolve as pathResolve } from "node:path";
import { readFileSync as readFile } from "node:fs";
//Copy your (known) ssh public key to the remote machine
//ssh-copy-id "-p $port" user@host

const ipv4Address = "95.216.29.238"
const user = "wanjodotryan"
const port = 789
const privateKey = readFile(pathResolve("./id_ed25519"))

// Connect to the Docker Server on the Hetzner Server
const dockerServer = new docker.Provider("Docker Server - Hetzner", {
    host: "ssh://wanjodotryan@95.216.29.238",
    sshOpts: ["-p 789", "-i", privateKey.toString(), "-o", "StrictHostKeyChecking=no"],
});

//TODO: First generate the self-signed certs, then and only then start MoQ
// Setup Docker Networks
const dockerNetworkPublic = new docker.Network(
    "Docker Network - Public",
    { name: "relay_network_public" },
    { provider: dockerServer }
);
const dockerNetworkInternal = new docker.Network(
    "Docker Network - Internal",
    { name: "relay_network_internal" },
    { provider: dockerServer }
);

// Setup Docker Volumes
const dockerVolumeCerts = new docker.Volume(
    "Docker Volume - Certs",
    { name: "relay_volume_certs" },
    { provider: dockerServer }
);

// First git clone the kixelated/moq-rs
const certDir = new command.remote.Command("Command - Git clone kixelated/moq-rs", {
    // Git clone into moq
    create: "git clone https://github.com/kixelated/moq-rs moq",
    connection: {
        host: ipv4Address,
        user,
        port,
        privateKey: privateKey.toString()
    },
});

//This installs the Root certificates to our shared volume 'relay_volume_certs'
const certInstall = new command.remote.Command("Command - Docker generate certs", {
    create: "docker run --rm --name certs -v ${CAROOT:-.}:/work/caroot -e CAROOT='/work/caroot' -v relay_volume_certs:/etc/ssl/certs -v ./moq/dev/go.mod:/work/go.mod -v ./moq/dev/go.sum:/work/go.sum  -w /work golang:latest go run filippo.io/mkcert -install",
    logging: command.remote.Logging.StdoutAndStderr, //log everything
    connection: {
        host: ipv4Address,
        user,
        port,
        privateKey: privateKey.toString()
    },
}, { dependsOn: [dockerVolumeCerts] });

//Generate our self-signed certificates
const certGenerate = new command.remote.Command("Command - Docker generate certs", {
    // This will generate the necessary keys and save them as: cert -> ~/moq/dev/localhost.crt and key -> ~/moq/dev/localhost.key
    create: "docker run --rm --name cert-generate -v relay_volume_certs:/etc/ssl/certs -v ./moq/dev/:/work/ -w /work golang:latest go run filippo.io/mkcert -ecdsa -days 10 -cert-file localhost.crt -key-file localhost.key localhost 127.0.0.1 ::1",
    logging: command.remote.Logging.StdoutAndStderr, //log everything
    connection: {
        host: ipv4Address,
        user,
        port,
        privateKey: privateKey.toString()
    },
}, { dependsOn: [certInstall] });

