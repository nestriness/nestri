//Deploys the website to cloudflare pages under the domain nestri.io (redirects all requests to www.nestri.io to avoid duplicate content)
// const cloudflareAccountId = new sst.Secret("CloudflareAccountId");

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

export const outputs = {
    www: www.subdomain,
};