import { Hono } from 'hono'
import { Resvg } from "@cf-wasm/resvg";
import { generateGradient, createAvatarSvg } from '../utils';
import { kvCaches } from "@nestri/cache"

const cacheOptions = {
    key: "nexus",
    namespace: "avatar-cache"
};

const app = new Hono()

const middleware = kvCaches(cacheOptions);

app.get('/:id', middleware, async (c) => {
    const id = c.req.param('id');
    const [name, fileType] = id.split('.');
    const size = parseInt(c.req.query("size") || "200");

    const validImageTypes = ["png"] //['jpg', 'png', 'webp', 'avif'];
    if (!validImageTypes.includes(fileType)) {
        return c.text('Invalid image type', 400);
    }

    const gradient = generateGradient(name || Math.random() + "");
    // console.log(`gradient: ${JSON.stringify(gradient)}`)
    const svg = createAvatarSvg(size, gradient, fileType);
    const resvg = await Resvg.create(svg.toString());
    const pngData = resvg.render()
    const pngBuffer = pngData.asPng()

    return c.newResponse(pngBuffer, 200, {
        "Content-Type": `image/${fileType}`,
        'Cache-Control': 'public, max-age=31536000, immutable'
    })
})

export default app