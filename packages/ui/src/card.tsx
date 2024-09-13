import { $, component$ } from "@builder.io/qwik";
import { Modal, Portal } from "@nestri/ui";

type Props = {
    game: {
        name: string;
        id: number;
        modalUrl: string;
    }
}

export const Card = component$(({ game }: Props) => {
    const imageUrl = `https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/${game.id}/library_600x900_2x.jpg`;

    return (
        <Modal.Root class="w-full">
            <Modal.Trigger
                class="min-w-[250px] group hover:ring-primary-500 focus:ring-primary-500 outline-none cursor-pointer backdrop-blur-sm select-none w-full group rounded-[20px] duration-200 flex flex-col ring-gray-800/70 transition-all gap-2 px-4 py-3 ring-[3px] ring-neutral-200 dark:ring-gray-800 text-gray-900/70 dark:text-gray-100/70 bg-gray-200/70 dark:bg-gray-800/70">
                <header class="flex gap-4 max-w-full justify-between p-4">
                    <div class="flex relative pr-[22px] overflow-hidden overflow-ellipsis whitespace-nowrap" >
                        <h3 class="overflow-hidden overflow-ellipsis whitespace-nowrap">{game.name}</h3>
                    </div>
                </header>
                <section class="flex justify-center items-center w-full pb-7">
                    <img
                        src={imageUrl}
                        class="rounded-2xl ring-2 aspect-[2/3] w-full max-w-[90%] ring-gray-900/70 group-hover:scale-105 group-focus:scale-105 duration-200 transition-transform shadow-2xl shadow-gray-950"
                        width={250}
                        height={200}
                        alt={game.name}
                    />
                </section>
            </Modal.Trigger>
            <Modal.Panel
                class="w-full max-w-3xl h-auto aspect-[1.778] backdrop:backdrop-blur-sm backdrop:bg-black/10 dark:backdrop:bg-white/10 bg-white dark:bg-black ring-2 backdrop-blur-md ring-gray-300 dark:ring-gray-700 rounded-xl text-gray-900 dark:text-gray-100 after:pointer-events-none after:select-none after:bg-gradient-to-b after:from-transparent after:dark:to-gray-900 after:to-gray-100 after:fixed after:left-0 after:bottom-0 after:z-10 after:backdrop-blur-sm after:h-[200px] after:w-full after:[-webkit-mask-image:linear-gradient(to_top,theme(colors.primary.100)_50%,transparent)] after:dark:[-webkit-mask-image:linear-gradient(to_top,theme(colors.primary.900)_50%,transparent)]">
                <img src={game.modalUrl} height={500} width={500} class="w-full h-full object-cover" />
                <div class="flex flex-col w-full absolute bottom-0 left-0 z-50 right-0 items-center justify-center gap-4 p-4">
                    <Portal />
                </div>
            </Modal.Panel>
        </Modal.Root>
    )
});