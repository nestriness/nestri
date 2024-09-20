import { component$ } from "@builder.io/qwik";

const games = [
    {
        cover: 'https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/444200/library_600x900_2x.jpg',
        release_date: 1478710740000,
        compatibility: 'playable',
        name: 'World of Tanks Blitz',
        appid: 444200
    },
    {
        cover: 'https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/1085660/library_600x900_2x.jpg',
        release_date: 1569949200000,
        compatibility: 'unsupported',
        name: 'Destiny 2',
        appid: 1085660
    },
    {
        cover: 'https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/1172470/library_600x900_2x.jpg',
        release_date: 1604548800000,
        compatibility: 'playable',
        name: 'Apex Legends',
        appid: 1172470
    },
    {
        cover: 'https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/1229380/library_600x900_2x.jpg',
        release_date: 1614870001000,
        compatibility: 'perfect',
        name: 'Everhood',
        appid: 1229380
    },
    {
        cover: 'https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/1637320/library_600x900_2x.jpg',
        release_date: 1664296270000,
        compatibility: 'perfect',
        name: 'Dome Keeper',
        appid: 1637320
    },
    {
        cover: 'https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/2581970/library_600x900_2x.jpg',
        release_date: 1698246127000,
        compatibility: 'playable',
        name: 'Shell Runner - Prelude',
        appid: 2581970
    }
]

export default component$(() => {
    return (
        <>
            <div class="w-full justify-center py-20 gap-10 flex flex-col px-6" >
                <div class="mx-auto w-full items-center max-w-xl flex gap-3 flex-col">
                    <span class="text-base underline underline-offset-4 px-2">Step 1 of 2</span>
                    <h1 class="text-4xl font-bold font-title ">
                        Let's play something cool
                    </h1>
                    <p class="text-lg font-medium text-gray-600/70 dark:text-gray-400/70">
                        Choose a game to play from your Steam library
                    </p>
                </div>
                <ul class="w-full max-w-xl mx-auto grid gap-2 grid-cols-2 sm:grid-cols-2 md:grid-cols-3" >
                    {games.map((game) => (
                        <li key={game.appid}>
                            <div class="rounded-2xl cursor-pointer ring-2 hover:ring-primary-500 ring-gray-200 dark:ring-gray-700 bg-white dark:bg-black overflow-hidden group gap-3 transition-all duration-200 p-5 flex-col aspect-[14/12] relative">
                                <div class="absolute inset-0 p-0">
                                    <img height={133} width={95} src={game.cover} alt={game.name} class="object-top h-full w-full object-cover rounded-lg blur-xl opacity-20" />
                                </div>
                                <h2 class="relative font-semibold">{game.name}</h2>
                                <div class="absolute transition-all duration-100 -bottom-16 rotate-6 left-1/2 -translate-x-1/2 w-1/2 flex items-center justify-center aspect-[10/14] rounded-sm group-hover:rotate-[5deg] group-hover:shadow-lg group-hover:shadow-gray-400 dark:group-hover:shadow-gray-600 group-hover:scale-105">
                                    <img height={133} width={95} src={`https://nexus.nestri.workers.dev/image/cover/${game.appid}.avif?width=400`} alt="Logo" class="pointer-events-none object-top h-full object-cover w-full rounded-lg aspect-[10/14]" />
                                </div>
                            </div>
                        </li>
                    ))}
                </ul>
            </div>
        </>
    )
})