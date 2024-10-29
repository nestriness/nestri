import { component$, useSignal, useVisibleTask$ } from "@builder.io/qwik";
import { useLocation } from "@builder.io/qwik-city";

// Upstream MoQ lib does not work well with our Qwik Vite implementation
import { Player } from "@nestri/moq/playback"

export default component$(() => {
    const id = useLocation().params.id;
    const element = useSignal<HTMLCanvasElement>();
    const url = 'https://relay.fst.so'

    useVisibleTask$(
        async () => {
            if(element.value){
                await Player.create({ url, fingerprint: undefined, canvas: element.value, namespace: id });
            }
        }
    )

    return (
        <canvas
            ref={element}
            // onClick$={async () => {
            //     if (element.value) {
            //         await element.value.requestFullscreen()
            //         // Do not use - unadjustedMovement: true - breaks input on linux
            //         element.value.requestPointerLock();
            //     }
            // }}
            class="aspect-video w-full rounded-md" />
    )
})