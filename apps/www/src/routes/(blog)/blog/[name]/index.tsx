/* eslint-disable qwik/jsx-img */
import { component$ } from "@builder.io/qwik";
import { allBlogs } from "content-collections";
import { Link, useLocation, type StaticGenerateHandler } from '@builder.io/qwik-city';

function getDaysAgo(date: Date): string {
  const now = new Date();
  const diffTime = Math.abs(now.getTime() - date.getTime());
  const diffDays = Math.ceil(diffTime / (1000 * 60 * 60 * 24));
  return diffDays === 1 ? '(1 day ago)' : `(${diffDays} days ago)`;
}

export default component$(() => {
  const { params } = useLocation();
  const blog = allBlogs.find((p) => p.slug === params.name);

  if (!blog) {
    return (
      <div class="w-screen h-screen flex flex-col justify-center items-center gap-4">
        <div class="flex flex-col justify-center items-center text-center gap-4 w-full max-w-xl">
          <img src="/logo.webp" alt="Nestri Logo" class="w-16 sm:w-20 aspect-[30/23]" height={80} width={80} />
          <div class="font-title">
            <h1 class="text-3xl sm:text-5xl font-bold font-title">Whoops!</h1>
            <h2 class="text-3xl sm:text-5xl font-bold font-title">This post doesn't exist.</h2>
          </div>
          <p>Go back to the <Link class="text-primary-500 underline underline-offset-2" href="/blog">blog</Link> page.</p>
        </div>
      </div>
    )
  }

  return (
    <div class="w-screen h-screen flex flex-col justify-start items-center gap-4 py-20 sm:px-0 px-4">
      <div class="flex flex-col gap-4 w-full max-w-xl">
        <h2 class="text-4xl text-start tracking-tight font-bold font-title">
          {blog.title}
        </h2>
        <div class="list-disc flex flex-row items-center justify-start mt-2 text-sm">
          <span class="text-base dark:text-neutral-400 text-neutral-600 text-nowrap">
            {new Date(blog.lastModified).toLocaleDateString('en-US', { year: 'numeric', month: 'short', day: 'numeric' })}
            &nbsp;
            {getDaysAgo(new Date(blog.lastModified))}
          </span>
        </div>
        <img src={blog.thumbnail} alt={blog.title} width={680} height={400} class="w-full aspect-video ring-2 ring-neutral-300 dark:ring-neutral-700 rounded-lg object-cover mt-4" />
        <div class="mt-8">
          {/* <MDXContent code={post.mdx} /> */}
          {/* {blog.html.replace(/<h1/g, '<h2')} */}
          {/**
           * Make all h tags use font-title and text white
           */
           }
          <div class="space-y-4 prose dark:text-neutral-400 text-neutral-600 pb-10 [&_h1]:text-white [&_h2]:text-white [&_h3]:text-white [&_h1]:text-3xl [&_h1]:font-title [&_h2]:text-2xl [&_h2]:font-title [&_h3]:text-xl [&_h3]:font-title [&_h4]:text-lg [&_h4]:font-title [&_h5]:text-base [&_h5]:font-title [&_h6]:text-sm [&_h6]:font-title [&_p]:text-base [&_a]:text-primary-500 [&_ul]:list-disc [&_ul]:pl-4 [&_ol]:list-decimal [&_ol]:pl-4 [&_li]:text-base [&_blockquote]:bg-neutral-100 [&_blockquote]:dark:bg-neutral-900 [&_blockquote]:p-4 [&_blockquote]:rounded-lg [&_strong]:text-white [&_em]:italic [&_code]:bg-neutral-100 [&_code]:dark:bg-neutral-900 [&_code]:p-1 [&_code]:rounded-md [&_code]:text-sm [&_code]:font-mono [&_code]:leading-relaxed [&_pre_code]:text-sm [&_pre_code]:font-mono [&_pre_code]:leading-relaxed [&_pre_code]:p-0 [&_pre_code]:bg-transparent [&_pre_code]:text-inherit [&_pre_code]:font-inherit [&_pre_code]:whitespace-pre [&_pre_code]:overflow-x-auto" dangerouslySetInnerHTML={blog.html} />
        </div>
      </div>
    </div>
  );
});

export const onStaticGenerate: StaticGenerateHandler = async () => {
  return {
    params: allBlogs.map((blog) => {
      return { name: blog.slug };
    }),
  };
};