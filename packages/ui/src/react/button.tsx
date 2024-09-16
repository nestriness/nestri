/** @jsxImportSource react */

import * as React from "react"
import { motion } from "framer-motion"
import { qwikify$ } from "@builder.io/qwik-react";
import {  cn, buttonIcon, buttonVariants, type ButtonVariantProps, type ButtonVariantIconProps } from "@nestri/ui/design";
import {cloneElement,} from "./utils"

export interface ButtonRootProps extends React.HTMLAttributes<HTMLButtonElement | HTMLAnchorElement>, ButtonVariantProps {
    class?: string;
    children?: React.ReactElement;
    onClick?: () => void | Promise<void>;
    setIsLoading?: (v: boolean) => void;
    isLoading: boolean;
    disabled?: boolean;
    loadingTime?: number;
    href?: string;
}

export interface ButtonIconProps extends React.HTMLAttributes<HTMLElement>, ButtonVariantIconProps {
    isLoading: boolean;
    height?: number;
    width?: number;
    class?:string;
}

export interface ButtonLabelProps extends React.HTMLAttributes<HTMLElement>, ButtonVariantIconProps {
    loadingText?: string;
    class?: string;
    isLoading: boolean;
}

export const Icon: React.FC<ButtonIconProps> = (({
    class:className,
    children,
    size = "md",
    isLoading,
    type = "leading",
    height = 20,
    width = 20,
}) => {

    if (isLoading) {
        return (
            <motion.svg
                height={height}
                width={width}
                viewBox="0 0 24 24"
                xmlns="http://www.w3.org/2000/svg"
                // className="h-4 w-4 fill-white"
                fill="currentColor"
                animate={{ rotate: 1080 }}
                transition={{
                    repeat: 1 / 0,
                    duration: 1,
                    ease: [.1, .21, .355, .68],
                    repeatType: "loop"
                }}
                className={buttonIcon({ size, type, className })}
            >
                <path d="M12,1A11,11,0,1,0,23,12,11,11,0,0,0,12,1Zm0,19a8,8,0,1,1,8-8A8,8,0,0,1,12,20Z" opacity=".25"></path>
                <path d="M10.14,1.16a11,11,0,0,0-9,8.92A1.59,1.59,0,0,0,2.46,12,1.52,1.52,0,0,0,4.11,10.7a8,8,0,0,1,6.66-6.61A1.42,1.42,0,0,0,12,2.69h0A1.57,1.57,0,0,0,10.14,1.16Z"></path>
            </motion.svg>
        )
    // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
    } else if (!isLoading && children) {
        return cloneElement(children as React.ReactElement, buttonIcon({ size, type, className }))
    }
})

export const Label = React.forwardRef<HTMLElement, ButtonLabelProps>(({
    class:className,
    children,
    loadingText,
    isLoading,
    ...props
}, forwardedRef) => {

    return (
        <span className={className} {...props} ref={forwardedRef}>{isLoading && loadingText ? loadingText : children}</span>
    )
})

const Root = React.forwardRef<HTMLButtonElement & HTMLAnchorElement, ButtonRootProps>(({
    class: className,
    href,
    disabled = false,
    loadingTime,
    intent = "primary",
    variant = "solid",
    size = "md",
    isLoading,
    setIsLoading,
    onClick,
    children,
    ...props }, forwardedRef) => {

    const handleClick = async () => {
        if (setIsLoading) {
            if (!isLoading && !disabled) {
                setIsLoading(true);
                if (onClick) {
                    await onClick();
                } else {
                    // Simulate an async operation
                    await new Promise(resolve => setTimeout(resolve, loadingTime));
                }
                setIsLoading(false);
            }
        }
    };
    const Component = href ? 'a' : 'button';
    const iconOnly = React.Children.toArray(children).some(child =>
        React.isValidElement(child) && child.type === Icon && child.props.type === 'only'
    );

    const buttonSize = iconOnly ? 'iconOnlyButtonSize' : 'size';

    return (
        <Component
            ref={forwardedRef}
            onClick={handleClick}
            disabled={isLoading || disabled}
            className={cn(buttonVariants[variant as keyof typeof buttonVariants]({ intent, [buttonSize]: size, className }))}
            {...props}
        >
            {children}
        </Component>
    )
})

export type ButtonRoot = typeof Root;
export type ButtonIcon = typeof Icon;
export type ButtonLabel = typeof Label;

Root.displayName = 'Root';
Icon.displayName = "Icon";
Label.displayName = "Label";

export const Button = {
    Root: qwikify$(Root),
    Icon: qwikify$(Icon),
    Label: qwikify$(Label)
}

//The ✔️ SVG is here:
{/* <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="currentColor" class="h-5 w-5 shrink-0 fill-white"><path fill-rule="evenodd" clip-rule="evenodd" d="M12 1C5.92487 1 1 5.92487 1 12C1 18.0751 5.92487 23 12 23C18.0751 23 23 18.0751 23 12C23 5.92487 18.0751 1 12 1ZM17.2071 9.70711C17.5976 9.31658 17.5976 8.68342 17.2071 8.29289C16.8166 7.90237 16.1834 7.90237 15.7929 8.29289L10.5 13.5858L8.20711 11.2929C7.81658 10.9024 7.18342 10.9024 6.79289 11.2929C6.40237 11.6834 6.40237 12.3166 6.79289 12.7071L9.79289 15.7071C10.1834 16.0976 10.8166 16.0976 11.2071 15.7071L17.2071 9.70711Z"></path></svg> */ }