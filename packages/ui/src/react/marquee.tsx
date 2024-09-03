import { cn } from "@nestri/ui/design";
import { component$, useStore, type Component } from "@builder.io/qwik";


interface MarqueeProps<T> {
    parentClass?: string;
    itemClass?: string;
    items: T[];
    pauseOnHover?: boolean;
    renderItem: Component<{ item: T; index: number }>;
    reverse?: boolean;
    direction?: 'horizontal' | 'vertical';
    pad?: boolean;
    translate?: 'track' | 'items';
    state?: 'running' | 'paused';
    spill?: boolean;
    diff?: boolean;
    inset?: number;
    outset?: number;
    speed?: number;
    scale?: number;
}

const renderStamp = Date.now()

export const Marquee = component$(<T,>({
    parentClass,
    itemClass,
    reverse = false,
    items,
    renderItem: RenderItem,
    direction = 'horizontal',
    translate = 'items',
    state = 'running',
    pad = false,
    spill = false,
    diff = false,
    inset = 0,
    outset = 0,
    pauseOnHover = false,
    speed = 10,
    scale = 1,
}: MarqueeProps<T>) => {
    const store = useStore({
        indices: Array.from({ length: items.length }, (_, i) => i),
    });

    return (
        <div
            class={cn("marquee-container", parentClass)}
            data-direction={direction}
            data-pad={pad}
            data-pad-diff={diff}
            data-pause-on-hover={pauseOnHover ? 'true' : 'false'}
            data-translate={translate}
            data-play-state={state}
            data-spill={spill}
            data-reverse={reverse}
            style={{ '--speed': speed, '--count': items.length, '--scale': scale, '--inset': inset, '--outset': outset }}
        >
            <ul>
                {pad && translate === 'track'
                    ? store.indices.map((index) => {
                        return (
                            <li
                                aria-hidden="true"
                                class={cn("pad pad--negative", itemClass)}
                                key={`pad-negative-${renderStamp}--${index}`}
                            >
                                <RenderItem item={items[index]} index={index} />
                            </li>
                        )
                    })
                    : null}
                {store.indices.map((index) => {
                    return (
                        <li key={`index-${renderStamp}--${index}`} style={{ '--index': index }}>
                            <RenderItem item={items[index]} index={index} />
                        </li>
                    )
                })}
                {pad && translate === 'track'
                    ? store.indices.map((index) => {
                        return (
                            <li
                                aria-hidden="true"
                                class={cn("pad pad--positive", itemClass)}
                                key={`pad-positive-${renderStamp}--${index}`}
                            >
                                <RenderItem item={items[index]} index={index} />
                            </li>
                        )
                    })
                    : null}
            </ul>
        </div>
    );
})