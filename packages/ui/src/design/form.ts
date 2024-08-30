import { tv } from "tailwind-variants";

export const form = tv({
    slots: {
        label: "text-nowrap text-[--title-text-color]",
        input: "[--btn-radius:lg] w-full px-[--input-px] bg-transparent peer transition-[outline] placeholder-[--placeholder-text-color] text-[--title-text-color] rounded-[--btn-radius] disabled:opacity-50",
        message: "mt-2 text-[--caption-text-color]",
        icon: "absolute inset-y-0 my-auto text-[--placeholder-text-color] pointer-events-none",
        field: "relative group *:has-[:disabled]:opacity-50 *:has-[:disabled]:pointer-events-none data-[invalid]:[--caption-text-color:theme(colors.danger.600)] dark:data-[invalid]:[--caption-text-color:theme(colors.danger.400)] data-[valid]:[--caption-text-color:theme(colors.success.600)] dark:data-[valid]:[--caption-text-color:theme(colors.success.400)]",
        textarea: "py-[calc(var(--input-px)/1.5)] h-auto",
    },
    variants: {
        variant: {
            outlined: {
                input: "outline-2 focus:outline-primary-600 -outline-offset-1 focus:outline border data-[invalid]:border-danger-600 focus:data-[invalid]:outline-danger-600 dark:data-[invalid]:border-danger-500 dark:focus:data-[invalid]:outline-danger-500 data-[valid]:border-success-600 focus:data-[valid]:outline-success-600 dark:data-[valid]:border-success-500 dark:focus:data-[valid]:outline-success-500",
            },
            soft: {
                input: "outline-none bg-[--ui-soft-bg] focus:brightness-95 dark:focus:brightness-105 data-[invalid]:[--ui-soft-bg:theme(colors.danger.100)] dark:data-[invalid]:[--ui-soft-bg:theme(colors.danger.800/0.25)] data-[valid]:[--ui-soft-bg:theme(colors.success.100)] dark:data-[valid]:[--ui-soft-bg:theme(colors.success.800/0.25)]",
            },
            mixed: {
                input: "placeholder-gray-950/50 dark:placeholder-gray-50/50 shadow-sm hover:shadow-lg dark:hover:shadow-sm shadow-gray-950/50 dark:shadow-gray-50 outline-2 focus:outline-primary-600 focus:outline -outline-offset-1 border border-primary-950 dark:border-primary-50 bg-gray-100 dark:bg-gray-800 data-[invalid]:border-2 data-[invalid]:border-danger-600 focus:data-[invalid]:outline-danger-600 dark:data-[invalid]:border-danger-500 dark:focus:data-[invalid]:outline-danger-500 data-[valid]:border-success-600 focus:data-[valid]:outline-success-600 dark:data-[valid]:border-success-500 dark:focus:data-[valid]:outline-success-500",
            },
            plain: {
                input: "rounded-none px-0 outline-none bg-transparent invalid:text-danger-600 dark:invalid:text-danger-400",
            },
            bottomOutlined: {
                input: "rounded-none transition-[border] px-0 focus:outline-none border-b focus:border-b-2 focus:border-primary-600 data-[invalid]:border-danger-400 dark:data-[invalid]:border-danger-600 data-[valid]:border-success-400 dark:data-[valid]:border-success-600",
            },
        },
        size: {
            xs: {
                message: "text-xs",
            },
            sm: {
                label: "text-sm",
                message: "text-sm",
                input: "text-sm h-8 [--input-px:theme(spacing[2.5])]",
                field: "[--input-px:theme(spacing[2.5])]",
            },
            md: {
                label: "text-base",
                message: "text-base",
                input: "text-sm h-9 [--input-px:theme(spacing[3])]",
                field: "[--input-px:theme(spacing[3])]",
            },
            lg: {
                label: "text-lg",
                input: "text-base h-10 [--input-px:theme(spacing[4])]",
                field: "[--input-px:theme(spacing[4])]",
            },
            xl: {
                label: "text-xl",
                input: "text-base h-12 [--input-px:theme(spacing[5])]",
                field: "[--input-px:theme(spacing[5])]",
            },
        },
        fancy: {
            true: {
                input: "shadow-inner shadow-gray-950/5 dark:shadow-gray-950/35",
            }
        },
        floating: {
            true: {
                label: "absolute block inset-y-0 text-base left-[--input-px] h-fit text-nowrap my-auto text-[--caption-text-color] pointer-events-none transition duration-150 scale-[.8] origin-top-left peer-placeholder-shown:scale-100 peer-focus:scale-[.8] peer-placeholder-shown:translate-y-0",
            },
        },
        asTextarea: {
            true: {
                label: "top-[calc(var(--input-px)/1.5)] mt-0",
            },
        },
    },
    compoundVariants: [
        {
            floating: true,
            variant: ["bottomOutlined", "plain"],
            class: {
                input: "px-0",
                label: "left-0",
            },
        },
        {
            floating: true,
            variant: ["outlined", "soft", "bottomOutlined", "mixed"],
            size: "xl",
            class: {
                field: "[--input-px:theme(spacing[2.5])]",
                input: "pt-3.5 h-14",
                label: "-translate-y-2.5 peer-focus:-translate-y-2.5",
            },
        },
        {
            floating: true,
            variant: ["soft", "bottomOutlined"],
            size: "lg",
            class: {
                input: "pt-4 h-12",
                label: "-translate-y-2 peer-focus:-translate-y-2",
            },
        },
        {
            floating: true,
            variant: ["outlined", "mixed"],
            size: "lg",
            class: {
                input: "h-12",
                label: "-translate-y-[21.5px] peer-focus:-translate-y-[21.5px]",
            },
        },
        {
            floating: true,
            variant: ["outlined", "bottomOutlined", "mixed"],
            size: "md",
            class: {
                input: "h-9",
                label: "-translate-y-[15.5px] peer-focus:-translate-y-[15.5px]",
            },
        },
        {
            floating: true,
            variant: ["outlined", "bottomOutlined", "mixed"],
            size: "sm",
            class: {
                input: "h-8",
                label: "text-base -translate-y-[13.5px] peer-focus:-translate-y-[13.5px] peer-placeholder-shown:text-sm peer-focus:text-base",
            },
        },
        {
            floating: true,
            size: ["sm", "md", "lg"],
            variant: ["outlined", "mixed"],
            class: {
                label: "peer-placeholder-shown:before:scale-x-0 peer-focus:before:scale-x-100 before:scale-x-100 before:absolute peer-placeholder-shown:before:transition-none before:transition-none peer-focus:before:transition peer-focus:before:delay-[.01s] before:duration-500 before:-z-[1] peer-placeholder-shown:before:-top-[9px] before:top-[48%] peer-focus:before:top-[44%] before:-inset-x-1 before:h-0.5 before:my-auto group-has-[:focus]:before:h-[3px] before:bg-white dark:before:bg-[--ui-bg]",
            },
        },
        {
            floating: true,
            variant: ["outlined", "mixed"],
            class: {
                label: "translate-x-px",
            },
        },
        {
            floating: true,
            asTextarea: true,
            size: ["lg", "xl"],
            variant: "bottomOutlined",
            class: {
                input: "pt-0",
            },
        },
        {
            variant: "plain",
            class: {
                input: "py-0",
            },
        },
        {
            floating: true,
            asTextarea: true,
            size: ["xl"],
            class: {
                input: "pt-7",
                label: "-translate-y-1 peer-focus:-translate-y-1",
            },
        },
        {
            floating: true,
            asTextarea: true,
            size: ["lg"],
            class: {
                input: "pt-6",
                label: "-translate-y-1 peer-focus:-translate-y-1 before:hidden",
            },
        },
        {
            floating: true,
            asTextarea: true,
            size: ["md"],
            variant: ["outlined", "bottomOutlined", "mixed"],
            class: {
                label: "-translate-y-[17.5px] peer-focus:-translate-y-[17.5px]",
            },
        },
        {
            floating: true,
            asTextarea: true,
            size: ["sm"],
            variant: ["outlined", "bottomOutlined", "mixed"],
            class: {
                label: "-translate-y-4 peer-focus:-translate-y-4",
            },
        },
    ],
});

export type FormProps = {
    variant?: keyof typeof form.variants.variant;
    size?: keyof typeof form.variants.size;
    floating?: boolean;
    asTextarea?: boolean;
};

export type InputProps = Omit<FormProps, "asTextarea"> & {
    size?: Exclude<FormProps["size"], "xs">;
    fancy?: boolean;
};

export type LabelProps = FormProps & {
    size?: Exclude<FormProps["size"], "xs">;
};

export type MessageProps = {
    size?: Exclude<FormProps["size"], "lg" | "xl">;
};