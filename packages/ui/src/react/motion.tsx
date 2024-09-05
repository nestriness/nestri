/** @jsxImportSource react */
import { qwikify$ } from '@builder.io/qwik-react';
import { motion, type MotionProps } from 'framer-motion';
import { type ReactNode } from 'react';

interface MotionComponentProps extends MotionProps {
    as?: keyof JSX.IntrinsicElements;
    children?: ReactNode;
    class?: string;
}

export const transition = {
    type: "spring",
    stiffness: 100,
    damping: 15,
    restDelta: 0.001,
    duration: 0.01,
    delay: 0.4,
}

export const ReactMotionComponent = ({
    as = 'div',
    children,
    class: className,
    ...motionProps
}: MotionComponentProps) => {
    const MotionTag = motion[as as keyof typeof motion];

    return (
        <MotionTag
            //@ts-ignore
            className={className}
            //@ts-ignore
            {...motionProps}
        // animate={isInView ? whileInView : undefined}
        >
            {children}
        </MotionTag>
    );
};

export const MotionComponent = qwikify$(ReactMotionComponent);