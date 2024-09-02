//Deploys the website to cloudflare pages under the domain nestri.io (redirects all requests to www.nestri.io to avoid duplicate content)

export const www = new cloudflare.PagesProject("www", {
    name: "nestri",
    accountId: "nestri",
    productionBranch: "main",
    buildConfig: {
        rootDir: "./apps/www",
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