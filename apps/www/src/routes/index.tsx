import { component$ } from "@builder.io/qwik";
import { NavBar } from "@nestri/ui";
import { HeroSection, Example } from "@nestri/ui/react";

export default component$(() => {
  return (
    <>
      <NavBar />
      {/* <HeroSection client:load>
        <button class="w-full max-w-xl rounded-xl flex items-center justify-between hover:bg-gray-300/70 dark:hover:bg-gray-700/70 transition-colors gap-2 px-4 py-3 h-[45px] border border-gray-300 dark:border-gray-700 mx-autotext-gray-500/70 bg-gray-200/70 dark:bg-gray-800/70">
          <span class="flex items-center gap-3 h-max justify-center overflow-hidden overflow-ellipsis whitespace-nowrap">
            <svg xmlns="http://www.w3.org/2000/svg" class="size-[18] flex-shrink-0" height="18" width="18" viewBox="0 0 24 24"><g fill="none" stroke="currentColor" stroke-width="2"><circle cx="11.5" cy="11.5" r="9.5" /><path stroke-linecap="round" d="M18.5 18.5L22 22" /></g></svg>
            Search for a game to play...
          </span>
          <span class="flex items-center gap-2">
            <div class="flex items-center gap-2 text-base font-title font-bold">
              <kbd class="border-neutral-300 dark:border-neutral-700 border px-2 py-1 rounded-md">
                <svg xmlns="http://www.w3.org/2000/svg" class="size-5 flex-shrink-0" width="20" height="20" viewBox="0 0 256 256"><path fill="currentColor" d="M180 144h-20v-32h20a36 36 0 1 0-36-36v20h-32V76a36 36 0 1 0-36 36h20v32H76a36 36 0 1 0 36 36v-20h32v20a36 36 0 1 0 36-36m-20-68a20 20 0 1 1 20 20h-20ZM56 76a20 20 0 0 1 40 0v20H76a20 20 0 0 1-20-20m40 104a20 20 0 1 1-20-20h20Zm16-68h32v32h-32Zm68 88a20 20 0 0 1-20-20v-20h20a20 20 0 0 1 0 40" /></svg>
              </kbd>
              <span class="px-2 py-0.5 rounded-md border border-neutral-300 dark:border-neutral-700">
                K
              </span>
            </div>
          </span>
        </button>
      </HeroSection> */}
      {/* <Example client:load /> */}
    </>
  );
});