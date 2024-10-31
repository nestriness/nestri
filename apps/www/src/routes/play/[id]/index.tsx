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

{/**
    .spinningCircleInner_b6db20 {
    transform: rotate(280deg);
}
.inner_b6db20 {
    position: relative;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 32px;
    height: 32px;
    contain: paint;
} */}

{/* <div class="loadingPopout_a8c724" role="dialog" tabindex="-1" aria-modal="true"><div class="spinner_b6db20 spinningCircle_b6db20" role="img" aria-label="Loading"><div class="spinningCircleInner_b6db20 inner_b6db20"><svg class="circular_b6db20" viewBox="25 25 50 50"><circle class="path_b6db20 path3_b6db20" cx="50" cy="50" r="20"></circle><circle class="path_b6db20 path2_b6db20" cx="50" cy="50" r="20"></circle><circle class="path_b6db20" cx="50" cy="50" r="20"></circle></svg></div></div></div> */}
// .loadingPopout_a8c724 {
//     background-color: var(--background-secondary);
//     display: flex;
//     justify-content: center;
//     padding: 8px;
// }

// .circular_b6db20 {
//     animation: spinner-spinning-circle-rotate_b6db20 2s linear infinite;
//     height: 100%;
//     width: 100%;
// }

// 100% {
//     transform: rotate(360deg);
// }


{/* .path3_b6db20 {
    animation-delay: .23s;
    stroke: var(--text-brand);
}
.path_b6db20 {
    animation: spinner-spinning-circle-dash_b6db20 2s ease-in-out infinite;
    stroke-dasharray: 1, 200;
    stroke-dashoffset: 0;
    fill: none;
    stroke-width: 6;
    stroke-miterlimit: 10;
    stroke-linecap: round;
    stroke: var(--brand-500);
}
circle[Attributes Style] {
    cx: 50;
    cy: 50;
    r: 20;
}
user agent stylesheet
:not(svg) {
    transform-origin: 0px 0px;
} */}



// .path2_b6db20 {
//     animation-delay: .15s;
//     stroke: var(--text-brand);
//     opacity: .6;
// }
// .path_b6db20 {
//     animation: spinner-spinning-circle-dash_b6db20 2s ease-in-out infinite;
//     stroke-dasharray: 1, 200;
//     stroke-dashoffset: 0;
//     fill: none;
//     stroke-width: 6;
//     stroke-miterlimit: 10;
//     stroke-linecap: round;
//     stroke: var(--brand-500);
// }
// circle[Attributes Style] {
//     cx: 50;
//     cy: 50;
//     r: 20;