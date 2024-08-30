// The solid and outlined variants are based on CSS Pro 3D Buttons code (https://csspro.com/css-3d-buttons/)

import { tv } from "tailwind-variants";

export type ButtonVariantProps = {
    variant?: keyof typeof buttonVariants | undefined;
    intent?: keyof typeof solid.variants.intent;
    size?: keyof typeof baseButton.variants.size;
};
export type ButtonVariantIconProps = {
    type?: keyof typeof buttonIcon.variants.type;
    size?: keyof typeof buttonIcon.variants.size;
};

const baseButton = tv({
    base: "group font-title flex justify-center select-none gap-1.5 items-center rounded-lg outline-2 outline-offset-2 focus-visible:outline outline-primary-6000 disabled:text-gray-400 disabled:border disabled:border-gray-300 dark:disabled:bg-gray-500/10 disabled:bg-gray-200 disabled:shadow-none disabled:hover:brightness-100 dark:disabled:text-gray-700 dark:disabled:shadow-none disabled:cursor-not-allowed dark:disabled:border dark:disabled:border-gray-700", // dark:disabled:[background-image:none]
    variants: {
        size: {
            xs: "text-sm h-7 px-3",
            sm: "text-sm h-8 px-3.5",
            md: "text-base h-9 px-4",
            lg: "text-base h-10 px-5",
            xl: "text-lg h-12 px-6",
        },
        iconOnlyButtonSize: {
            xs: "size-7",
            sm: "size-8",
            md: "size-9",
            lg: "size-10",
            xl: "size-12",
        },
        defaultVariants: {
            intent: "primary",
            size: "md",
        },
    },
});

const link = tv({
    // extend: baseButton,
    // base: "group transition-shadows relative ml-1.5 font-medium shadow-[inset_0_-0.2em_0_0_theme(colors.primary.200)] dark:shadow-[inset_0_-0.2em_0_0_theme(colors.primary.600)] duration-300 ease-out hover:shadow-[inset_0_-1em_0_0_theme(colors.primary.200)] dark:hover:shadow-[inset_0_-1em_0_0_theme(colors.primary.600)] text-primary-950 font-medium dark:text-primary-50", //"relative text-primary-950 font-medium dark:text-primary-50 hover:after:w-[calc(100%+2px)] focus:after:w-[calc(100%+2px)] px-1 after:-bottom-1 transition-all duration-[.2s] ease-[cubic-bezier(.165,.84,.44,1)] after:w-0 after:h-0.5 after:bg-primary-950 dark:after:bg-primary-50 after:absolute after:left-0 after:transition-[width] after:duration-[.2s] focus:shadow-none outline-none"
    base: "group relative ml-1.5 font-medium border-b-2 border-primary-950 dark:border-primary-50 hover:border-primary-950/60 dark:hover:border-primary-50/60 hover:text-primary-950/70 dark:hover:text-primary-50/70 text-primary-950 font-medium dark:text-primary-50", //"relative text-primary-950 font-medium dark:text-primary-50 hover:after:w-[calc(100%+2px)] focus:after:w-[calc(100%+2px)] px-1 after:-bottom-1 transition-all duration-[.2s] ease-[cubic-bezier(.165,.84,.44,1)] after:w-0 after:h-0.5 after:bg-primary-950 dark:after:bg-primary-50 after:absolute after:left-0 after:transition-[width] after:duration-[.2s] focus:shadow-none outline-none"
    variants: {
        size: {
            xs: "text-sm h-7 px-3",
            sm: "text-sm h-8 px-3.5",
            md: "text-base h-9 px-4",
            lg: "text-base h-10 px-5",
            xl: "text-lg h-12 px-6",
        },
        iconOnlyButtonSize: {
            xs: "size-7",
            sm: "size-8",
            md: "size-9",
            lg: "size-10",
            xl: "size-12",
        },
        defaultVariants: {
            intent: "primary",
            size: "md",
        },
    },
});

