import { tv } from "tailwind-variants";

export const base = tv({
    variants: {
        weight: {
            black: "font-black",
            bold: "font-bold",
            semibold: "font-semibold",
            medium: "font-medium",
            normal: "font-normal",
        },
        align: {
            left: "text-left",
            center: "text-center",
            right: "text-right",
        },
    },
    defaultVariants: {
        size: "xl",
        weight: "normal",
    },
});

export const caption = tv(
    {
        extend: base,
        base: "text-gray-500",
        variants: {
            size: {
                xs: "text-xs",
                sm: "text-sm",
                base: "text-base",
            },
            neutral: {
                true: "text-gray-950 dark:text-white",
            },
        },
        defaultVariants: {
            size: "sm",
            weight: "normal",
        },
    },
    {
        responsiveVariants: ["sm", "md", "lg", "xl", "2xl"],
    },
);

export const text = tv(
    {
        extend: base,
        base: "text-gray-700",
        variants: {
            size: {
                sm: "text-sm",
                base: "text-base",
                lg: "text-lg",
                xl: "text-xl",
            },
            neutral: {
                true: "text-gray-950 dark:text-white",
            },
        },
        defaultVariants: {
            size: "base",
            weight: "normal",
        },
    },
    {
        responsiveVariants: ["sm", "md", "lg", "xl", "2xl"],
    },
);

export const list = tv(
    {
        extend: text,
        base: "list-outside pl-4",
        variants: {
            type: {
                disc: "list-disc",
                decimal: "list-decimal",
                none: "list-none",
            },
            inside: {
                true: "list-outside pl-0",
            },
        },
        defaultVariants: {
            size: "base",
            type: "disc",
            weight: "normal",
            inside: false,
        },
    },
    {
        responsiveVariants: ["sm", "md", "lg", "xl", "2xl"],
    },
);

export const link = tv(
    {
        extend: base,
        base: "transition",
        variants: {
            size: {
                xs: "text-xs",
                sm: "text-sm",
                base: "text-base",
                lg: "text-lg",
                xl: "text-xl",
            },
            intent: {
                primary:
                    "text-primary-600 hover:text-primary-700 dark:text-primary-400 dark:hover:text-primary-500",
                secondary:
                    "text-secondary-600 hover:text-secondary-700 dark:text-secondary-400 dark:hover:text-secondary-500",
                accent: "text-accent-600 hover:text-accent-700 dark:text-accent-400 dark:hover:text-accent-500",
                info: "text-info-600 hover:text-info-700 dark:text-info-400 dark:hover:text-info-500",
                danger: "text-danger-600 hover:text-danger-700 dark:text-danger-400 dark:hover:text-danger-500",
                success:
                    "text-success-600 hover:text-success-700 dark:text-success-400 dark:hover:text-success-500",
                warning:
                    "text-warning-700 hover:text-warning-600 dark:text-warning-400 dark:hover:text-warning-500",
                gray: "text-gray-700",
                neutral:
                    "text-gray-950 hover:text-gray-800 dark:text-white dark:hover:text-gray-200",
            },
            variant: {
                plain: "",
                underlined: "underline",
                ghost: "hover:underline",
                animated:
                    "relative before:absolute before:inset-x-0 before:bottom-0 before:h-px before:scale-x-0 before:origin-right hover:before:origin-left hover:before:scale-x-100 before:transition before:duration-200",
            },
            visited: {
                true: "visited:text-accent-600 dark:visited:text-accent-400",
            },
        },
        compoundVariants: [
            {
                variant: ["plain", "ghost", "underlined"],
                intent: "gray",
                class: "hover:text-gray-950 dark:hover:text-white",
            },
            {
                variant: "animated",
                intent: "primary",
                class: "before:bg-primary-600/50 dark:before:bg-primary-400/50",
            },
            {
                variant: "animated",
                intent: "info",
                class: "before:bg-info-600/50 dark:before:bg-info-400/50",
            },
            {
                variant: "animated",
                intent: "neutral",
                class: "before:bg-gray-950/50 dark:before:bg-white/50",
            },
            {
                variant: "animated",
                intent: "gray",
                class: "before:bg-gray-600/50 dark:before:bg-gray-400/50",
            },
            {
                variant: "animated",
                intent: "secondary",
                class: "before:bg-secondary-600/50 dark:before:bg-secondary-400/50",
            },
            {
                variant: "animated",
                intent: "accent",
                class: "before:bg-accent-600/50 dark:before:bg-accent-400/50",
            },
            {
                variant: "animated",
                intent: "danger",
                class: "before:bg-danger-600/50 dark:before:bg-danger-400/50",
            },
            {
                variant: "animated",
                intent: "success",
                class: "before:bg-success-600/50 dark:before:bg-success-400/50",
            },
            {
                variant: "animated",
                intent: "warning",
                class: "before:bg-warning-600/50 dark:before:bg-warning-400/50",
            },
            {
                variant: "underlined",
                intent: "primary",
                class: "decoration-primary-600/50 dark:decoration-primary-400/50",
            },
            {
                variant: "underlined",
                intent: "info",
                class: "decoration-info-600/50 dark:decoration-info-400/50",
            },
            {
                variant: "underlined",
                intent: "gray",
                class: "decoration-gray-600/50 dark:decoration-gray-400/50",
            },
            {
                variant: "underlined",
                intent: "neutral",
                class: "decoration-gray-950/25 dark:decoration-white/25",
            },
            {
                variant: "underlined",
                intent: "secondary",
                class: "decoration-secondary-600/50 dark:decoration-secondary-400/50",
            },
            {
                variant: "underlined",
                intent: "accent",
                class: "decoration-accent-600/50 dark:decoration-accent-400/50",
            },
            {
                variant: "underlined",
                intent: "danger",
                class: "decoration-danger-600/50 dark:decoration-danger-400/50",
            },
            {
                variant: "underlined",
                intent: "success",
                class: "decoration-success-600/50 dark:decoration-success-400/50",
            },
            {
                variant: "underlined",
                intent: "warning",
                class: "decoration-warning-600/50 dark:decoration-warning-400/50",
            },
        ],
        defaultVariants: {
            intent: "primary",
            variant: "ghost",
            size: "base",
        },
    },
    {
        responsiveVariants: ["sm", "md", "lg", "xl", "2xl"],
    },
);

