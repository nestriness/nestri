import { component$, useSignal } from "@builder.io/qwik"
import { cn } from "./design"

type Props = {
    class?: string
}
const minValue = 2;
const maxValue = 6;
const teamSizes = Array.from({ length: maxValue - minValue + 1 }, (_, i) => i + minValue);

export const TeamCounter = component$(({ class: className }: Props) => {
    const teammates = useSignal(2)
    const shake = useSignal(false)

    return (
        <div class={cn("flex items-center justify-center", shake.value && "animate-shake", className)}>
            <button
                onClick$={() => {
                    if (teammates.value == minValue) {
                        shake.value = true
                        setTimeout(() => {
                            shake.value = false
                        }, 500);
                    } else {
                        teammates.value = Math.max(minValue, teammates.value - 1)
                    }
                }}
                class={cn("size-[30px] rounded-full bg-gray-300 dark:bg-gray-700 flex items-center justify-center active:scale-90", teammates.value <= minValue && "opacity-30")}>
                <svg xmlns="http://www.w3.org/2000/svg" class="size-[15px]" viewBox="0 0 256 256"><path fill="currentColor" d="M228 128a12 12 0 0 1-12 12H40a12 12 0 0 1 0-24h176a12 12 0 0 1 12 12" /></svg>
            </button>
            <div class="flex items-center justify-center mx-2 mt-1">
                <p class="text-lg font-normal w-full flex items-center justify-center">
                    <span
                        class="w-[2ch] relative inline-block h-[2.625rem] overflow-hidden [mask:linear-gradient(#0000,_#000_35%_65%,_#0000)]">
                        <span
                            style={{
                                translate: `0 calc((${teammates.value - (minValue + 1)} + 1) * (2.625rem * -1))`,
                                transition: `translate 0.625s linear(0 0%,0.5007 7.21%,0.7803 12.29%,0.8883 14.93%,0.9724 17.63%,1.0343 20.44%,1.0754 23.44%,1.0898 25.22%,1.0984 27.11%,1.1014 29.15%,1.0989 31.4%,1.0854 35.23%,1.0196 48.86%,1.0043 54.06%,0.9956 59.6%,0.9925 68.11%,1 100%)`
                            }}
                            class="absolute w-full flex top-0 flex-col">
                            {teamSizes.map((num) => (
                                <span
                                    class="w-full font-title h-[2.625rem] flex flex-col items-center justify-center leading-[1rem]"
                                    key={`team-member-${num}`} >+{num}</span>
                            ))}
                        </span>
                    </span>
                </p>
            </div>
            <button onClick$={() => {
                if (teammates.value == maxValue) {
                    shake.value = true
                    setTimeout(() => {
                        shake.value = false
                    }, 500);
                } else {
                    teammates.value = Math.min(maxValue, teammates.value + 1)
                }
            }}
                class={cn("size-[30px] rounded-full bg-gray-300 dark:bg-gray-700 flex items-center justify-center active:scale-90", teammates.value >= maxValue && "opacity-30")}>
                <svg xmlns="http://www.w3.org/2000/svg" class="size-[15px]" viewBox="0 0 256 256"><path fill="currentColor" d="M228 128a12 12 0 0 1-12 12h-76v76a12 12 0 0 1-24 0v-76H40a12 12 0 0 1 0-24h76V40a12 12 0 0 1 24 0v76h76a12 12 0 0 1 12 12" /></svg>
            </button>
        </div>
    )
})