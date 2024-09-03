//Deploys the website to cloudflare pages under the domain nestri.io (redirects all requests to www.nestri.io to avoid duplicate content)

export const www = new cloudflare.PagesProject("www", {
    name: "nestri",
    accountId: "8405b2acb6746935b975bc2cfcb5c288",
    productionBranch: "main",
    buildConfig: {
        rootDir: "apps/www",
        buildCommand: "bun run build",
        destinationDir: "dist"
    },
    deploymentConfigs: {
        production: {
            compatibilityFlags: ["nodejs_compat"]
        },
        preview: {
            compatibilityFlags: ["nodejs_compat"]
        }
    },
    source: {
        type: "github",
        config: {
            owner: "nestriness",
            deploymentsEnabled: true,
            productionBranch: "main",
            repoName: "nestri",
            productionDeploymentEnabled: true,
            prCommentsEnabled: true,
        }
    }
});

//TODO: Maybe handle building Qwik ourselves? This prevents us from relying on CF too much, we are open-source anyway 🤷🏾‍♂️
//TODO: Add a local dev server for Qwik that can be linked with whatever we want
//TODO: Link the www PageRule with whatever we give to the local dev server

export const wwwDev = new sst.x.DevCommand("www", {
    dev: {
        command: "bun run dev",
        directory: "apps/www",
        autostart: true,
    },
})

// //This creates a resource that can be accessed by itself
// new sst.Linkable.wrap(cloudflare.PageRule, (resource) => ({
//     // these properties will be available when linked
//     properties: {
//         arn: resource.urn
//     }
// }))
// //And then you call your linkable resource like this:
// // const www = cloudflare.PageRule("www", {})

// //this creates a linkable resource that can be linked to other resources
// export const linkable2 = new sst.Linkable("ExistingResource", {
//     properties: {
//         arn: "arn:aws:s3:::nestri-website-artifacts-prod-nestri-io-01h70zg50qz5z"
//     },
//     include: [
//         sst.aws.permission({
//             actions: ["s3:*"],
//             resources: ["arn:aws:s3:::nestri-website-artifacts-prod-nestri-io-01h70zg50qz5z"]
//         }),
//         sst.cloudflare.binding({
//             type: "r2BucketBindings",
//             properties: {
//                 bucketName: "nestri-website-artifacts-prod-nestri-io-01h70zg50qz5z",
//             }
//         })
//     ]
// })

export const outputs = {
    www: www.subdomain,
};