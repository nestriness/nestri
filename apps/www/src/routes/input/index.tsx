import { component$, useSignal, useVisibleTask$ } from "@builder.io/qwik"
import { Mouse } from "./mouse_input_handler"
import { Keyboard } from "./keyboard_input_handler"

export default component$(() => {
    const canvas = useSignal<HTMLCanvasElement>()
    // const connected = useSignal(false);
    // const wsRef = useSignal<WebSocket | null>(null);

    useVisibleTask$(() => {
        const ws = new WebSocket("ws://127.0.0.1:8080/ws");
        // wsRef.value = ws

        ws.onopen = (ev) => {
            console.log("ws opened")
            //Send auth JWT here
            // connected.value = true
        }

        ws.onmessage = async (event) => {
            if (event.data) {
                console.log("msg recieved", event.data);
                // connected.value = true
            }
        }

        ws.onerror = (err) => {
            console.error("Error handling the websocket connection", err)
        }

        ws.onclose = () => {
            console.warn("Websocket connection closed")
        }

        document.addEventListener("pointerlockchange", () => {
            if (!canvas.value) return;
            new Mouse({ ws, canvas: canvas.value });
            new Keyboard({ ws, canvas: canvas.value });
        })

    })

    return (
        <canvas
            ref={canvas}
            onClick$={() => {
                if (canvas.value) {
                    canvas.value.requestPointerLock()
                }
            }}
        />
    )
})