import { component$ } from "@builder.io/qwik";
import { GameCard, HomeNavBar, Card } from "@nestri/ui";

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
                <div class="flex flex-col gap-4 mx-auto max-w-xl w-full">
                    <h1 class="text-5xl font-bold font-title">{getGreeting()}, Wanjohi</h1>
                    <p class="dark:text-gray-50/70 text-gray-950/70 text-xl">What will you play today?</p>
                </div>
            </section>
            <section class="flex flex-col gap-4 justify-center pt-10 items-center w-full text-left pb-4">
                <div class="flex gap-4 mx-auto max-w-xl w-full">
                    {/* <GameCard
                        game={{
                            release_date: 1478710740000,
                            compatibility: 'playable',
                            name: 'World of Tanks Blitz',
                            appid: '444200',
                            teams: 10
                        }}
                    /><GameCard
                        game={{
                            release_date: 1478710740000,
                            compatibility: 'playable',
                            name: 'World of Tanks Blitz',
                            appid: '444200',
                            teams: 10
                        }}
                    /> */}
                </div>
                <div class="gap-4 mx-auto max-w-xl w-full grid grid-cols-1 md:grid-cols-2">
                    <Card
                        game={{
                            // release_date: 1478710740000,
                            // compatibility: 'playable',
                            name: 'The Lord of the Rings: Return to Moria™',
                            id: 2933130,
                            // teams: 10
                        }}
                    /><Card
                        game={{
                            // release_date: 1478710740000,
                            // compatibility: 'playable',
                            name: 'Control Ultimate Edition',
                            id: 870780,
                            // teams: 10
                        }}
                    />
                </div>
            </section>
        </>
    )
})