const solid = tv({
    extend: baseButton,
    base: "bg-gradient-to-b [box-shadow:rgba(255,255,255,0.25)_0px_1px_0px_0px_inset,var(--btn-border-color)_0px_0px_0px_1px] text-white hover:brightness-[1.1] transition-[filter] duration-150 ease-in-out active:brightness-95 dark:border-t dark:shadow-white/10 disabled:from-gray-200 disabled:to-gray-200 dark:disabled:text-gray-400 dark:disabled:from-gray-800 dark:disabled:to-gray-800",
    variants: {
        intent: {
            primary:
                "from-primary-500 to-primary-600 [--btn-border-color:theme(colors.primary.600)] dark:border-primary-500/75",
            secondary:
                "from-secondary-500 to-secondary-600 [--btn-border-color:theme(colors.secondary.700)] dark:border-secondary-400/75",
            accent: "from-accent-500 to-accent-600 [--btn-border-color:theme(colors.accent.700)] dark:border-accent-400/75",
            danger: "from-danger-500 to-danger-600 [--btn-border-color:theme(colors.danger.700)] dark:border-danger-400/75",
            info: "from-info-500 to-info-600 [--btn-border-color:theme(colors.info.700)] dark:border-info-400/75",
            success:
                "from-success-500 to-success-600 [--btn-border-color:theme(colors.success.700)] dark:border-success-400/75",
            warning:
                "from-warning-400 to-warning-500 text-warning-950 [--btn-border-color:theme(colors.warning.600)] dark:border-warning-300",
            gray: "from-gray-500 to-gray-600 [--btn-border-color:theme(colors.gray.700)] dark:border-gray-500",
            neutral:
                "bg-gray-900 [background-image:radial-gradient(76%_151%_at_52%_-52%,rgba(255,255,255,0.5)_0%,transparent_100%)] [box-shadow:rgba(255,255,255,0.3)_0px_1px_0px_0px_inset,theme(colors.gray.950)_0px_0px_0px_1px] hover:brightness-125 dark:bg-white dark:text-gray-950 dark:border-gray-300",
        },
    },
});

const outlined = tv({
    extend: baseButton,
    base: "[--outline-radial-opacity:0.6] dark:[background-image:none] [--inner-border-color:1] dark:[--inner-border-color:0] dark:[--outline-radial-opacity:0.2] [background-image:radial-gradient(76%_151%_at_52%_-52%,rgba(255,255,255,var(--outline-radial-opacity))_0%,transparent_100%)] [box-shadow:rgba(255,255,255,var(--inner-border-color))_0px_1px_0px_0px_inset,var(--btn-border-color)_0px_0px_0px_1px,0px_1px_2px_rgba(0,0,0,0.1)] hover:brightness-[0.98] active:brightness-100 transtion-[filter] ease-in-out duration-150",
    variants: {
        intent: {
            primary:
                "[--btn-border-color:theme(colors.primary.200)] dark:[--btn-border-color:theme(colors.primary.500/0.3)] text-primary-800 bg-primary-50 dark:text-primary-300 dark:bg-primary-500/5 dark:hover:bg-primary-500/10 dark:active:bg-primary-500/5",
            secondary:
                "[--btn-border-color:theme(colors.secondary.200)] dark:[--btn-border-color:theme(colors.secondary.500/0.3)] text-secondary-800 bg-secondary-50 dark:text-secondary-300 dark:bg-secondary-500/5 dark:hover:bg-secondary-500/10 dark:active:bg-secondary-500/5",
            accent: "[--btn-border-color:theme(colors.accent.200)] dark:[--btn-border-color:theme(colors.accent.500/0.3)] text-accent-800 bg-accent-50 dark:text-accent-300 dark:bg-accent-500/5 dark:hover:bg-accent-500/10 dark:active:bg-accent-500/5",
            danger: "[--btn-border-color:theme(colors.danger.200)] dark:[--btn-border-color:theme(colors.danger.500/0.3)] text-danger-800 bg-danger-50 dark:text-danger-300 dark:bg-danger-500/5 dark:hover:bg-danger-500/10 dark:active:bg-danger-500/5",
            info: "[--btn-border-color:theme(colors.info.200)] dark:[--btn-border-color:theme(colors.info.500/0.3)] text-info-800 bg-info-50 dark:text-info-300 dark:bg-info-500/5 dark:hover:bg-info-500/10 dark:active:bg-info-500/5",
            success:
                "[--btn-border-color:theme(colors.success.200)] dark:[--btn-border-color:theme(colors.success.500/0.3)] text-success-800 bg-success-100 dark:text-success-300 dark:bg-success-500/5 dark:hover:bg-success-500/10 dark:active:bg-success-500/5",
            warning:
                "[--btn-border-color:theme(colors.warning.200)] dark:[--btn-border-color:theme(colors.warning.500/0.3)] text-warning-800 bg-warning-50 dark:text-warning-300 dark:bg-warning-500/5 dark:hover:bg-warning-500/10 dark:active:bg-warning-500/5",
            gray: "[--btn-border-color:theme(colors.gray.200)] dark:[--btn-border-color:theme(colors.gray.500/0.3)] text-gray-800 bg-gray-50 dark:text-gray-300 dark:bg-gray-500/5 dark:hover:bg-gray-500/10 dark:active:bg-gray-500/5",
            neutral:
                "[--btn-border-color:theme(colors.gray.300)] dark:[--btn-border-color:theme(colors.gray.700)] text-gray-800 bg-gray-100 dark:text-white dark:bg-gray-500/5 dark:hover:bg-gray-500/10 dark:active:bg-gray-500/5",
        },
    },
});

