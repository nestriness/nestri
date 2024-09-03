/** @jsxImportSource react */

import React from "react"
import {
  text,
  type TextProps as TextVariants,
  type TextAlignProp,
  type TextWeightProp,
  cn
} from "@nestri/ui/design"
// import * as ReactBalancer from "react-wrap-balancer"
import { qwikify$ } from "@builder.io/qwik-react"

type TextSize = TextVariants["size"]
type TitleSizeProp = TextSize | {
  initial?: TextSize,
  sm?: TextSize,
  md?: TextSize,
  lg?: TextSize,
  xl?: TextSize,
  xxl?: TextSize,
}

export interface TextProps extends React.HTMLAttributes<HTMLParagraphElement | HTMLSpanElement | HTMLDivElement> {
  as?: "p" | "div" | "span" | "em" | "strong",
  className?: string,
  size?: TitleSizeProp;
  align?: TextAlignProp;
  weight?: TextWeightProp;
  neutral?: boolean;
}

export const ReactText: React.FC<TextProps> = ({
  size,
  as = "p",
  weight,
  align,
  neutral,
  children,
  className,
  ...props
}) => {

  const TextElement = as

  if (as === "strong") {
    weight = weight || "medium"
    neutral = neutral || true
  } else if (as === "em") {
    neutral = neutral || true
  }

  return (
    <TextElement className={text({
      size,
      weight,
      align,
      neutral,
      className: cn("dark:text-primary-50/70 text-primary-950/70", className)
    })} {...props}>
      {/* <ReactBalancer.Balancer> */}
        {children}
      {/* </ReactBalancer.Balancer> */}
    </TextElement>
  )
}

ReactText.displayName = "Text"

export const Text = qwikify$(ReactText)