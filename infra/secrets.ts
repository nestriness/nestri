export const secret = {
    CloudflareAccountIdSecret: new sst.Secret("CloudflareAccountId"),
};

export const allSecrets = Object.values(secret);