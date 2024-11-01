import { type Input } from "./types"

interface Props {
    ws: WebSocket;
    canvas: HTMLCanvasElement;
}
//FIXME: removeEventListener does not work, i dunno why
//FIXME: For some reason, multiple eventlisteners are being attached, causing a multitude of keys being evoked
export class Keyboard {
    protected websocket: WebSocket;
    protected canvas: HTMLCanvasElement;
    protected connected!: boolean;
    protected abortController: AbortController;

    // Store references to event listeners
    private keydownListener: (e: KeyboardEvent) => void;
    private keyupListener: (e: KeyboardEvent) => void;

    constructor({ ws, canvas }: Props) {
        this.websocket = ws;
        this.canvas = canvas;
        this.abortController = new AbortController();
        this.keydownListener = this.createKeyboardListener("keydown", (e: any) => ({ type: "KeyDown", key: this.keyToVirtualKeyCode(e.code) }));
        this.keyupListener = this.createKeyboardListener("keyup", (e: any) => ({ type: "KeyUp", key: this.keyToVirtualKeyCode(e.code) }));
        this.run()
    }

    private run() {
        //calls all the other functions
        if (!document.pointerLockElement) {
            if (this.connected) {
                this.stop()
            }
            return;
        }

        if (document.pointerLockElement == this.canvas) {
            this.connected = true
            document.addEventListener("keydown", this.keydownListener, { signal: this.abortController.signal });
            document.addEventListener("keyup", this.keyupListener, { signal: this.abortController.signal });
        } else {
            if (this.connected) {
                this.stop()
            }
        }

    }
    private stop() {
        this.canvas.removeEventListener("keydown", this.keydownListener);
        this.canvas.removeEventListener("keyup", this.keyupListener);
        this.abortController.abort();
        this.connected = false;
    }

    // Helper function to create and return mouse listeners
    private createKeyboardListener(type: string, dataCreator: (e: Event) => Partial<Input>): (e: Event) => void {
        return (e: Event) => {
            e.preventDefault();
            e.stopPropagation();
            const data = dataCreator(e as any); // type assertion because of the way dataCreator is used
            this.websocket.send(JSON.stringify({ ...data, type } as Input));
        };
    }

    public dispose() {
        document.exitPointerLock();
        this.stop();
        this.abortController.abort();
        this.connected = false;
    }

    private keyToVirtualKeyCode(code: string) {

        const keyToVirtualKeyCodeMap = new Map([
            // ASCII
            ['KeyA', 0x41], ['KeyB', 0x42], ['KeyC', 0x43], ['KeyD', 0x44], ['KeyE', 0x45],
            ['KeyF', 0x46], ['KeyG', 0x47], ['KeyH', 0x48], ['KeyI', 0x49], ['KeyJ', 0x4A],
            ['KeyK', 0x4B], ['KeyL', 0x4C], ['KeyM', 0x4D], ['KeyN', 0x4E], ['KeyO', 0x4F],
            ['KeyP', 0x50], ['KeyQ', 0x51], ['KeyR', 0x52], ['KeyS', 0x53], ['KeyT', 0x54],
            ['KeyU', 0x55], ['KeyV', 0x56], ['KeyW', 0x57], ['KeyX', 0x58], ['KeyY', 0x59],
            ['KeyZ', 0x5A],

            // Digits
            ['Digit0', 0x30], ['Digit1', 0x31], ['Digit2', 0x32], ['Digit3', 0x33], ['Digit4', 0x34],
            ['Digit5', 0x35], ['Digit6', 0x36], ['Digit7', 0x37], ['Digit8', 0x38], ['Digit9', 0x39],

            // Special
            ['Escape', 0x1B],
            ['Backspace', 0x08],
            ['Tab', 0x09],
            ['Enter', 0x0D],
            ['ShiftLeft', 0xA0],
            ['ShiftRight', 0xA1],
            ['ControlLeft', 0xA2],
            ['ControlRight', 0xA3],
            ['AltLeft', 0xA4],
            ['AltRight', 0xA5],
            ['Space', 0x20],
            ['PageUp', 0x21],
            ['PageDown', 0x22],
            ['End', 0x23],
            ['Home', 0x24],
            ['ArrowLeft', 0x25],
            ['ArrowUp', 0x26],
            ['ArrowRight', 0x27],
            ['ArrowDown', 0x28],
            ['Insert', 0x2D],
            ['Delete', 0x2E],
        ]);

        return keyToVirtualKeyCodeMap.get(code) || 0; // Default to 0 for unknown keys
    }
}