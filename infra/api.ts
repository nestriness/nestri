import { isPermanentStage } from "./stage";

//TODO: Use this instead of wrangler
// export const api = new sst.cloudflare.Worker("apiApi", {
//     url: true,
//     handler: "packages/api/src/index.ts",
//     // live: true, 
// });

if (!isPermanentStage) {
    new sst.x.DevCommand("apiDev", {
        dev: {
            command: "bun run dev",
            directory: "packages/api",
            autostart: true,
        },
    })
}

export const outputs = {
    api: api.url
}