import { $, component$, useSignal } from "@builder.io/qwik";
import { Modal, Portal } from "@nestri/ui";

type Props = {
    game: {
        name: string;
        id: number;
    }
}

export const LargeCard = component$(({ game }: Props) => {
    const imageUrl = `https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/${game.id}/header.jpg`;
    const modalUrl = "https://images.igdb.com/igdb/image/upload/t_720p/sc8qxw.webp"
    const modalTitleUrl = "https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/870780/logo.png"

    return (
        <Modal.Root class="w-full">
            <Modal.Trigger
                class="min-w-[250px] h-[350px] lg:flex-row flex-col group overflow-hidden hover:ring-primary-500 focus:ring-primary-500 outline-none cursor-pointer backdrop-blur-sm select-none w-full group rounded-[20px] duration-200 flex transition-all gap-2 px-4 py-3 ring-[3px] ring-gray-200 dark:ring-gray-800 text-gray-900/70 dark:text-gray-100/70 bg-gray-200/70 dark:bg-gray-800/70">
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
                                        <img src={`https://nexus.nestri.workers.dev/image/avatar/avatar-${index + 1}.png`} height={32} width={32} class="rounded-full size-full" />
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
                    <img
                        src={imageUrl}
                        class="lg:rounded-tr-none rounded-t-xl lg:m-0 mx-4 ring-2 aspect-[92/43] w-full ring-gray-900/70 group-hover:scale-105 group-focus:scale-105 duration-200 transition-transform shadow-2xl shadow-gray-950"
                        width={250}
                        height={400}
                        alt={game.name}
                    />
                </section>
            </Modal.Trigger>
            <Modal.Panel
                class="w-full overflow-hidden max-w-3xl h-auto aspect-[1.778] backdrop:backdrop-blur-sm backdrop:bg-black/10 dark:backdrop:bg-white/10 bg-white dark:bg-black ring-2 backdrop-blur-md ring-gray-300 dark:ring-gray-700 rounded-xl text-gray-900 dark:text-gray-100 after:pointer-events-none after:select-none after:bg-gradient-to-b after:from-transparent after:dark:to-gray-900 after:to-gray-100 after:fixed after:left-0 after:bottom-0 after:z-10 after:backdrop-blur-sm after:h-[200px] after:w-full after:[-webkit-mask-image:linear-gradient(to_top,theme(colors.primary.100)_50%,transparent)] after:dark:[-webkit-mask-image:linear-gradient(to_top,theme(colors.primary.900)_50%,transparent)]">
                <div class="w-full h-full before:absolute before:left-0 before:top-0 before:w-full before:animate-zoom-out before:h-full before:bg-[url('https://images.igdb.com/igdb/image/upload/t_720p/sc8qxw.webp')] before:bg-cover before:bg-center before:bg-no-repeat after:absolute after:left-0 after:top-[10%] after:w-full after:h-full after:bg-[url('https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/870780/logo.png')] after:bg-[center_top] after:bg-no-repeat" />
                <div class="flex flex-col w-full absolute bottom-0 left-0 z-50 right-0 items-center justify-center gap-4 p-4">
                    <Portal />
                </div>
            </Modal.Panel>
        </Modal.Root>
    )
});