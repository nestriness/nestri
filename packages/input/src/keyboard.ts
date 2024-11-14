import {type Input} from "./types"
import type PartySocket from "partysocket";
import {keyCodeToLinuxEventCode} from "./codes"

interface Props {
    ws: PartySocket;
    canvas: HTMLCanvasElement;
}

export class Keyboard {
    protected websocket: PartySocket;
    protected canvas: HTMLCanvasElement;
    protected connected!: boolean;

    // Store references to event listeners
    private keydownListener: (e: KeyboardEvent) => void;
    private keyupListener: (e: KeyboardEvent) => void;

    constructor({ws, canvas}: Props) {
        this.websocket = ws;
        this.canvas = canvas;
        this.keydownListener = this.createKeyboardListener("keydown", (e: any) => ({
            type: "KeyDown",
            key: this.keyToVirtualKeyCode(e.code)
        }));
        this.keyupListener = this.createKeyboardListener("keyup", (e: any) => ({
            type: "KeyUp",
            key: this.keyToVirtualKeyCode(e.code)
        }));
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
            document.addEventListener("keydown", this.keydownListener);
            document.addEventListener("keyup", this.keyupListener);
        } else {
            if (this.connected) {
                this.stop()
            }
        }

    }

    private stop() {
        document.removeEventListener("keydown", this.keydownListener);
        document.removeEventListener("keyup", this.keyupListener);
        this.connected = false;
    }

    // Helper function to create and return mouse listeners
    private createKeyboardListener(type: string, dataCreator: (e: Event) => Partial<Input>): (e: Event) => void {
        return (e: Event) => {
            e.preventDefault();
            e.stopPropagation();
            const data = dataCreator(e as any); // type assertion because of the way dataCreator is used
            this.websocket.send(JSON.stringify({...data, type} as Input));
        };
    }

    public dispose() {
        document.exitPointerLock();
        this.stop();
        this.connected = false;
    }

    private keyToVirtualKeyCode(code: string) {
        return keyCodeToLinuxEventCode[code] || undefined;
    }
}