const soft = tv({
    extend: baseButton,
    variants: {
        intent: {
            primary:
                "text-primary-700 bg-primary-100 hover:bg-primary-200/75 active:bg-primary-100 dark:text-primary-300 dark:bg-primary-500/10 dark:hover:bg-primary-500/15 dark:active:bg-primary-500/10",
            secondary:
                "text-secondary-700 bg-secondary-100 hover:bg-secondary-200/75 active:bg-secondary-100 dark:text-secondary-300 dark:bg-secondary-500/10 dark:hover:bg-secondary-500/15 dark:active:bg-secondary-500/10",
            accent: "text-accent-700 bg-accent-100 hover:bg-accent-200/75 active:bg-accent-100 dark:text-accent-300 dark:bg-accent-500/10 dark:hover:bg-accent-500/15 dark:active:bg-accent-500/10",
            danger: "text-danger-700 bg-danger-100 hover:bg-danger-200/75 active:bg-danger-100 dark:text-danger-300 dark:bg-danger-500/10 dark:hover:bg-danger-500/15 dark:active:bg-danger-500/10",
            info: "text-info-700 bg-info-100 hover:bg-info-200/75 active:bg-info-100 dark:text-info-300 dark:bg-info-500/10 dark:hover:bg-info-500/15 dark:active:bg-info-500/10",
            success:
                "text-success-700 bg-success-100 hover:bg-success-200/75 active:bg-success-100 dark:text-success-300 dark:bg-success-500/10 dark:hover:bg-success-500/15 dark:active:bg-success-500/10",
            warning:
                "text-warning-700 bg-warning-100 hover:bg-warning-200/75 active:bg-warning-100 dark:text-warning-300 dark:bg-warning-500/10 dark:hover:bg-warning-500/15 dark:active:bg-warning-500/10",
            gray: "text-gray-800 bg-gray-100 hover:bg-gray-200/75 active:bg-gray-100 dark:text-gray-300 dark:bg-gray-500/10 dark:hover:bg-gray-500/15 dark:active:bg-gray-500/10",
            neutral:
                "text-gray-950 bg-gray-100 hover:bg-gray-950 hover:text-white active:text-white active:bg-gray-900 dark:text-gray-300 dark:bg-gray-500/10 dark:hover:bg-white dark:hover:text-gray-950 dark:active:bg-gray-200 dark:active:text-gray-950",
        },
    },
});

const ghost = tv({
    extend: baseButton,
    variants: {
        intent: {
            primary:
                "hover:bg-gray-100 active:bg-primary-200/75 dark:hover:bg-gray-800 dark:active:bg-primary-500/15",
            secondary:
                "text-secondary-600 hover:bg-secondary-100 active:bg-secondary-200/75 dark:text-secondary-400 dark:hover:bg-secondary-500/10 dark:active:bg-secondary-500/15",
            accent: "text-accent-600 hover:bg-accent-100 active:bg-accent-200/75 dark:text-accent-400 dark:hover:bg-accent-500/10 dark:active:bg-accent-500/15",
            danger: "text-danger-600 hover:bg-danger-100 active:bg-danger-200/75 dark:text-danger-400 dark:hover:bg-danger-500/10 dark:active:bg-danger-500/15",
            info: "text-info-600 hover:bg-info-100 active:bg-info-200/75 dark:text-info-400 dark:hover:bg-info-500/10 dark:active:bg-info-500/15",
            success:
                "text-success-600 hover:bg-success-100 active:bg-success-200/75 dark:text-success-400 dark:hover:bg-success-500/10 dark:active:bg-success-500/15",
            warning:
                "text-warning-600 hover:bg-warning-100 active:bg-warning-200/75 dark:text-warning-400 dark:hover:bg-warning-500/10 dark:active:bg-warning-500/15",
            gray: "text-gray-800 hover:bg-gray-100 active:bg-gray-200/75 dark:text-gray-300 dark:hover:bg-gray-500/10 dark:active:bg-gray-500/15",
            neutral:
                "text-gray-950 hover:bg-gray-950 hover:text-white active:text-white active:bg-gray-900 dark:text-white dark:hover:bg-white dark:hover:text-gray-950 dark:active:bg-gray-200 dark:active:text-gray-950",
        },
    },
});

export const buttonIcon = tv({
    variants: {
        type: {
            leading: "-ml-1",
            trailing: "-mr-1",
            only: "m-auto",
        },
        size: {
            xs: "size-3.5",
            sm: "size-4",
            md: "size-[1.125rem]",
            lg: "size-5",
            xl: "size-6",
        },
    },
});

export const buttonVariants = {
    solid,
    outlined,
    soft,
    ghost,
    link
};