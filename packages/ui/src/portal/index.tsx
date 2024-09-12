import { $, component$, useSignal, useVisibleTask$ } from "@builder.io/qwik";
import portal from "./play";

// interface PortalProps {
//     onComplete$: QRL<() => void>;
// }

export default component$(() => {
    const buttonRef = useSignal<HTMLCanvasElement | undefined>();
    const iconRef = useSignal<HTMLCanvasElement | undefined>();
    const imagesLoaded = useSignal(false);
    // const isPlaying = useSignal(false);

    const imageUrls = [
        portal.assets.button_assets["intro"].image,
        portal.assets.button_assets["idle"].image,
        portal.assets.icon_assets["exit"].image,
        portal.assets.icon_assets["intro"].image,
        portal.assets.icon_assets["loop"].image
    ];

    const loadImages = $(() => {
        return Promise.all(imageUrls.map(url => {
            return new Promise((resolve, reject) => {
                const img = new Image();
                img.onload = () => resolve(img);
                img.onerror = reject;
                img.src = url;
            });
        }));
    })

    // eslint-disable-next-line qwik/no-use-visible-task
    useVisibleTask$(async ({ track }) => {
        track(() => imagesLoaded.value);

        if (buttonRef.value && iconRef.value) {
            const [introImg, idleImg, exitImg, , loopImg] = await loadImages();

            await portal.button(buttonRef.value, "intro", false, introImg as HTMLImageElement);
            await portal.icon(iconRef.value, "exit", false, exitImg as HTMLImageElement, false);
            await portal.button(buttonRef.value, "idle", true, idleImg as HTMLImageElement, 3);

            // Intro and loop animation
            await Promise.all([
                (async () => {
                    if (iconRef.value) {
                        await portal.icon(iconRef.value, "loop", false, loopImg as HTMLImageElement, true);
                        await portal.icon(iconRef.value, "loop", false, loopImg as HTMLImageElement, true);
                        await portal.icon(iconRef.value, "exit", false, exitImg as HTMLImageElement, true);
                    }
                })(),
                portal.button(buttonRef.value, "idle", true, idleImg as HTMLImageElement, 2),
            ]);
        }
    });


    return (
        <button class="relative rounded-full size-[100px] outline-none focus:ring-2 focus:ring-primary-500 hover:ring-2 hover:ring-primary-500 transition-all duration-200">
            <canvas class="absolute w-full h-full left-0 top-0 bottom-0 right-0" height={100} width={100} ref={buttonRef} />
            <canvas class="relative w-full h-full z-[5] left-0 top-0 bottom-0 right-0" height={100} width={100} ref={iconRef} />
        </button>
    )
})