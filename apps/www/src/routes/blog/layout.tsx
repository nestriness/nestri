import { $, component$, Slot, useOnDocument, useStyles$ } from "@builder.io/qwik";
import { useContent } from "@builder.io/qwik-city";
import { Footer } from "@nestri/ui";
import styles from "./blog.css?inline"
import { Link, useDocumentHead, useLocation } from "@builder.io/qwik-city";

function getDaysAgo(date: Date): string {
    const now = new Date();
    const diffTime = Math.abs(now.getTime() - date.getTime());
    const diffDays = Math.ceil(diffTime / (1000 * 60 * 60 * 24));
    return diffDays === 1 ? '(1 day ago)' : `(${diffDays} days ago)`;
}

export default component$(() => {
    useStyles$(styles)
    //How do i know if it is /blog/** and not /blog?
    const isBlogPost = useLocation().url.pathname.startsWith("/blog/")
    const { frontmatter } = useDocumentHead()
    const { headings } = useContent();
    // console.log(headings)

    useOnDocument('load', $(() => {
        const sections = document.querySelectorAll('.blog h3');
        const tocLinks = document.querySelectorAll('#toc a');

        const observerOptions = {
            threshold: 0.5
        };

        const observer = new IntersectionObserver((entries) => {
            entries.forEach(entry => {
                if (entry.isIntersecting) {
                    const id = entry.target.getAttribute('id');
                    tocLinks.forEach(link => {
                        link.classList.remove('active');
                        if (link.getAttribute('href') === `#${id}`) {
                            link.classList.add('active');
                        }
                    });
                }
            });
        }, observerOptions);

        sections.forEach(section => observer.observe(section));
    }))

    return (
        <>
            {
                isBlogPost ?
                    (
                        <div class="w-full grid grid-cols-1 relative sm:grid-cols-[1fr_auto_1fr] items-center justify-center px-4 py-20 pb-10">
                            <div class="hidden sm:block h-full">
                                <nav class="sticky top-20 z-10 w-full flex justify-center items-center">
                                    <Link href="/blog" class="flex w-max justify-center gap-1 items-center text-gray-600 dark:text-gray-400 hover:text-gray-900 dark:hover:text-gray-100">
                                        <svg width="18px" height="18px" stroke-width="1.5" viewBox="0 0 24 24" fill="none" color="currentColor"><path d="M10.25 4.75l-3.5 3.5 3.5 3.5" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"></path><path d="M6.75 8.25h6a4 4 0 014 4v7" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"></path></svg>
                                        Blog
                                    </Link>
                                </nav>
                            </div>
                            <div class="max-w-xl mx-auto gap-1 flex flex-col">
                                <h2 class="text-4xl text-start tracking-tight font-bold font-title">
                                    {frontmatter.blogTitle}
                                </h2>
                                <div class="list-disc flex flex-row items-center justify-start mt-2 text-sm">
                                    <span class="text-base dark:text-neutral-400 text-neutral-600 text-nowrap">
                                        {new Date(frontmatter.createdAt).toLocaleDateString('en-US', { year: 'numeric', month: 'short', day: 'numeric' })}
                                        &nbsp;
                                        {getDaysAgo(new Date(frontmatter.createdAt))}
                                    </span>
                                </div>
                                <img src={frontmatter.thumbnail} alt={frontmatter.title} width={680} height={400} class="w-full aspect-video ring-2 ring-neutral-300 dark:ring-neutral-700 rounded-lg object-cover mt-4" />                        <p class="text-sm text-gray-600 dark:text-gray-400">{frontmatter.date}</p>
                                <div class="mt-10 gap-4 prose dark:prose-invert flex flex-col blog">
                                    <Slot />
                                </div>
                            </div>
                            <div class="hidden sm:block h-full">
                                <nav class="sticky top-20 z-10 w-full flex justify-center items-center">
                                    <ul id="toc" class="list-none p-0 m-0 flex flex-col items-start justify-start gap-1">
                                        {headings?.map((heading) => (
                                            <li key={heading.id} class="text-sm text-gray-600 dark:text-gray-400">
                                                <a href={`#${heading.id}`} class="hover:text-gray-900 dark:hover:text-gray-100">
                                                    {heading.text}
                                                </a>
                                            </li>
                                        ))}
                                    </ul>
                                </nav>
                            </div>
                        </div>
                    )
                    : <Slot />
            }
            <Footer />
        </>
    );
});