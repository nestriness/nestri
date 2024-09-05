import { component$ } from "@builder.io/qwik";

type Props = {
    game: {
        name: string;
        id: number;
    }
}

export const LargeCard = component$(({ game }: Props) => {
    // const imageUrl = `https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/${game.id}/library_600x900_2x.jpg`;
    const imageUrl = `https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/${game.id}/header.jpg`;

    return (
        <button
            class="min-w-[250px] h-[350px] lg:flex-row flex-col group col-span-full overflow-hidden hover:ring-primary-500 focus:ring-primary-500 outline-none cursor-pointer backdrop-blur-sm select-none w-full group rounded-[20px] duration-200 flex ring-gray-900/70 transition-all gap-2 px-4 py-3 ring-[3px] ring-neutral-300 dark:ring-gray-700 text-gray-900/70 dark:text-gray-100/70 bg-gray-300/70 dark:bg-gray-700/70">
            <header class="flex gap-4 lg:w-1/3 w-full justify-between p-4">
                <div class="flex flex-col gap-4 text-left relative overflow-hidden" >
                    <h3 class="text-base">Continue Playing</h3>
                    <h3 class="font-semibold font-title text-3xl">{game.name}</h3>
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
        </button>
    )
});