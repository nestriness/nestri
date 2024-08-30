import { $, component$, useOnDocument, useSignal } from "@builder.io/qwik";
import { Link, useLocation } from "@builder.io/qwik-city";
import { buttonVariants, cn } from "@/design";

const navLinks = [
    {
        name: "Changelog",
        href: "/changelog"
    },
    {
        name: "Pricing",
        href: "/pricing"
    },
    {
        name: "Login",
        href: "/login"
    }
]

export const HomeNavBar = component$(() => {
    const location = useLocation()
    const hasScrolled = useSignal(false);

    useOnDocument(
        'scroll',
        $(() => {
            hasScrolled.value = window.scrollY > 0;
        })
    );

    return (
        <nav class={cn("sticky justify-between top-0 z-50 px-2 sm:px-6 text-xs sm:text-sm leading-[1] text-gray-800/70 dark:text-gray-200/70 h-[66px] dark:bg-gray-950/70 before:backdrop-blur-[15px] before:absolute before:-z-[1] before:top-0 before:left-0 before:w-full before:h-full flex items-center", hasScrolled.value && "shadow-[0_2px_20px_1px] shadow-gray-200 dark:shadow-gray-800")} >
            <div class="w-6 h-6 flex-shrink-0 md:mr-2">
                <svg
                    class="h-full w-full"
                    width="100%"
                    height="100%"
                    viewBox="0 0 12.8778 9.7377253"
                    version="1.1"
                    id="svg1"
                    xmlns="http://www.w3.org/2000/svg">
                    <path
                        d="m 2.093439,1.7855532 h 8.690922 V 2.2639978 H 2.093439 Z m 0,2.8440874 h 8.690922 V 5.1080848 H 2.093439 Z m 0,2.8440866 h 8.690922 V 7.952172 H 2.093439 Z"
                        style="font-size:12px;fill:#ff4f01;fill-opacity:1;fill-rule:evenodd;stroke:#ff4f01;stroke-width:1.66201;stroke-linecap:round;stroke-dasharray:none;stroke-opacity:1" />
                </svg>
            </div>
            <div class="relative flex items-center">
                <hr class="w-[1px] h-7 bg-gray-700/50 dark:bg-gray-300/50 mx-3 rotate-[16deg]" />
                <button class="rounded-full transition-all flex items-center duration-200 px-3 h-8 gap-2 select-none cursor-pointer hover:bg-gray-300/70 dark:hover:bg-gray-700/70" >
                    <img src="http://localhost:8787/image/avatar/the-avengers.png" height={16} width={16} class="size-4 rounded-full" alt="Avatar" />
                    <p class="whitespace-nowrap [text-overflow:ellipsis] overflow-hidden max-w-[20ch]">The Avengers</p>
                    <svg xmlns="http://www.w3.org/2000/svg" width={16} height={16} viewBox="0 0 21 21"><path fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" d="m7.5 8.5l3-3l3 3m-6 5l3 3l3-3" /></svg>
                </button>
            </div>
            {/* <div class="flex items-center mx-auto w-full h-full max-w-xl border-b-2 border-gray-950/50 dark:border-gray-950/50">

            </div> */}
            <button class="ml-auto rounded-full transition-all flex items-center duration-200 px-3 h-8 gap-1 select-none cursor-pointer hover:bg-gray-300/70 dark:hover:bg-gray-700/70" >
                <img src="http://localhost:8787/image/avatar/wanjohi.png" height={16} width={16} class="size-4 rounded-full" alt="Avatar" />
                <p class="whitespace-nowrap [text-overflow:ellipsis] overflow-hidden max-w-[20ch]">Wanjohi</p>
                <svg xmlns="http://www.w3.org/2000/svg" width={16} height={16} viewBox="0 0 21 21"><path fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" d="m7.5 8.5l3-3l3 3m-6 5l3 3l3-3" /></svg>
            </button>
        </nav>
    )
})