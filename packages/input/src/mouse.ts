import { type Input } from "./types"
import type PartySocket from "partysocket";
import { mouseButtonToLinuxEventCode } from "./codes"
import { WebRTCStream } from "./webrtc-stream";

interface Props {
    webrtc: WebRTCStream;
    canvas: HTMLCanvasElement;
}
export class Mouse {
    protected wrtc: WebRTCStream;
    protected canvas: HTMLCanvasElement;
    protected connected!: boolean;

    private absX: number = 0;
    private absY: number = 0;

    // Store references to event listeners
    private mousemoveListener: (e: MouseEvent) => void;
    private mousedownListener: (e: MouseEvent) => void;
    private mouseupListener: (e: MouseEvent) => void;
    private mousewheelListener: (e: WheelEvent) => void;

    constructor({ webrtc, canvas }: Props, absoluteTrick?: boolean) {
        this.wrtc = webrtc;
        this.canvas = canvas;
        if (!absoluteTrick) {
            this.mousemoveListener = this.createMouseListener("mousemove", (e: any) => ({
                type: "MouseMove",
                x: e.movementX,
                y: e.movementY
            }));
        } else {
            this.mousemoveListener = this.createMouseListener("mousemoveabs", (e: any) => ({
                type: "MouseMoveAbs",
                x: this.absX = this.absX + e.movementX,
                y: this.absY = this.absY + e.movementY
            }));

            this.absX = this.canvas.width / 2;
            this.absY = this.canvas.height / 2;
        }

        this.mousedownListener = this.createMouseListener("mousedown", (e: any) => ({
            type: "MouseKeyDown",
            key: this.keyToVirtualKeyCode(e.button)
        }));

        this.mouseupListener = this.createMouseListener("mouseup", (e: any) => ({
            type: "MouseKeyUp",
            key: this.keyToVirtualKeyCode(e.button)
        }));
        this.mousewheelListener = this.createMouseListener("wheel", (e: any) => ({
            type: "MouseWheel",
            x: e.deltaX,    
            y: e.deltaY
        }));

        this.run()
    }

    private run() {
        //calls all the other functions
        if (!document.pointerLockElement) {
            console.log("no pointerlock")
            if (this.connected) {
                this.stop()
            }
            return;
        }

        if (document.pointerLockElement == this.canvas) {
            this.connected = true
            this.canvas.addEventListener("mousemove", this.mousemoveListener);
            this.canvas.addEventListener("mousedown", this.mousedownListener);
            this.canvas.addEventListener("mouseup", this.mouseupListener);
            this.canvas.addEventListener("wheel", this.mousewheelListener);

        } else {
            if (this.connected) {
                this.stop()
            }
        }

    }

    private stop() {
        this.canvas.removeEventListener("mousemove", this.mousemoveListener);
        this.canvas.removeEventListener("mousedown", this.mousedownListener);
        this.canvas.removeEventListener("mouseup", this.mouseupListener);
        this.canvas.removeEventListener("wheel", this.mousewheelListener);
        this.connected = false;
    }

    // Helper function to create and return mouse listeners
    private createMouseListener(type: string, dataCreator: (e: Event) => Partial<Input>): (e: Event) => void {
        return (e: Event) => {
            e.preventDefault();
            e.stopPropagation();
            const data = dataCreator(e as any); // type assertion because of the way dataCreator is used
            this.wrtc.sendData(JSON.stringify({ ...data, type } as Input));
        };
    }

    public dispose() {
        document.exitPointerLock();
        this.stop();
        this.connected = false;
    }

    private keyToVirtualKeyCode(code: number) {
        return mouseButtonToLinuxEventCode[code] || undefined;
    }
}