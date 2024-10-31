import { $, component$, QRL, useSignal, useVisibleTask$ } from "@builder.io/qwik"
import { Mouse } from "./mouse_input_handler"
import { Keyboard } from "./keyboard_input_handler"


export default component$(() => {
    const canvas = useSignal<HTMLCanvasElement>();
    const retryConnecting = useSignal(false)

    useVisibleTask$(({ track }) => {
        track(() => retryConnecting.value);

        function attemptConnection() {
            if (!canvas.value) return; // Ensure canvas is available
            // const ws = connectWebSocket(retryConnecting);
            const ws = new WebSocket("ws://127.0.0.1:8080/ws");

            ws.onopen = (ev) => {
                retryConnecting.value = false;
            };

            ws.onmessage = async (event) => {
                if (event.data) {
                    console.log("msg recieved", event.data);
                    retryConnecting.value = false;
                }
            };

            ws.onerror = (err) => {
                console.error("[input]: We got an error while handling the connection", err);
                retryConnecting.value = true;
            };

            ws.onclose = () => {
                console.warn("[input]: We lost connection to the server");
                retryConnecting.value = true
            };


            document.addEventListener("pointerlockchange", () => {
                if (!canvas.value) return;
                new Mouse({ ws, canvas: canvas.value });
                new Keyboard({ ws, canvas: canvas.value });
            })
        }

        attemptConnection();


        if (retryConnecting.value) {
            console.log("[input]: Hang tight we are trying to reconnect to the server :)")
            const retryInterval = setInterval(attemptConnection, 5000); // Retry every 5 seconds
            return () => { retryInterval && clearInterval(retryInterval) }
        }
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