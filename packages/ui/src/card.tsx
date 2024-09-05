import { component$ } from "@builder.io/qwik";

type Props = {
    game: {
        name: string;
        id: number;
    }
}

export const Card = component$(({ game }: Props) => {
    const imageUrl = `http://localhost:8787/image/cover/${game.id}.avif`

    return (
        <button
            class="min-w-[250px] group hover:ring-primary-500 focus:ring-primary-500 outline-none cursor-pointer backdrop-blur-sm select-none w-full group rounded-[20px] duration-200 flex flex-col ring-gray-900/70 transition-all gap-2 px-4 py-3 ring-[3px] ring-neutral-300 dark:ring-gray-700 text-gray-900/70 dark:text-gray-100/70 bg-gray-300/70 dark:bg-gray-700/70">
            <header class="flex gap-4 max-w-full justify-between p-4">
                <div class="flex relative pr-[22px] overflow-hidden overflow-ellipsis whitespace-nowrap" >
                    <h3 class="overflow-hidden overflow-ellipsis whitespace-nowrap">{game.name}</h3>
                </div>
            </header>
            <section class="flex justify-center items-center w-full py-7">
                <img
                    src={imageUrl}
                    class="rounded-2xl ring-2 aspect-[2/3] w-full max-w-[90%] ring-gray-900/70 group-hover:scale-105 group-focus:scale-105 duration-200 transition-transform shadow-2xl shadow-gray-950"
                    width={250}
                    height={200}
                    alt={game.name}
                    crossOrigin="anonymous"
                />
            </section>
        </button>
    )
});