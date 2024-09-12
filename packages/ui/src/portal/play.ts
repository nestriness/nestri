import PlayButtonIntro from "./assets/play_button_intro.json"
import PlayButtonIdle from "./assets/play_button_idle.json"
import PlayIconIntro from "./assets/play_icon_intro.json"
import PlayIconLoop from "./assets/play_icon_loop.json"
import PlayIconExit from "./assets/play_icon_exit.json"

const button_assets = {
    intro: {
        image: "/portal/play_button_intro.png",
        json: PlayButtonIntro
    },
    idle: {
        image: "/portal/play_button_idle.png",
        json: PlayButtonIdle
    }
}

const icon_assets = {
    intro: {
        image: "/portal/play_icon_intro.png",
        json: PlayIconIntro
    },
    loop: {
        image: "/portal/play_icon_loop.png",
        json: PlayIconLoop
    },
    exit: {
        image: "/portal/play_icon_exit.png",
        json: PlayIconExit
    }
}

let currentFrame = 0;  // Define the current frame index and animation speed
const animationSpeed = 50; // Time in milliseconds between frames
const buttonQueue: (() => void)[] = []; // Queue to hold the pending button calls
let isButtonRunning = false; // Flag to track whether the button function is currently running

async function button(canvas: HTMLCanvasElement, type: "intro" | "idle", loop: boolean, image: HTMLImageElement, index?: number) {
    return new Promise<void>((resolve) => {
        const buttonTask = () => {
            // Get the canvas element
            const ctx = canvas.getContext('2d');

            // Load the JSON data
            const animationData = button_assets[type].json;

            // Play the animation
            const frames = animationData.frames;
            const totalFrames = frames.length;

            if (index) currentFrame = index;

            const targetDim = 100 //target dimensions of the output image (height, width)

            // Start the animation
            const updateFrame = () => {

                // Check if we have reached the last frame
                if (!loop && currentFrame === totalFrames - 1) {
                    // Animation has reached the last frame, stop playing
                    isButtonRunning = false;

                    // Resolve the Promise to indicate completion
                    resolve();
                    return null;
                }

                // Clear the canvas
                ctx?.clearRect(0, 0, canvas.width, canvas.height);

                // Get the current frame details
                const singleFrame = frames[currentFrame];
                const { frame, sourceSize: ss, rotated, spriteSourceSize: sss, trimmed } = singleFrame;

                canvas.width = targetDim;
                canvas.height = targetDim;
                canvas.style.borderRadius = `${ss.h / 2}px`

                const newSize = {
                    w: frame.w,
                    h: frame.h
                };

                const newPosition = {
                    x: 0,
                    y: 0
                };

                if (rotated) {
                    ctx?.save()
                    ctx?.translate(canvas.width / 2, canvas.height / 2)
                    ctx?.rotate(-Math.PI / 2);
                    ctx?.translate(-canvas.height / 2, -canvas.width / 2);

                    newSize.w = frame.h;
                    newSize.h = frame.w;
                }

                if (trimmed) {
                    newPosition.x = sss.x;
                    newPosition.y = sss.y;

                    if (rotated) {
                        newPosition.x = canvas.height - sss.h - sss.y;
                        newPosition.y = sss.x;
                    }
                }

                const scaleFactor = Math.min(targetDim / newSize.w, targetDim / newSize.h);
                const scaledWidth = newSize.w * scaleFactor;
                const scaledHeight = newSize.w * scaleFactor;

                // Calculate the center position to draw the resized image
                const x = (targetDim - scaledWidth) / 2;
                const y = (targetDim - scaledHeight) / 2;

                ctx?.drawImage(
                    image,
                    frame.x,
                    frame.y,
                    newSize.w,
                    newSize.h,
                    x,
                    y,
                    scaledWidth,
                    scaledHeight
                )


                if (rotated) {
                    ctx?.restore()
                }
                // Increment the frame index
                currentFrame = (currentFrame + 1) % totalFrames


                // Schedule the next frame update
                setTimeout(updateFrame, animationSpeed);
            };

            return updateFrame()
        }
        // Check if the button function is already running
        if (isButtonRunning) {
            // If running, add the button task to the queue
            buttonQueue.push(buttonTask);

        } else {
            // If not running, set the flag and execute the button task immediately
            isButtonRunning = true;
            buttonTask();
        }
    })
}

let currentIconFrame = 0;  // Define the current frame index and animation speed
const iconQueue: (() => void)[] = []; // Queue to hold the pending button calls
let isIconRunning = false; // Flag to track whether the button function is currently running

function icon(canvas: HTMLCanvasElement, type: "loop" | "intro" | "exit", loop: boolean, image: HTMLImageElement, play: boolean) {
    return new Promise<void>((resolve) => {
        const iconTask = () => {
            // Get the canvas element
            const ctx = canvas.getContext('2d');

            // Load the JSON data
            const animationData = icon_assets[type].json;

            // Load the image
            // const image = new Image();
            image.src = icon_assets[type].image; // Path to the sprite sheet image

            // Play the animation
            const frames = animationData.frames;
            const totalFrames = frames.length;

            if (!play) {
                currentIconFrame = totalFrames - 3
            } else { currentIconFrame = 0 }

            // Start the animation
            const updateFrame = () => {

                // Check if we have reached the last frame
                if (!loop && currentIconFrame === totalFrames - 1) {
                    // Animation has reached the last frame, stop playing
                    isIconRunning = false;

                    // Resolve the Promise to indicate completion
                    resolve();
                    return;
                }

                // Clear the canvas
                ctx?.clearRect(0, 0, canvas.width, canvas.height);

                // Get the current frame details
                const singleFrame = frames[currentIconFrame];
                const { frame, sourceSize: ss, rotated, spriteSourceSize: sss, trimmed } = singleFrame;

                canvas.width = ss.w;
                canvas.height = ss.h
                canvas.style.borderRadius = `${ss.h / 2}px`

                const newSize = {
                    w: frame.w,
                    h: frame.h
                };

                const newPosition = {
                    x: 0,
                    y: 0
                };

                if (rotated) {
                    ctx?.save()
                    ctx?.translate(canvas.width / 2, canvas.height / 2)
                    ctx?.rotate(-Math.PI / 2);
                    ctx?.translate(-canvas.height / 2, -canvas.width / 2);

                    newSize.w = frame.h;
                    newSize.h = frame.w;
                }

                if (trimmed) {
                    newPosition.x = sss.x;
                    newPosition.y = sss.y;

                    if (rotated) {
                        newPosition.x = canvas.height - sss.h - sss.y;
                        newPosition.y = sss.x;
                    }
                }

                ctx?.drawImage(
                    image,
                    frame.x,
                    frame.y,
                    newSize.w,
                    newSize.h,
                    newPosition.x,
                    newPosition.y,
                    newSize.w,
                    newSize.h
                )


                if (rotated) {
                    ctx?.restore()
                }
                // Increment the frame index
                currentIconFrame = (currentIconFrame + 1) % totalFrames


                // Schedule the next frame update
                if (!play) {
                    isIconRunning = false;

                    resolve();
                    return;
                }

                setTimeout(updateFrame, animationSpeed)
            };

            return updateFrame();
        }
        // Check if the icon function is already running
        if (isIconRunning) {
            // If running, add the button icon to the queue
            iconQueue.push(iconTask);
        } else {
            // If not running, set the flag and execute the button task immediately
            isIconRunning = true;
            iconTask();
        }
    })
}

const portal = { button, icon, assets: { button_assets, icon_assets } }
export default portal;