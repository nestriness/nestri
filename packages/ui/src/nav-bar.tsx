import { $, component$, useOnDocument, useSignal } from "@builder.io/qwik";
import { Link, useLocation } from "@builder.io/qwik-city";
import { buttonVariants, cn } from "./design";

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

export const NavBar = component$(() => {
    const location = useLocation()
    const hasScrolled = useSignal(false);

    useOnDocument(
        'scroll',
        $(() => {
            hasScrolled.value = window.scrollY > 0;
        })
    );

    return (
        <nav class={cn("sticky top-0 z-50 px-4 text-sm font-extrabold bg-gray-100/70 dark:bg-gray-900/70 before:backdrop-blur-[15px] before:absolute before:-z-[1] before:top-0 before:left-0 before:w-full before:h-full", hasScrolled.value && "shadow-[0_2px_20px_1px] shadow-gray-300 dark:shadow-gray-700")} >
            <div class="mx-auto flex max-w-xl items-center border-b-2 dark:border-gray-50/50 border-gray-950/50" >
                <Link class="outline-none transition-all duration-200 focus:ring-2 focus:ring-primary-500 p-1 rounded-md" href="/" >
                    <h1 class="text-lg font-title" >
                        Nestri
                    </h1>
                </Link>
                <ul class="ml-0 -mr-4 flex font-medium m-4 flex-1 gap-1 tracking-[0.035em] items-center justify-end dark:text-primary-50/70 text-primary-950/70">
                    {navLinks.map((linkItem, key) => (
                        <li key={`linkItem-${key}`}>
                            <Link href={linkItem.href} class={cn(buttonVariants.ghost({ intent: "gray", size: "sm" }), "hover:bg-gray-300/70 dark:hover:bg-gray-700/70 outline-none focus:ring-2 duration-200 transition-all focus:ring-primary-500", location.url.pathname === linkItem.href && "bg-gray-300/70 hover:bg-gray-300/70 dark:bg-gray-700/70 dark:hover:bg-gray-700/70")}>
                                {linkItem.name}
                            </Link>
                        </li>
                    ))}
                </ul>
            </div>
        </nav>
    )
})