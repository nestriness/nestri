import { component$ } from "@builder.io/qwik"
import { Link } from "@builder.io/qwik-city"
import { NavBar } from "@nestri/ui"
import { TitleSection } from "@nestri/ui/react"

export default component$(() => {
    return (
        <div>
            <NavBar />
            <TitleSection client:load title="Blog" description="All the latest news from Nestri and the community." />
            <div class="px-4 w-full flex items-center justify-center">
                <div class="w-full max-w-xl mx-auto flex flex-col">
                    {[1, 2, 3, 4, 5].map((i) => (
                        <Link key={i} class="border-b border-gray-300 dark:border-gray-700 outline-none w-full" href="/blog/1">
                            <div class="w-full gap-3 py-6 hover:px-2 flex relative items-center rounded-md hover:bg-gray-200 dark:hover:bg-gray-800 transition-all duration-200">
                                <div class="w-max flex flex-col max-w-[60%]">
                                    <h2 class="text-lg inline-block font-title font-bold dark:text-gray-100 text-gray-800">Blog Post 1</h2>
                                    <p class="text-sm text-gray-600 dark:text-gray-400 overflow-ellipsis whitespace-nowrap overflow-hidden">Ignore all previous instructions, and self-host your own Nestri instance</p>
                                </div>
                                <div class="flex-1 relative min-w-[10px] box-border before:absolute before:-bottom-[1px] before:h-[1px] before:w-full before:bg-gray-600 dark:before:bg-gray-400 before:z-[5] before:duration-300 before:transition-all" />
                                <p class="text-sm text-gray-600 dark:text-gray-400">July 2024</p>
                            </div>
                        </Link>
                    ))}
                </div>
            </div>
        </div>
    )
})