import { component$ } from "@builder.io/qwik";
import { HomeNavBar } from "@nestri/ui";

const icons = [
    // {
    //     icon: "https://framerusercontent.com/images/tvMz2bcYNcZtI2YPD2blDNogzDA.png",
    //     label: "Finder"
    // },
    // {
    //     icon: "https://framerusercontent.com/images/lykOj3xsaQWFncrRJf0PQaJC0.png",
    //     label: "Multi"
    // }, 
      {
        icon: "https://cdn-1.webcatalog.io/catalog/steam-web-store/steam-web-store-icon-filled-256.webp?v=1714775986747",
        label: "Steam"
    }, 
    {
        icon: "https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/1091500/3d642f225dfb69962f5d8d36f7868caf9febf90d.ico",
        label: "Cyberpunk 2077"
    },{
        icon: "https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/1568400/f954b5bd7c6a041a73ab8362dd207e4b79d57a37.ico",
        label: "Sheepy: A Short Adventure"
    },{
        icon: "https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/1174180/5bf6edd7efb1110b457da905e7ac696c6c619ed1.ico",
        label: "Red Dead Redemption 2"
    },{
        icon: "https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/1663220/0c54ad0aa052c7218540416512c71a860a382388.ico",
        label: "Coffee Talk Episode 2: Hibiscus & Butterfly"
    },
    // {
    //     icon: "https://framerusercontent.com/images/L5EHzir7oVTY67QVRBnRAjAugXk.png",
    //     label: "Sally's Xcode"
    // }, {
    //     icon: "https://framerusercontent.com/images/CWJH4LdVKLxfs83b5rUDcjSHM.png",
    //     label: "Kim's Diagram"
    // }, 
    {
        icon: "https://framerusercontent.com/images/lhfyHPlKFxlLMSiIJMr053Ewd0.png",
        label: "Bin"
    },

]

export default component$(() => {
    return (
        <>
            <HomeNavBar />
            {/* <div class="bg-red-500 h-[66px] w-screen"></div> */}
            {/* <section class="absolute flex mx-auto my-0 inset-[0_0_20%] overflow-hidden -z-[1] before:inset-0 before:absolute before:z-[1] after:absolute after:inset-0 after:[background:linear-gradient(180deg,transparent_60%,#000)] before:[background:linear-gradient(90deg,transparent_85%,#000),linear-gradient(-90deg,transparent_85%,#000)]" >
                <img src="https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/1172470/library_hero_2x.jpg" height={200} width={300} class="max-w-full min-w-full max-h-full min-h-full object-cover absolute top-0 bottom-0 left-0 right-0 w-0 h-0"/>
            </section> */}

            <section class="w-full top-[70px] pb-5 ring-gray-300 ring-2 max-w-3xl rounded-xl overflow-hidden relative h-auto shadow-xl">
                <div class="w-full h-auto relative">
                    <img src="https://media.rawg.io/media/games/511/5118aff5091cb3efec399c808f8c598f.jpg" height={200} width={300} class="w-full aspect-[16/9]  object-cover" />
                    <img src="https://shared.cloudflare.steamstatic.com/store_item_assets/steam/apps/1174180/logo_2x.png?t=1671484934" height={200} width={300} class="w-[40%] aspect-[16/9] absolute bottom-4 left-1/2 -translate-x-1/2  object-cover" />
                </div>
                <div class="px-6 pt-2">
                    <div class="flex  gap-2 items-center h-max">
                        {/* <img src="https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/1174180/5bf6edd7efb1110b457da905e7ac696c6c619ed1.ico" height={20} width={20} class="size-10 bg-black aspect-square rounded-xl ring-2 ring-gray-700" /> */}
                        <p class="text-2xl font-title font-bold">Red Dead Redemption 2</p>
                    </div>
                </div>
            </section>
            <div class="absolute bottom-0 w-full">
                <div class="mx-auto w-max h-[64px] gap-1 items-end relative justify-center py-1 px-1.5 flex-row -translate-y-2 flex border backdrop-blur-xl bg-black/30 rounded-2xl [box-shadow:rgba(0,0,0,0.15)_0px_0px_29p_0px,rgba(255,255,255,0.08)_0px_0px_0px_1px] ">
                    {icons.map((icon, key) => (
                        <div key={`icon-${key}`} class="relative size-[56px] group flex items-center justify-center hover:cursor-pointer hover:size-[80px] transition-all duration-200 rounded-3xl">
                            <img draggable={false} src={icon.icon} height={56} width={56} class="w-full h-full rounded-2xl group-hover:rounded-2xl" alt="Icon" />
                            <div class="hidden group-hover:block absolute text-white text-center z-[1] w-max px-4 py-1 bottom-[90px] bg-gray-800 rounded-md transition-opacity duration-[.6s] after:top-full after:absolute after:border-[7px] after:-ml-[5px] after:left-1/2 after:[border-color:theme(colors.gray.800)_transparent_transparent_transparent] font-title">
                                {icon.label}
                            </div>
                        </div>
                    ))}
                    {/* <div class="size-[56px] hover:cursor-pointer hover:size-[80px] transition-all duration-200 relative rounded-3xl">
                        <img draggable={false} src="https://framerusercontent.com/images/lykOj3xsaQWFncrRJf0PQaJC0.png" height={56} width={56} class="w-full h-full" alt="Icon" />
                    </div>
                    <div class="size-[56px] hover:cursor-pointer hover:size-[80px] transition-all duration-200 relative rounded-3xl">
                        <img draggable={false} src="https://framerusercontent.com/images/L5EHzir7oVTY67QVRBnRAjAugXk.png" height={56} width={56} class="w-full h-full" alt="Icon" />
                    </div>
                    <div class="size-[56px] hover:cursor-pointer hover:size-[80px] transition-all duration-200 relative rounded-3xl">
                        <img draggable={false} src="https://framerusercontent.com/images/CWJH4LdVKLxfs83b5rUDcjSHM.png" height={56} width={56} class="w-full h-full" alt="Icon" />
                    </div>
                    <div class="bg-white/40 mb-1 h-12 w-[2px]" />
                    <div class="size-[56px] hover:cursor-pointer hover:size-[80px] transition-all duration-200 -left-0.5 relative rounded-3xl">
                        <img draggable={false} src="https://framerusercontent.com/images/lhfyHPlKFxlLMSiIJMr053Ewd0.png" height={56} width={56} class="w-full h-full" alt="Icon" />
                    </div> */}
                </div>
            </div>
        </>
    )
})