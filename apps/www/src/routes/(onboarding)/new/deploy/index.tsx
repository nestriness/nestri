import { component$ } from "@builder.io/qwik";

export default component$(() => {
    return (
        <>
            <div class="w-full justify-center py-20 gap-10 flex flex-col px-6" >
                <div class="mx-auto w-full items-center max-w-xl flex gap-3 flex-col">
                    <span class="text-base underline underline-offset-4 px-2">Step 2 of 2</span>
                    <h1 class="text-4xl font-bold font-title">
                        You're almost done
                    </h1>
                    <p class="text-lg font-medium text-gray-600/70 dark:text-gray-400/70">
                        Please follow the steps to configure your game and install it
                    </p>
                </div>
               
            </div>
        </>
    )
})