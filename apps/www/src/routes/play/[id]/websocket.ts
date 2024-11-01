import { Mouse, Keyboard } from "@nestri/input"

type Props = {
    url: string;
    canvas: HTMLCanvasElement;
}
export class WS {
    private retryConnecting: boolean;
    private websocket: WebSocket;
    private canvas: HTMLCanvasElement;
    private retryInterval: NodeJS.Timeout | undefined;
    private url: string;
    private pointerLockListener: (() => void) | undefined;
    private mouse: Mouse | undefined;
    private keyboard: Keyboard | undefined;

    constructor({ url, canvas }: Props) {
        this.url = url;
        this.retryConnecting = false;
        this.websocket = new WebSocket(url);
        this.canvas = canvas;

        this.initializeWebSocket();
    }

    private initializeWebSocket() {
        this.websocket.onopen = () => {
            this.retryConnecting = false;
            this.cleanupInput();
            this.run();
            console.log("[input]: WebSocket connection opened");
        };

        this.websocket.onmessage = (evt) => {
            this.retryConnecting = false;
            console.log("[input]: Received a WebSocket message", evt.data);
        };

        this.websocket.onclose = () => {
            this.retryConnecting = true;
            console.log("[input]: WebSocket connection closed, retrying");
            this.retry();
        };

        this.websocket.onerror = (err) => {
            this.retryConnecting = true;
            console.log("[input]: WebSocket connection errored out", err);
            this.retry();
        };
    }

    private run() {
        if (!this.retryConnecting) {
            //Clear any retry intervals
            this.retryInterval && clearInterval(this.retryInterval);

            // Add pointer lock change listener only once
            if (!this.pointerLockListener) {
                this.pointerLockListener = () => {
                    if (!this.canvas) return;
                    if (document.pointerLockElement)
                        this.initializeInput();
                    else
                        this.cleanupInput();
                };

                document.addEventListener("pointerlockchange", this.pointerLockListener);
            }
        } else {
            this.retry();
        }
    }

    private retry() {
        this.retryInterval && clearInterval(this.retryInterval);

        if (this.retryConnecting) {
            console.log("[input]: Hang tight, we are trying to reconnect to the server :)");
            this.retryInterval = setInterval(() => {
                this.websocket = new WebSocket(this.url);
                this.initializeWebSocket();
            }, 5000); // Retry every 5 seconds
        }
    }

    // Optional: Clean up method to remove listeners
    private cleanup() {
        if (this.pointerLockListener) {
            document.removeEventListener("pointerlockchange", this.pointerLockListener);
            this.pointerLockListener = undefined;
        }
    }

    private initializeInput() {
        if (this.canvas && !this.mouse && !this.keyboard) {
            this.mouse = new Mouse({ ws: this.websocket, canvas: this.canvas });
            this.keyboard = new Keyboard({ ws: this.websocket, canvas: this.canvas });
        }
    }

    private cleanupInput() {
        if (this.mouse) {
            this.mouse.dispose();
            this.mouse = undefined;
        }
        if (this.keyboard) {
            this.keyboard.dispose();
            this.keyboard = undefined;
        }
    }
}