import { component$, useSignal, useVisibleTask$, $ } from "@builder.io/qwik";

type Props = {
    game: {
        name: string;
        id: number;
    }
}

export const Card = component$(({ game }: Props) => {
    const imageUrl = `http://localhost:8787/image/cover/${game.id}.avif`
    const backgroundColor = useSignal<string | undefined>(undefined);
    const imgRef = useSignal<HTMLImageElement>();

    const extractColor = $((img: HTMLImageElement) => {
        const canvas = document.createElement('canvas');
        const ctx = canvas.getContext('2d');
        if (!ctx) return "rgb(200,200,200)"; // Fallback light gray

        canvas.width = img.naturalWidth;
        canvas.height = img.naturalHeight;
        ctx.drawImage(img, 0, 0, img.naturalWidth, img.naturalHeight);

        const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height).data;
        let r = 0, g = 0, b = 0, count = 0;

        for (let i = 0; i < imageData.length; i += 4) {
            // Only consider brighter pixels
            if (imageData[i] > 150 || imageData[i + 1] > 150 || imageData[i + 2] > 150) {
                r += imageData[i];
                g += imageData[i + 1];
                b += imageData[i + 2];
                count++;
            }
        }

        if (count === 0) return "rgb(200,200,200)"; // Fallback if no bright pixels found

        r = Math.floor(r / count);
        g = Math.floor(g / count);
        b = Math.floor(b / count);


        // For light mode, keep the existing logic
        const minBrightness = 100;
        r = Math.max(r, minBrightness);
        g = Math.max(g, minBrightness);
        b = Math.max(b, minBrightness);


        return `rgb(${r},${g},${b})`;
    });

    useVisibleTask$(async ({ track }) => {
        track(() => imgRef.value);

        const img = imgRef.value;
        if (img) {
            const processImage = async () => {
                backgroundColor.value = await extractColor(img);
            };

            if (img.complete) {
                await processImage();
            } else {
                img.onload = processImage;
            }
        }
    });

    return (
        <div
            style={{
                "--bg-color": backgroundColor.value,
            }}
            class="min-w-[250px] bg-[--bg-color] group cursor-pointer backdrop-blur-sm select-none w-full group rounded-3xl text-primary-950/70 duration-200 transition-colors flex flex-col">
            <header class="flex gap-4 justify-between p-4">
                <div class="flex relative pr-[22px] overflow-hidden overflow-ellipsis whitespace-nowrap" >
                    <h3 class="overflow-hidden overflow-ellipsis whitespace-nowrap">{game.name}</h3>
                </div>
            </header>
            <section class="flex justify-center items-center w-full py-7">
                <img
                    ref={imgRef}
                    src={imageUrl}
                    class="rounded-2xl ring-2 aspect-[2/3] w-full max-w-[90%] ring-gray-900/70 group-hover:scale-105 duration-200 transition-transform shadow-2xl shadow-gray-900"
                    width={250}
                    height={200}
                    alt={game.name}
                    crossOrigin="anonymous"
                />
            </section>
        </div>
    )
});