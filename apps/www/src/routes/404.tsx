import { component$ } from "@builder.io/qwik";
import { Link } from "@builder.io/qwik-city";
import { buttonVariants, cn } from "@nestri/ui/design";

export default component$(() => {
    return (
        <div class="flex flex-col w-screen select-none items-center justify-center gap-4 h-screen">
            <h1 class="font-bold text-[200px] text-primary-200 dark:text-primary-800 transition-all duration-200 [-webkit-text-stroke-color:theme(colors.primary.500)] [-webkit-text-stroke-width:4px] leading-[1em]">404</h1>
            <p class=" text-3xl font-medium max-w-lg text-center mx-auto">Whoops! The page you are looking for does not exist</p>
            <Link href="/" class={cn(buttonVariants.solid({ intent: "gray", size: "lg" }), "rounded-xl")} >Go home</Link>
        </div>
    );
});