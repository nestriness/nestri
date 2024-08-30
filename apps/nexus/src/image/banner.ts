import { Hono } from 'hono'
import { kvCaches } from "@nestri/cache"
import resize, { initResize } from '@jsquash/resize';
import decodeJpeg, { init as initDecodeJpeg } from '@jsquash/jpeg/decode';
import encodeAvif, { init as initEncodeAvif } from '@jsquash/avif/encode.js';
import JPEG_DEC_WASM from "@jsquash/jpeg/codec/dec/mozjpeg_dec.wasm";
import RESIZE_WASM from "@jsquash/resize/lib/resize/pkg/squoosh_resize_bg.wasm";
import AVIF_ENC_WASM from "@jsquash/avif/codec/enc/avif_enc.wasm";

const cacheOptions = {
    key: "nexus",
    namespace: "cover-cache"
};

const middleware = kvCaches(cacheOptions);

const decodeImage = async (buffer: ArrayBuffer) => {
    await initDecodeJpeg(JPEG_DEC_WASM);
    return decodeJpeg(buffer);
}

const resizeImage = async (image: { width: number; height: number }, width: number, height: number) => {
    await initResize(RESIZE_WASM);
    // Resize image with respect to aspect ratio
    const aspectRatio = image.width / image.height;
    const newWidth = width;
    const newHeight = width / aspectRatio;
    return resize(image, { width: newWidth, height: newHeight, fitMethod: "stretch" });
}

const encodeImage = async (image: { width: number; height: number }, format: string) => {
    if (format === 'avif') {
        await initEncodeAvif(AVIF_ENC_WASM);
        return encodeAvif(image);
    }
    throw new Error(`Unsupported image format: ${format}`);
}

const app = new Hono()

app.notFound((c) => c.json({ message: 'Not Found', ok: false }, 404))

app.get('/:id', middleware, async (c) => {
    const [gameId, imageType] = c.req.param("id").split('.');
    const width = parseInt(c.req.query("width") || "600");
    //We don't even use this, but let us keep it for future use
    const height = parseInt(c.req.query("height") || "900");
    if (!gameId || !imageType) {
        return c.text("Invalid image parameters", 400)
    }
    //Support Avif only because of it's small size
    const validImageTypes = ["avif"] //['jpg', 'png', 'webp', 'avif'];
    if (!validImageTypes.includes(imageType)) {
        return c.text('Invalid image type', 400);
    }

    const imageUrl = `https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/${gameId}/header.jpg`;
    const image = await fetch(imageUrl);
    if (!image.ok) {
        return c.text('Image not found', 404);
    }
    const imageBuffer = await image.arrayBuffer();
    const imageData = await decodeImage(imageBuffer);
    const resizedImage = await resizeImage(imageData, width, height);
    const resizedImageBuffer = await encodeImage(resizedImage, imageType);
    return c.newResponse(resizedImageBuffer, 200, {
        "Content-Type": `image/${imageType}`,
        'Cache-Control': 'public, max-age=31536000, immutable'
    })
})

export default app
