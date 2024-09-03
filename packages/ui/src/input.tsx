import {
    form,
    cn,
    type InputProps as InputVariants,
} from "./design"
import { type QwikIntrinsicElements, component$ } from '@builder.io/qwik';

export interface InputComponentProps extends Omit<QwikIntrinsicElements["input"], 'size'>, InputVariants {
    label?: string;
    includeName?: string;
    labelFor?: string;
    description?: string;
}

export const Input = component$(({ labelFor,description, label, variant = "mixed", fancy = false, size = "md", includeName, class: className, ...props }: InputComponentProps) => {
    const { input } = form();

    return (
        <div class="text-start w-full gap-2 flex flex-col" >{
            label && labelFor && <label for={labelFor} class='text-sm font-medium leading-none peer-disabled:cursor-not-allowed peer-disabled:opacity-70'>{label}</label>
        }
            {description && <p class='text-[0.8rem]'>{description}</p>}
            <div class="flex flex-row w-full h-max relative" >
                {includeName && <p class="absolute top-1/2 -translate-y-1/2 left-3 text-gray-950 dark:text-gray-50" >{includeName}&nbsp;</p>}
                <input
                    id={labelFor}
                    class={input({ variant, fancy, size, className: cn("rounded-md w-full data-[invalid]:animate-shake", className as any) })}
                    {...props}
                />
            </div>
        </div>
    )
})

// export const Input = qwikify$(ReactInput)