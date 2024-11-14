import { component$, useSignal, useVisibleTask$ } from "@builder.io/qwik";
import { useLocation } from "@builder.io/qwik-city";
import PartySocket from "partysocket";
import {Keyboard, Mouse} from "@nestri/input"


export default component$(() => {
    const id = useLocation().params.id;
    const canvas = useSignal<HTMLCanvasElement>();

    useVisibleTask$(({ track }) => {
        track(() => canvas.value);

        const ws = new PartySocket({
            host: "ws://localhost:1999", // or localhost:1999 in dev
            room: id,
        })
        
        if (!canvas.value) return; // Ensure canvas is available
        
        document.addEventListener("pointerlockchange",()=>{
            if (!canvas.value) return; // Ensure canvas is available
            if (document.pointerLockElement){
            new Mouse({canvas: canvas.value, ws})
            new Keyboard({canvas: canvas.value, ws})
        }
        })
        
        ws.onmessage = (msg) => {
            console.log(msg.data)
        }   
    })

    return (
        <canvas
            ref={canvas}
            onClick$={async () => {
                if (canvas.value) {
                    // await element.value.requestFullscreen()
                    // Do not use - unadjustedMovement: true - breaks input on linux
                    canvas.value.requestPointerLock();
                }
            }}
            class="aspect-video h-screen rounded-sm" />
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

{/* <div class="loadingPopout_a8c724" role="dialog" tabindex="-1" aria-modal="true"><div class="spinner_b6db20 spinningCircle_b6db20" role="img" aria-label="Loading"><div class="spinningCircleInner_b6db20 inner_b6db20"><svg class="circular_b6db20" viewBox="25 25 50 50"><circle class="path_b6db20 path3_b6db20" cx="50" cy="50" r="20"></circle><circle class="path_b6db20 path2_b6db20" cx="50" cy="50" r="20"></circle><circle class="path_b6db20" cx="50" cy="50" r="20"></circle></svg></div></div></div> */ }
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


// function throttle(func, limit) {
//     let inThrottle;
//     return function(...args) {
//         if (!inThrottle) {
//             func.apply(this, args);
//             inThrottle = true;
//             setTimeout(() => inThrottle = false, limit);
//         }
//     }
// }

// // Use it like this:
// const throttledMouseMove = throttle((x, y) => {
//     websocket.send(JSON.stringify({
//         type: 'mousemove',
//         x: x,
//         y: y
//     }));
// }, 16); // ~60fps

// use std::time::Instant;

// // Add these to your AppState
// struct AppState {
//     pipeline: Arc<Mutex<gst::Pipeline>>,
//     last_mouse_move: Arc<Mutex<(i32, i32, Instant)>>, // Add this
// }

// // Then in your MouseMove handler:
// InputMessage::MouseMove { x, y } => {
//     let mut last_move = state.last_mouse_move.lock().unwrap();
//     let now = Instant::now();
    
//     // Only process if coordinates are different or enough time has passed
//     if (last_move.0 != x || last_move.1 != y) && 
//        (now.duration_since(last_move.2).as_millis() > 16) { // ~60fps
        
//         println!("Mouse moved to x: {}, y: {}", x, y);
        
//         let structure = gst::Structure::builder("MouseMoveRelative")
//             .field("pointer_x", x as f64)
//             .field("pointer_y", y as f64)
//             .build();

//         let event = gst::event::CustomUpstream::new(structure);
//         pipeline.send_event(event);
        
//         // Update last position and time
//         *last_move = (x, y, now);
//     }
// }
