import { $, component$, type PropsOf, useVisibleTask$ } from "@builder.io/qwik";
import { Modal, Portal } from "@nestri/ui";
import { cn } from "@nestri/ui/design"
import { BasicImageLoader } from "./image";

interface Props extends PropsOf<"div"> {
    game: {
        name: string;
        id: number;
    },
    size: "xs" | "small" | "large";
    titleWidth: number;
    titleHeight: number;
}

//TODO: Show gradient animation while image loads

export const Card = component$(({ titleWidth, titleHeight, game, size, ...props }: Props) => {

    const modalUrl = `https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/${game.id}/library_hero.jpg`;
    const imageUrl = size == "large" ? `https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/${game.id}/header.jpg` : `https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/${game.id}/library_600x900_2x.jpg`;
    const modalTitleUrl = `https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/${game.id}/logo.png`
    const loadModalImages = $(() => {
        return Promise.all([modalUrl, modalTitleUrl].map(url => {
            return new Promise((resolve, reject) => {
                const img = new Image();
                // img.crossOrigin = "anonymous"
                img.onload = () => resolve(img);
                img.onerror = reject;
                img.src = url;
            });
        }));
    });

    // eslint-disable-next-line qwik/no-use-visible-task
    useVisibleTask$(async () => {
        await loadModalImages();
    });

    return (
        <Modal.Root
            class="w-full"
            {...(props as any)}>
            {size === "large" ? (
                <Modal.Trigger
                    class="min-w-[250px] h-[350px] lg:flex-row flex-col group overflow-hidden hover:ring-primary-500 focus:ring-primary-500 outline-none cursor-pointer backdrop-blur-sm select-none w-full group rounded-[20px] duration-200 flex transition-all gap-2 px-4 py-3 ring-[3px] ring-gray-200 dark:ring-gray-800 text-gray-900/70 dark:text-gray-100/70 bg-gray-200/70 dark:bg-gray-800/70"
                    >
                    <header class="flex gap-4 lg:w-1/3 h-full w-full justify-between p-4">
                        <div class="flex flex-col gap-4 text-left h-full justify-between relative overflow-hidden p-4" >
                            <div class="flex flex-col gap-2">
                                <h3 class="text-base">Continue Playing</h3>
                                <h2 class="font-semibold font-title text-3xl">{game.name}</h2>
                            </div>
                            <div class="flex pt-2 flex-col justify-center gap-2">
                                <div class="flex -space-x-2">
                                    {[1, 2, 3, 4, 5].map((_, index) => (
                                        <div key={index} class="inline-block size-6 rounded-full ring-[3px] ring-gray-300/70 dark:ring-gray-700/70 bg-gray-700" style={{ zIndex: 5 + index }}>
                                            <BasicImageLoader class="rounded-full size-full" width={24} height={24} src={`https://nexus.nestri.workers.dev/image/avatar/avatar-${index + 1}.png`} alt={`avatar-${index + 1}`} />
                                        </div>
                                    ))}
                                </div>
                                <span class="text-sm max-w-[70%]">
                                    <span class="font-semibold font-title">JD the Smith</span>&nbsp;
                                    {"and"}&nbsp;
                                    {"15 others"}&nbsp;
                                    {"are playing"}
                                </span>
                            </div>
                        </div>
                    </header>
                    <section class="flex justify-center lg:left-1/3 lg:absolute relative items-center lg:w-3/4 w-full lg:top-10 lg:right-0">
                        <BasicImageLoader
                            class="lg:rounded-tr-none rounded-t-xl lg:m-0 mx-4 ring-2 aspect-[92/43] w-full ring-gray-900/70 group-hover:scale-105 group-focus:scale-105 duration-200 transition-transform shadow-2xl shadow-gray-950"
                            width={250}
                            height={400}
                            src={imageUrl}
                            alt={game.name}
                        />
                    </section>
                </Modal.Trigger>) : size == "small" ? (
                    <Modal.Trigger
                        class="min-w-[250px] group hover:ring-primary-500 focus:ring-primary-500 outline-none cursor-pointer backdrop-blur-sm select-none w-full group rounded-[20px] duration-200 flex flex-col ring-gray-800/70 transition-all gap-2 px-4 py-3 ring-[3px] ring-neutral-200 dark:ring-gray-800 text-gray-900/70 dark:text-gray-100/70 bg-gray-200/70 dark:bg-gray-800/70"
                        >
                        <header class="flex gap-4 max-w-full justify-between p-4">
                            <div class="flex relative pr-[22px] overflow-hidden overflow-ellipsis whitespace-nowrap" >
                                <h3 class="overflow-hidden overflow-ellipsis whitespace-nowrap">{game.name}</h3>
                            </div>
                        </header>
                        <section class="flex justify-center items-center w-full pb-7">
                            <BasicImageLoader
                                class="rounded-2xl ring-2 w-full max-w-[90%] ring-gray-900/70 group-hover:scale-105 group-focus:scale-105 duration-200 transition-transform shadow-2xl shadow-gray-950"
                                width={230}
                                height={345}
                                src={imageUrl}
                                alt={game.name}
                            />
                        </section>
                    </Modal.Trigger>
                ) : (
                <Modal.Trigger class="w-full h-full">
                    <BasicImageLoader width={251} height={314} src={imageUrl} alt={game.name} />
                </Modal.Trigger>
            )}
            <Modal.Panel
                class="lg:w-full w-[calc(100%-1rem)] overflow-hidden max-w-3xl h-full max-h-[396px] aspect-[1536/496] backdrop:backdrop-blur-sm backdrop:bg-black/10 dark:backdrop:bg-white/10 bg-white dark:bg-black ring-2 backdrop-blur-md ring-gray-700 rounded-xl text-gray-900 dark:text-gray-100 after:pointer-events-none after:select-none after:bg-gradient-to-b after:from-transparent after:to-gray-900 after:fixed after:left-0 after:bottom-0 after:z-10 after:backdrop-blur-sm after:h-[200px] after:w-full after:[-webkit-mask-image:linear-gradient(to_top,theme(colors.primary.900)_50%,transparent)]">
                <div
                    style={{
                        '--before-url': `url('${modalUrl}')`,
                        '--title-url': `url('${modalTitleUrl}')`,
                        '--after-width': `${titleWidth}%`,
                        '--after-height': `${titleHeight}%`
                    }}
                    class={cn(
                        "w-full rounded-xl mx-auto inset-0 h-full relative flex overflow-hidden",
                        "before:absolute before:inset-0 before:w-full before:animate-zoom-out before:h-full before:bg-cover before:bg-center before:bg-no-repeat before:bg-[url:var(--before-url)]",
                        "after:absolute after:w-[var(--after-width)] after:bg-contain after:h-[var(--after-height)] after:left-0 after:m-auto after:right-0 after:bottom-0 after:top-0 after:bg-center after:bg-no-repeat after:bg-[url:var(--title-url)]"
                    )} />
                <div class="flex flex-col w-full absolute bottom-0 left-0 z-50 right-0 items-center justify-center gap-4 p-4">
                    <Portal />
                </div>
            </Modal.Panel>
        </Modal.Root >
    )
});