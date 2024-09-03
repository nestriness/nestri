/** @jsxImportSource react */
import React from 'react'
import { motion } from 'framer-motion'
import { cn } from "@nestri/ui/design"
import { qwikify$ } from '@builder.io/qwik-react';

export const CursorSVG = ({ flip }: { flip?: boolean }) => (
    <svg fill="none" height="18"
        className={cn(flip ? 'scale-x-[-1]' : '', 'mb-9')}
        viewBox="0 0 17 18" width="17">
        <path
            d="M15.5036 3.11002L12.5357 15.4055C12.2666 16.5204 10.7637 16.7146 10.22 15.7049L7.4763 10.6094L2.00376 8.65488C0.915938 8.26638 0.891983 6.73663 1.96711 6.31426L13.8314 1.65328C14.7729 1.28341 15.741 2.12672 15.5036 3.11002ZM7.56678 10.6417L7.56645 10.6416C7.56656 10.6416 7.56667 10.6416 7.56678 10.6417L7.65087 10.4062L7.56678 10.6417Z"
            fill="currentColor"
            style={{
                stroke: `var(--cursor-color)`,
                fill: `var(--cursor-color-light)`,
            }}
            stroke="currentColor"
            // className={cn(color ? `stroke-${color}-400 text-${color}-500` : 'stroke-primary-400 text-primary-500')}
            strokeWidth="1.5"
        />
    </svg>
)
type CursorProps = { class?: string; text: string, color?: string, flip?: boolean }

const hexToRGBA = (hex: string, alpha: number = 1) => {
    const r = parseInt(hex.slice(1, 3), 16);
    const g = parseInt(hex.slice(3, 5), 16);
    const b = parseInt(hex.slice(5, 7), 16);
    return `rgba(${r}, ${g}, ${b}, ${alpha})`;
};

export const ReactCursor: React.FC<CursorProps> = ({
    class: className,
    text,
    color = "#3B82F6",
    flip = false,
}) => {
    const [randomVariant, setRandomVariant] = React.useState({});

    React.useEffect(() => {
        const generateRandomVariant = () => {
            const randomX = Math.random() * 40 - 20; // Random value between -20 and 20
            const randomY = Math.random() * 40 - 20; // Random value between -40 and 40
            const randomDuration = 3 + Math.random() * 7    ; // Random duration between 3 and 5 seconds

            return {
                animate: {
                    translateX: [0, randomX, 0],
                    translateY: [0, randomY, 0],
                },
                transition: {
                    duration: randomDuration,
                    repeat: Infinity,
                    repeatType: "reverse" as const,
                    ease: "easeInOut",
                },
            };
        };

        setRandomVariant(generateRandomVariant());
    }, []);

    const cursorElement = <CursorSVG flip={flip} />;
    const textElement = (
        <div
            style={{
                backgroundColor: `var(--cursor-color)`,
                borderColor: `var(--cursor-color-dark)`,
            }}
            className={cn(
                'w-fit rounded-full py-1 px-2 text-white',
            )}
        >
            {text}
        </div>
    );

    const colorStyles = {
        '--cursor-color': color,
        '--cursor-color-light': hexToRGBA(color, 0.7),
        '--cursor-color-dark': hexToRGBA(color, 0.8),
    } as React.CSSProperties;

    return (
        <motion.div
            initial={{ translateX: 0, translateY: 0 }}
            // 
            {...randomVariant}
            style={colorStyles}
            className="flex items-center"
        >
            <div className={cn('flex items-center', className)}>
                {flip ? (
                    <>
                        {cursorElement}
                        {textElement}
                    </>
                ) : (
                    <>
                        {textElement}
                        {cursorElement}
                    </>
                )}
            </div>
        </motion.div>
    );
}



export const Cursor = qwikify$(ReactCursor)