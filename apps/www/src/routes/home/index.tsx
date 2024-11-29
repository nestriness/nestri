import { component$ } from "@builder.io/qwik";
import { HomeNavBar, Card } from "@nestri/ui";

function getGreeting(): string {
    const hour = new Date().getHours();
    if (hour >= 5 && hour < 12) return "Good Morning";
    if (hour >= 12 && hour < 18) return "Good Afternoon";
    return "Good Evening";
}

export default component$(() => {
    return (
        <>
            <HomeNavBar />
            {/* <div class="bg-red-500 h-[66px] w-screen"></div> */}
            <section class="absolute flex mx-auto my-0 inset-[0_0_20%] overflow-hidden -z-[1] before:inset-0 before:absolute before:z-[1] after:absolute after:inset-0 after:[background:linear-gradient(180deg,transparent_60%,#000)] before:[background:linear-gradient(90deg,transparent_85%,#000),linear-gradient(-90deg,transparent_85%,#000)]" >
                <img src="https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/1172470/library_hero_2x.jpg" height={200} width={300} class="max-w-full min-w-full max-h-full min-h-full object-cover absolute top-0 bottom-0 left-0 right-0 w-0 h-0"/>
            </section>
            
            {/* <section class="w-full before:inset-0 before:absolute before:z-[1] relative after:bg-gradient-to-b after:from-transparent after:from-60% after:to-black before:[background:linear-gradient(90deg,transparent_70%,#000),linear-gradient(-90deg,transparent_70%,#000)]" >
                <img src="https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/359550/library_hero_2x.jpg" height={200} width={300} class="w-full aspect-[96/31]"/>
            </section><section class="w-full before:inset-0 before:absolute before:z-[1] relative after:bg-gradient-to-b after:from-transparent after:from-60% after:to-black before:[background:linear-gradient(90deg,transparent_70%,#000),linear-gradient(-90deg,transparent_70%,#000)]" >
                <img src="https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/359550/library_hero_2x.jpg" height={200} width={300} class="w-full aspect-[96/31]"/>
            </section><section class="w-full before:inset-0 before:absolute before:z-[1] relative after:bg-gradient-to-b after:from-transparent after:from-60% after:to-black before:[background:linear-gradient(90deg,transparent_70%,#000),linear-gradient(-90deg,transparent_70%,#000)]" >
                <img src="https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/359550/library_hero_2x.jpg" height={200} width={300} class="w-full aspect-[96/31]"/>
            </section><section class="w-full before:inset-0 before:absolute before:z-[1] relative after:bg-gradient-to-b after:from-transparent after:from-60% after:to-black before:[background:linear-gradient(90deg,transparent_70%,#000),linear-gradient(-90deg,transparent_70%,#000)]" >
                <img src="https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/359550/library_hero_2x.jpg" height={200} width={300} class="w-full aspect-[96/31]"/>
            </section> */}
            {/* <section class="flex flex-col gap-4 justify-center pt-20 items-center w-full text-left pb-4">
                <div class="flex flex-col gap-4 mx-auto max-w-2xl w-full">
                    <h1 class="text-5xl font-bold font-title">{getGreeting()},&nbsp;<span>Wanjohi</span></h1>
                    <p class="dark:text-gray-50/70 text-gray-950/70 text-xl">What will you play today?</p>
                </div>
            </section> */}
            {/* <section class="flex flex-col gap-4 justify-center pt-10 items-center w-full text-left pb-4">
                <ul class="gap-4 relative list-none w-full max-w-xl lg:max-w-4xl grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 after:pointer-events-none after:select-none after:bg-gradient-to-b after:from-transparent after:dark:to-gray-900 after:to-gray-100 after:fixed after:left-0 after:-bottom-[1px] after:z-10 after:backdrop-blur-sm after:h-[100px] after:w-full after:[-webkit-mask-image:linear-gradient(to_top,theme(colors.primary.100)_50%,transparent)] after:dark:[-webkit-mask-image:linear-gradient(to_top,theme(colors.primary.900)_50%,transparent)]">
                    <li class="col-span-full">
                        <Card
                            size="large"
                            titleWidth={55.61}
                            titleHeight={100}
                            game={{
                                name: 'Control Ultimate Edition',
                                id: 870780
                            }}
                        />
                    </li>
                    <li>
                        <Card
                            size="small"
                            titleWidth={56.30}
                            titleHeight={69.79}
                            game={{
                                name: 'Black Myth: Wukong',
                                id: 2358720,
                            }}
                        />
                    </li>
                    <li>
                        <Card
                            size="small"
                            titleWidth={34.09}
                            titleHeight={98.26}
                            game={{
                                name: 'The Lord of the Rings: Return to Moria™',
                                id: 2933130,
                            }}
                        />
                    </li>
                    <li>
                        <Card
                            size="small"
                            titleWidth={48.77}
                            titleHeight={100}
                            game={{
                                name: 'Grand Theft Auto V',
                                id: 271590,
                            }}
                        />
                    </li>
                    <li>
                        <Card
                            titleWidth={31.65}
                            titleHeight={82.87}
                            size="small"
                            game={{
                                name: 'Apex Legends',
                                id: 1172470,
                            }}
                        />
                    </li>
                    <li>
                        <Card
                            size="small"
                            titleHeight={99.75}
                            titleWidth={73.44}
                            game={{
                                name: "Tom Clancy's Rainbow Six Siege",
                                id: 359550,
                            }}
                        />
                    </li>
                </ul>
            </section> */}
        </>
    )
})