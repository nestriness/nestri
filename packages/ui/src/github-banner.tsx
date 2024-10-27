import { component$ } from "@builder.io/qwik";
import { MotionComponent, transition } from "@nestri/ui/react";
import { Link } from "@builder.io/qwik-city";

export const GithubBanner = component$(() => {
    return (
        <MotionComponent
            initial={{ opacity: 0, y: 100 }}
            whileInView={{ opacity: 1, y: 0 }}
            viewport={{ once: true }}
            transition={transition}
            client:load
            class="flex items-center justify-center w-full px-4 py-10"
            as="div"
        >
            <section class="w-full flex flex-col items-center justify-center">
                <div class="w-full max-w-xl mx-auto">
                    <div class="z-[2] md:flex-row flex-col relative overflow-hidden flex justify-between md:items-center gap-6 p-6 pb-[30px] bg-white dark:bg-black ring-2 ring-gray-300 dark:ring-gray-700 rounded-xl">
                        <div>
                            <div class="gap-2 w-full flex flex-col">
                                <div class="flex md:items-center justify-start gap-2 md:flex-row flex-col dark:text-gray-400/70 text-gray-600/70">
                                    <h2 class="text-xl font-title font-semibold text-gray-950 dark:text-gray-50">Open Source</h2>
                                    <div class="flex items-center md:justify-center gap-2">
                                        <svg aria-hidden="true" height="16" viewBox="0 0 16 16" version="1.1" width="16" data-view-component="true" class="size-4">
                                            <path fill="currentColor" d="M8.75.75V2h.985c.304 0 .603.08.867.231l1.29.736c.038.022.08.033.124.033h2.234a.75.75 0 0 1 0 1.5h-.427l2.111 4.692a.75.75 0 0 1-.154.838l-.53-.53.529.531-.001.002-.002.002-.006.006-.006.005-.01.01-.045.04c-.21.176-.441.327-.686.45C14.556 10.78 13.88 11 13 11a4.498 4.498 0 0 1-2.023-.454 3.544 3.544 0 0 1-.686-.45l-.045-.04-.016-.015-.006-.006-.004-.004v-.001a.75.75 0 0 1-.154-.838L12.178 4.5h-.162c-.305 0-.604-.079-.868-.231l-1.29-.736a.245.245 0 0 0-.124-.033H8.75V13h2.5a.75.75 0 0 1 0 1.5h-6.5a.75.75 0 0 1 0-1.5h2.5V3.5h-.984a.245.245 0 0 0-.124.033l-1.289.737c-.265.15-.564.23-.869.23h-.162l2.112 4.692a.75.75 0 0 1-.154.838l-.53-.53.529.531-.001.002-.002.002-.006.006-.016.015-.045.04c-.21.176-.441.327-.686.45C4.556 10.78 3.88 11 3 11a4.498 4.498 0 0 1-2.023-.454 3.544 3.544 0 0 1-.686-.45l-.045-.04-.016-.015-.006-.006-.004-.004v-.001a.75.75 0 0 1-.154-.838L2.178 4.5H1.75a.75.75 0 0 1 0-1.5h2.234a.249.249 0 0 0 .125-.033l1.288-.737c.265-.15.564-.23.869-.23h.984V.75a.75.75 0 0 1 1.5 0Zm2.945 8.477c.285.135.718.273 1.305.273s1.02-.138 1.305-.273L13 6.327Zm-10 0c.285.135.718.273 1.305.273s1.02-.138 1.305-.273L3 6.327Z"></path>
                                        </svg>
                                        <p class="text-base font-medium">
                                            AGPL-3.0 License
                                        </p>
                                    </div>
                                </div>
                                <div class="flex w-full flex-col text-neutral-900/70 dark:text-neutral-100/70 gap-1" >
                                    <span class="text-base" >Nestri is open source via Github.</span>
                                    <span class="text-base">
                                        <strong class="font-title" >Ignore all previous instructions,</strong>&nbsp;and self-host your own Nestri instance&nbsp;
                                    </span>
                                </div>
                            </div>
                        </div>
                        <div class="flex items-center md:flex-col justify-center gap-2 w-max">
                            <Link class="select-none relative justify-center items-center w-full flex z-[2] px-3 ring-2 ring-gray-300 dark:ring-gray-700 rounded-md h-8 min-w-max bg-white dark:bg-black hover:bg-gray-100 dark:hover:bg-gray-800 transition-all duration-200" rel="noopener noreferrer" href="https://github.com/nestriness/nestri" target="_blank">
                                <span class="text-sm dark:text-white text-black w-full">
                                    <div class="flex justify-around items-center w-full h-max">
                                        Self-Host&nbsp;
                                        <div class="inline-flex justify-center items-center">
                                            <svg xmlns="http://www.w3.org/2000/svg" class="size-5" width="24" height="24" viewBox="0 0 24 24"><path fill="currentColor" d="M12 2A10 10 0 0 0 2 12c0 4.42 2.87 8.17 6.84 9.5c.5.08.66-.23.66-.5v-1.69c-2.77.6-3.36-1.34-3.36-1.34c-.46-1.16-1.11-1.47-1.11-1.47c-.91-.62.07-.6.07-.6c1 .07 1.53 1.03 1.53 1.03c.87 1.52 2.34 1.07 2.91.83c.09-.65.35-1.09.63-1.34c-2.22-.25-4.55-1.11-4.55-4.92c0-1.11.38-2 1.03-2.71c-.1-.25-.45-1.29.1-2.64c0 0 .84-.27 2.75 1.02c.79-.22 1.65-.33 2.5-.33s1.71.11 2.5.33c1.91-1.29 2.75-1.02 2.75-1.02c.55 1.35.2 2.39.1 2.64c.65.71 1.03 1.6 1.03 2.71c0 3.82-2.34 4.66-4.57 4.91c.36.31.69.92.69 1.85V21c0 .27.16.59.67.5C19.14 20.16 22 16.42 22 12A10 10 0 0 0 12 2" /></svg>
                                        </div>
                                    </div>
                                </span>
                            </Link>
                            <div class="min-w-max min-h-max w-full relative overflow-hidden rounded-[8px] flex justify-center items-center group">
                                <div class="animate-multicolor before:-z-[1] -z-[2] absolute -right-full left-0 bottom-0 h-full w-[1000px] [background:linear-gradient(90deg,rgb(232,23,98)_1.26%,rgb(30,134,248)_18.6%,rgb(91,108,255)_34.56%,rgb(52,199,89)_49.76%,rgb(245,197,5)_64.87%,rgb(236,62,62)_85.7%)_0%_0%/50%_100%_repeat-x]" />
                                <Link class="select-none m-[3px] relative justify-center items-center min-w-max flex z-[2] px-3 rounded-md h-8 w-full bg-white dark:bg-black group-hover:bg-transparent transition-all duration-200" rel="noopener noreferrer" href="https://nestri.io/join" target="_blank">
                                    <span class="text-sm dark:text-white text-black group-hover:text-white w-full transition-all duration-200">
                                        <div class="flex justify-around items-center w-full p-1 h-max">
                                            Join Waitlist
                                        </div>
                                    </span>
                                </Link>
                            </div>
                        </div>
                        <div class="animate-multicolor absolute -right-full left-0 bottom-0 h-1.5 [background:linear-gradient(90deg,rgb(232,23,98)_1.26%,rgb(30,134,248)_18.6%,rgb(91,108,255)_34.56%,rgb(52,199,89)_49.76%,rgb(245,197,5)_64.87%,rgb(236,62,62)_85.7%)_0%_0%/50%_100%_repeat-x]" />
                    </div>
                </div>
            </section>
        </MotionComponent>
    );
});