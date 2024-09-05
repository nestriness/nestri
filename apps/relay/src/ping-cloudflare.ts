export const handler = async () => {
    fetch("https://www.cloudflare.com/ips-v4")
    return {
        statusCode: 200,
        body: "Pinged Cloudflare",
    };
};