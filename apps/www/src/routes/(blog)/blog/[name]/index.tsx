import { component$ } from "@builder.io/qwik";
import { allBlogs } from "content-collections";
import { useLocation, type StaticGenerateHandler } from '@builder.io/qwik-city';

export default component$(() => {
  const { params } = useLocation();
  const blog = allBlogs.find((p) => p.slug === params.name);

  // if (!blog) {
    return (
      <div class="w-screen h-screen flex flex-col justify-center items-center gap-4">
        <h1 class="text-4xl font-bold font-title">Blog not found</h1>
        <p>Go back to the <a class="text-primary-500 underline underline-offset-2" href="/blog">blog</a> page.</p>
      </div>
    )
  // }

  // return (
  //   <p>Example: {params.name}</p>
  // );
});

export const onStaticGenerate: StaticGenerateHandler = async () => {
  return {
    params: allBlogs.map((blog) => {
      return { name: blog.slug };
    }),
  };
};