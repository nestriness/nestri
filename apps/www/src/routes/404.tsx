/* eslint-disable qwik/jsx-img */
import { component$ } from "@builder.io/qwik";
import { Link } from "@builder.io/qwik-city";

export default component$(() => {
    return (
        <div class="w-screen h-screen flex flex-col justify-center items-center gap-4">
            <div class="flex flex-col justify-center items-center text-center gap-4 w-full max-w-xl">
                <img src="/logo.webp" alt="Nestri Logo" class="w-16 sm:w-20 aspect-[30/23]" height={80} width={80} />
                <div class="font-title">
                    <h1 class="text-3xl sm:text-5xl font-bold font-title">Whoops!</h1>
                    <h2 class="text-3xl sm:text-5xl font-bold font-title">This page doesn't exist.</h2>
                </div>
                <p>Go back to the <Link class="text-primary-500 underline underline-offset-2" href="/">home</Link> page.</p>
            </div>
        </div>
    );
});