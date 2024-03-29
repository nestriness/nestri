import { component$ } from "@builder.io/qwik";

export default component$(() => {
  return (
    <div class='justify-center items-center w-screen h-screen flex flex-col gap-3' >
      <h1 class='text-3xl' >Hi 👋</h1>
      <p class='text-xl' >
        Can't wait to see what you build with qwik!
        <br />
        Happy coding.
      </p>
    </div>
  );
});