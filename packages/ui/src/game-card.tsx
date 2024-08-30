import { component$ } from "@builder.io/qwik"
import { cn } from "@nestri/ui/design"
import { ImageLoader } from "@nestri/ui/image"

type Game = {
    appid: string
    name: string
    release_date: number
    compatibility: string
    teams: number
}

type Props = {
    game: Game;
    class?: string;
}
export const GameCard = component$(({ game, class: className }: Props) => {
    return (
        <div key={`game-${game.appid}`} class={cn("bg-gray-200/70 min-w-[250px] backdrop-blur-sm ring-gray-300 select-none max-w-[270px] group dark:ring-gray-700 ring dark:bg-gray-800/70 group rounded-3xl dark:text-primary-50/70 text-primary-950/70 duration-300 transition-colors flex flex-col", className)}>
            <header class="flex gap-4 justify-between p-4">
                <div class="flex relative pr-[22px] overflow-hidden overflow-ellipsis whitespace-nowrap" >
                    <h3 class="overflow-hidden overflow-ellipsis whitespace-nowrap">{game.name}</h3>
                    <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" class={cn("absolute right-0 bottom-0 top-0", game.compatibility == "perfect" ? "text-green-600 dark:text-green-400" : "text-amber-600 dark:text-amber-300")} viewBox="0 0 24 24">
                        <path fill="currentColor" d="M9.592 3.2a5.727 5.727 0 0 1-.495.399c-.298.2-.633.338-.985.408c-.153.03-.313.043-.632.068c-.801.064-1.202.096-1.536.214a2.713 2.713 0 0 0-1.655 1.655c-.118.334-.15.735-.214 1.536a5.707 5.707 0 0 1-.068.632c-.07.352-.208.687-.408.985c-.087.13-.191.252-.399.495c-.521.612-.782.918-.935 1.238c-.353.74-.353 1.6 0 2.34c.153.32.414.626.935 1.238c.208.243.312.365.399.495c.2.298.338.633.408.985c.03.153.043.313.068.632c.064.801.096 1.202.214 1.536a2.713 2.713 0 0 0 1.655 1.655c.334.118.735.15 1.536.214c.319.025.479.038.632.068c.352.07.687.209.985.408c.13.087.252.191.495.399c.612.521.918.782 1.238.935c.74.353 1.6.353 2.34 0c.32-.153.626-.414 1.238-.935c.243-.208.365-.312.495-.399c.298-.2.633-.338.985-.408c.153-.03.313-.043.632-.068c.801-.064 1.202-.096 1.536-.214a2.713 2.713 0 0 0 1.655-1.655c.118-.334.15-.735.214-1.536c.025-.319.038-.479.068-.632c.07-.352.209-.687.408-.985c.087-.13.191-.252.399-.495c.521-.612.782-.918.935-1.238c.353-.74.353-1.6 0-2.34c-.153-.32-.414-.626-.935-1.238a5.574 5.574 0 0 1-.399-.495a2.713 2.713 0 0 1-.408-.985a5.72 5.72 0 0 1-.068-.632c-.064-.801-.096-1.202-.214-1.536a2.713 2.713 0 0 0-1.655-1.655c-.334-.118-.735-.15-1.536-.214a5.707 5.707 0 0 1-.632-.068a2.713 2.713 0 0 1-.985-.408a5.73 5.73 0 0 1-.495-.399c-.612-.521-.918-.782-1.238-.935a2.713 2.713 0 0 0-2.34 0c-.32.153-.626.414-1.238.935" opacity=".5" />
                        <path fill="currentColor" d="M16.374 9.863a.814.814 0 0 0-1.151-1.151l-4.85 4.85l-1.595-1.595a.814.814 0 0 0-1.151 1.151l2.17 2.17a.814.814 0 0 0 1.15 0z" />
                    </svg>
                </div>
                <time>{new Date(game.release_date).getUTCFullYear()}</time>
            </header>
            <div class="flex-1 flex items-center justify-center">
                <div class="max-h-64 h-full p-4 2xl:max-h-80 relative">
                    <ImageLoader height={224} width={150} src={game.appid} alt={game.name} class="block h-full mx-auto rounded-2xl shadow-2xl shadow-primary-900 dark:shadow-primary-800 relative" />
                </div>
            </div>
            <footer class="flex justify-between p-4">
                <span class="text-left max-w-[70%]" >
                    {"Downloaded in "}&nbsp;
                    {`${game.teams}`}&nbsp;
                    {"Nestri Teams"}
                </span>
                <div>
                    <div class="flex relative p-3 text-primary-500">
                        <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24"><path fill="currentColor" d="M12.832 21.801c3.126-.626 7.168-2.875 7.168-8.69c0-5.291-3.873-8.815-6.658-10.434c-.619-.36-1.342.113-1.342.828v1.828c0 1.442-.606 4.074-2.29 5.169c-.86.559-1.79-.278-1.894-1.298l-.086-.838c-.1-.974-1.092-1.565-1.87-.971C4.461 8.46 3 10.33 3 13.11C3 20.221 8.289 22 10.933 22q.232 0 .484-.015c.446-.056 0 .099 1.415-.185" opacity=".5" /><path fill="currentColor" d="M8 18.444c0 2.62 2.111 3.43 3.417 3.542c.446-.056 0 .099 1.415-.185C13.871 21.434 15 20.492 15 18.444c0-1.297-.819-2.098-1.46-2.473c-.196-.115-.424.03-.441.256c-.056.718-.746 1.29-1.215.744c-.415-.482-.59-1.187-.59-1.638v-.59c0-.354-.357-.59-.663-.408C9.495 15.008 8 16.395 8 18.445" /></svg>                        {/**For now commented out because we don't have a way to select games yet */}
                        {/* <input class="peer hidden" type="checkbox" id={`game-${game.appid}`} value={game.appid} checked={false} />
                        <label for={`game-${game.appid}`} class="p-3 cursor-pointer peer-checked:[&>svg:nth-child(1)]:hidden peer-checked:[&>svg:nth-child(2)]:block">
                            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" class="block" >
                                <g fill="none" stroke="currentColor" stroke-linecap="round" stroke-width="1.5">
                                    <path d="M6.286 19C3.919 19 2 17.104 2 14.765c0-2.34 1.919-4.236 4.286-4.236c.284 0 .562.028.83.08m7.265-2.582a5.765 5.765 0 0 1 1.905-.321c.654 0 1.283.109 1.87.309m-11.04 2.594a5.577 5.577 0 0 1-.354-1.962C6.762 5.528 9.32 3 12.476 3c2.94 0 5.361 2.194 5.68 5.015m-11.04 2.594a4.29 4.29 0 0 1 1.55.634m9.49-3.228C20.392 8.78 22 10.881 22 13.353c0 2.707-1.927 4.97-4.5 5.52" opacity=".5" />
                                    <path stroke-linejoin="round" d="M12 22v-6m0 6l2-2m-2 2l-2-2" />
                                </g>
                            </svg>
                            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" class="hidden" >
                                <g fill="none" stroke="currentColor" stroke-linecap="round" stroke-width="1.5">
                                    <path d="M6.286 19C3.919 19 2 17.104 2 14.765c0-2.34 1.919-4.236 4.286-4.236c.284 0 .562.028.83.08m7.265-2.582a5.765 5.765 0 0 1 1.905-.321c.654 0 1.283.109 1.87.309m-11.04 2.594a5.577 5.577 0 0 1-.354-1.962C6.762 5.528 9.32 3 12.476 3c2.94 0 5.361 2.194 5.68 5.015m-11.04 2.594a4.29 4.29 0 0 1 1.55.634m9.49-3.228C20.392 8.78 22 10.881 22 13.353c0 2.707-1.927 4.97-4.5 5.52" opacity=".5" />
                                    <path stroke-linejoin="round" d="m10 19.8l1.143 1.2L14 18" />
                                </g>
                            </svg>
                        </label> */}
                    </div>
                </div>
            </footer>
        </div>
    )
})