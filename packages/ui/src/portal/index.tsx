import { $, component$, useSignal, useVisibleTask$ } from "@builder.io/qwik";
import portal, { PortalButton, PortalIcon } from "./play";

//TODO: Fix the portal animation does not restart when a new element renders it. What i mean is, if you click one games, then quit, then click on another, the animation won't show.
//FIXME: I dunno why the animation is not showing on the second game.

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

        // let isMounted = true;
        // cleanup(() => {
        //     isMounted = false;
        //     // Reset canvas states if necessary
        //     if (buttonRef.value) {
        //         const ctx = buttonRef.value.getContext('2d');
        //         ctx?.clearRect(0, 0, buttonRef.value.width, buttonRef.value.height);
        //     }
        //     if (iconRef.value) {
        //         const ctx = iconRef.value.getContext('2d');
        //         ctx?.clearRect(0, 0, iconRef.value.width, iconRef.value.height);
        //     }
        // });

        if (buttonRef.value && iconRef.value) {
            const [introImg, idleImg, exitImg, , loopImg] = await loadImages();

            const button = new PortalButton(buttonRef.value);
            const icon = new PortalIcon(iconRef.value)
            // if (!isMounted) return;

            await button.render("intro", false, introImg as HTMLImageElement);
            await icon.render("exit", false, exitImg as HTMLImageElement, false);
            await button.render("idle", true, idleImg as HTMLImageElement, 3);

            // Intro and loop animation
            await Promise.all([
                (async () => {
                    if (iconRef.value) {
                        await icon.render("loop", false, loopImg as HTMLImageElement, true);
                        await icon.render("loop", false, loopImg as HTMLImageElement, true);
                        await icon.render("exit", false, exitImg as HTMLImageElement, true);
                    }
                })(),
                button.render("idle", true, idleImg as HTMLImageElement, 2),
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