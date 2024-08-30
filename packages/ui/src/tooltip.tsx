import { component$, Slot } from "@builder.io/qwik"
import { cn } from "./design";

type Props = {
    position: "bottom" | "top" | "left" | "right",
    text: string;
}

const textPosition = {
    top: "bottom-[125%] left-1/2 -ml-[60px] after:absolute after:left-1/2 after:top-[100%] after:-ml-[5px] after:border-[5px] after:border-[#000_transparent_transparent_transparent]"
}

export const Tooltip = component$(({ position, text }: Props) => {

    return (
        <>
            <Slot />
            {/**@ts-ignore */}
            <span class={cn("invisible absolute w-[120px] group-hover:visible group-hover:opacity-100 text-white bg-black text-center py-1 rounded-md", textPosition[position])} >
                {text}
            </span>
        </>
    )
})