import { component$ } from "@builder.io/qwik";
import { HomeNavBar, Card, LargeCard } from "@nestri/ui";

function getGreeting(): string {
    const hour = new Date().getHours();
    if (hour < 12) return "Good Morning";
    if (hour < 18) return "Good Afternoon";
    return "Good Evening";
}

export default component$(() => {
    return (
        <>
            <HomeNavBar />
            <section class="flex flex-col gap-4 justify-center pt-20 items-center w-full text-left pb-4">
                <div class="flex flex-col gap-4 mx-auto max-w-2xl w-full">
                    <h1 class="text-5xl font-bold font-title">{getGreeting()},&nbsp;<span>Wanjohi</span></h1>
                    <p class="dark:text-gray-50/70 text-gray-950/70 text-xl">What will you play today?</p>
                </div>
            </section>
            <section class="flex flex-col gap-4 justify-center pt-10 items-center w-full text-left pb-4">
                <ul class="gap-4 relative list-none w-full max-w-xl lg:max-w-4xl grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 after:pointer-events-none after:select-none after:bg-gradient-to-b after:from-transparent after:dark:to-gray-900 after:to-gray-100 after:fixed after:left-0 after:-bottom-[1px] after:z-10 after:backdrop-blur-sm after:h-[100px] after:w-full after:[-webkit-mask-image:linear-gradient(to_top,theme(colors.primary.100)_50%,transparent)] after:dark:[-webkit-mask-image:linear-gradient(to_top,theme(colors.primary.900)_50%,transparent)]">
                    <li class="col-span-full">
                        <LargeCard
                            game={{
                                // release_date: 1478710740000,
                                // compatibility: 'playable',
                                name: 'The Lord of the Rings: Return to Moria™',
                                id: 2933130,
                                // teams: 10
                            }}
                        />
                    </li>
                    <li>
                        <Card
                            game={{
                                // release_date: 1478710740000,
                                // compatibility: 'playable',
                                name: 'The Lord of the Rings: Return to Moria™',
                                id: 2933130,
                                // teams: 10
                            }}
                        />
                    </li>
                    <li>
                        <Card
                            game={{
                                // release_date: 1478710740000,
                                // compatibility: 'playable',
                                name: 'Control Ultimate Edition',
                                id: 870780,
                                // teams: 10
                            }}
                        />
                    </li>
                    <li>
                        <Card
                            game={{
                                // release_date: 1478710740000,
                                // compatibility: 'playable',
                                name: 'Grand Theft Auto V',
                                id: 271590,
                                // teams: 10
                            }}
                        />
                    </li>
                    <li>
                        <Card
                            game={{
                                // release_date: 1478710740000,
                                // compatibility: 'playable',
                                name: 'Apex Legends',
                                id: 1172470,
                                // teams: 10
                            }}
                        />
                    </li>
                    <li>
                        <Card
                            game={{
                                // release_date: 1478710740000,
                                // compatibility: 'playable',
                                name: "Tom Clancy's Rainbow Six Siege",
                                id: 359550,
                                // teams: 10
                            }}
                        />
                    </li>
                </ul>
            </section>
            <nav class="w-full flex justify-center h-[100px] z-50 items-center gap-4 bg-transparent fixed -bottom-[1px] left-0 right-0">
                <nav class="flex gap-4 w-max px-4 py-2 rounded-full bg-neutral-900 ring-gray-700 ring-2">
                    <button class="text-gray-100/70 text-xl font-title">
                        <span class="material-symbols-outlined">
                            home
                        </span>
                    </button>
                    <button class="text-gray-100/70 text-xl font-title">
                        <span class="material-symbols-outlined">
                            home
                        </span>
                    </button>
                </nav>
            </nav>
        </>
    )
})