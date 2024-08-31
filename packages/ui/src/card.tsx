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
    const ringColor = useSignal<string | undefined>(undefined);
    const imgRef = useSignal<HTMLImageElement>();

    // Function to extract dominant color
    const extractColor = $((img: HTMLImageElement) => {
        const canvas = document.createElement('canvas');
        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        canvas.width = img.naturalWidth;
        canvas.height = img.naturalHeight;
        ctx.drawImage(img, 0, 0, img.naturalWidth, img.naturalHeight);

        const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
        const data = imageData.data;

        let r = 0, g = 0, b = 0;

        for (let i = 0; i < data.length; i += 4) {
            r += data[i];
            g += data[i + 1];
            b += data[i + 2];
        }

        r = Math.floor(r / (data.length / 4));
        g = Math.floor(g / (data.length / 4));
        b = Math.floor(b / (data.length / 4));

        return `rgb(${r},${g},${b})`;
    });

    // Function to darken a color
    const darkenColor = $((color: string | undefined, amount: number) => {
        if (!color) return color;

        const rgb = color.match(/\d+/g);
        if (!rgb || rgb.length !== 3) return color;

        const darkenChannel = (channel: number) => Math.max(0, channel - amount);
        const r = darkenChannel(parseInt(rgb[0]));
        const g = darkenChannel(parseInt(rgb[1]));
        const b = darkenChannel(parseInt(rgb[2]));

        return `rgb(${r},${g},${b})`;
    });

    useVisibleTask$(async ({ track }) => {
        track(() => imgRef.value);

        const img = imgRef.value;
        if (img) {
            if (img.complete) {
                const extractedColor = await extractColor(img);
                backgroundColor.value = extractedColor;
                ringColor.value = await darkenColor(extractedColor, 30);
            } else {
                await new Promise<void>((resolve) => {
                    img.onload = async () => {
                        const extractedColor = await extractColor(img);
                        backgroundColor.value = extractedColor;
                        ringColor.value = await darkenColor(extractedColor, 30);
                        resolve();
                    };
                });
            }
        }
    });

    return (
        <div
            style={{
                backgroundColor: backgroundColor.value,
                "--tw-ring-color": ringColor.value
            }}
            class="bg-gray-200/70 min-w-[250px] backdrop-blur-sm ring-gray-300 select-none w-full group dark:ring-gray-700 ring dark:bg-gray-800/70 group rounded-3xl dark:text-primary-50/70 text-primary-950/70 duration-300 transition-colors flex flex-col">
            <header class="flex gap-4 justify-between p-4">
                <div
                    class="flex relative pr-[22px] overflow-hidden text-white overflow-ellipsis whitespace-nowrap" >
                    <h3 class="overflow-hidden overflow-ellipsis whitespace-nowrap">{game.name}</h3>
                </div>
            </header>
            <section class="flex justify-center items-center w-full py-7">
                <img
                    ref={imgRef}
                    src={imageUrl}
                    class="rounded-2xl shadow-2xl shadow-gray-900"
                    width={270}
                    height={215}
                    alt={game.name}
                    crossOrigin="anonymous"
                />
            </section>
        </div>
    )
});