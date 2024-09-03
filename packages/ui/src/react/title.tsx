/** @jsxImportSource react */

import React from "react"
import {
    title,
    type TitleProps as TitleVariants,
    type TextAlignProp,
    type TextWeightProp,
    cn
} from "@nestri/ui/design"
import { qwikify$ } from "@builder.io/qwik-react";


type TitleSize = TitleVariants["size"]
type TitleSizeProp = TitleSize | {
    initial?: TitleSize,
    sm?: TitleSize,
    md?: TitleSize,
    lg?: TitleSize,
    xl?: TitleSize,
    xxl?: TitleSize,
}

export interface TitleProps extends React.HTMLAttributes<HTMLHeadingElement> {
    as?: "h1" | "h2" | "h3" | "h4" | "h5" | "h6" | "div" | "span",
    className?: string,
    size?: TitleSizeProp;
    align?: TextAlignProp;
    weight?: TextWeightProp
}

export const ReactTitle: React.FC<TitleProps> = ({
    size,
    as = "h1",
    weight,
    align,
    children,
    className,
    ...props
}) => {
    const TitleElement = as
    return (
        <TitleElement className={title({
            size,
            weight,
            align,
            className: cn("font-title dark:text-primary-50 text-primary-950", className)
        })} {...props}>
            {children}
        </TitleElement>
    )
}

ReactTitle.displayName = "Title"

export const Title = qwikify$(ReactTitle)