export const display = tv(
    {
        extend: base,
        base: "block text-gray-950 dark:text-gray-50",
        variants: {
            size: {
                "4xl": "text-4xl",
                "5xl": "text-5xl",
                "6xl": "text-6xl",
                "7xl": "text-7xl",
                "8xl": "text-8xl",
                "9xl": "text-9xl",
            },
        },
        defaultVariants: {
            size: "6xl",
            weight: "bold",
        },
    },
    {
        responsiveVariants: ["sm", "md", "lg", "xl", "2xl"],
    },
);

export const title = tv(
    {
        extend: base,
        base: "block text-gray-950",
        variants: {
            size: {
                base: "text-base",
                lg: "text-lg",
                xl: "text-xl",
                "2xl": "text-2xl",
                "3xl": "text-3xl",
            },
        },
        defaultVariants: {
            size: "xl",
            weight: "semibold",
        },
    },
    {
        responsiveVariants: ["sm", "md", "lg", "xl", "2xl"],
    },
);

export const codeTheme = tv({
    base: "text-sm inline-block border rounded-md py-px px-1",
    variants: {
        intent: {
            primary:
                "bg-primary-50 text-primary-600 dark:text-primary-300 border-primary-200 dark:border-primary-500/20 dark:bg-primary-500/5",
            secondary:
                "bg-secondary-50 text-secondary-600 dark:text-secondary-300 border-secondary-200 dark:border-secondary-500/20 dark:bg-secondary-500/5",
            accent: "bg-accent-50 text-accent-600 dark:text-accent-300 border-accent-200 dark:border-accent-500/20 dark:bg-accent-500/5",
            gray: "bg-gray-50 text-gray-700 dark:border-gray-500/20 dark:bg-gray-500/5 dark:border-gray-500/20",
            neutral: "bg-gray-50 text-gray-950 dark:text-white dark:bg-gray-500/5 dark:border-gray-500/20",
        },
    },
    defaultVariants: {
        intent: "gray",
    },
});

export const kbdTheme = tv({
    base: "inline-flex items-center justify-center text-gray-800 dark:text-white h-5 text-[11px] min-w-5 px-1.5 rounded font-sans bg-gray-100 dark:bg-white/10 ring-1 border-b border-t border-t-white border-b-gray-200 dark:border-t-transparent dark:border-b-gray-950 ring-gray-300 dark:ring-white/15",
});

export type CodeThemeProps = {
    intent?: keyof typeof codeTheme.variants.intent;
};

export type Weight = keyof typeof base.variants.weight;
export type Align = keyof typeof base.variants.align;

type BaseTextProps = {
    weight?: Weight;
    align?: Align;
};

export type CaptionProps = BaseTextProps & {
    size?: keyof typeof caption.variants.size;
    neutral?: boolean;
};

export type TextProps = BaseTextProps & {
    size?: keyof typeof text.variants.size;
    neutral?: boolean;
};

export type ListProps = BaseTextProps & {
    size?: keyof typeof text.variants.size;
    type?: keyof typeof list.variants.type;
    inside?: boolean;
    neutral?: boolean;
};

export type LinkProps = BaseTextProps & {
    size?: keyof typeof text.variants.size | keyof typeof link.variants.size;
    variant?: keyof typeof link.variants.variant;
    intent?: keyof typeof link.variants.intent;
    visited?: boolean;
};

export type TitleProps = BaseTextProps & {
    size?: keyof typeof title.variants.size;
};

export type TitleSizeProp =
    | TitleProps["size"]
    | {
          initial?: TitleProps["size"];
          sm?: TitleProps["size"];
          md?: TitleProps["size"];
          lg?: TitleProps["size"];
          xl?: TitleProps["size"];
          xxl?: TitleProps["size"];
      };

export type TextSizeProp =
    | TextProps["size"]
    | {
          initial?: TextProps["size"];
          sm?: TextProps["size"];
          md?: TextProps["size"];
          lg?: TextProps["size"];
          xl?: TextProps["size"];
          xxl?: TextProps["size"];
      };

export type DisplayProps = BaseTextProps & {
    size?: keyof typeof display.variants.size;
};

export type TextWeightProp =
    | Weight
    | {
          initial?: Weight;
          sm?: Weight;
          md?: Weight;
          lg?: Weight;
          xl?: Weight;
          xxl?: Weight;
      };

export type TextAlignProp =
    | Align
    | {
          initial?: Align;
          sm?: Align;
          md?: Align;
          lg?: Align;
          xl?: Align;
          xxl?: Align;
      };