import { $, useVisibleTask$ } from '@builder.io/qwik';

export const setupImageLoader = $(() => {
    const imageCache = new Map();

    const loadImage = async (img: HTMLImageElement) => {
        const src = img.getAttribute('data-src');
        console.log('src', src);
        if (!src) return;

        // Check if the image is already in the cache
        if (imageCache.has(src)) {
            img.src = imageCache.get(src);
            img.classList.add('loaded');
            return;
        }

        // Check if the image is in the browser's cache
        const cache = await caches.open('image-cache');
        console.log('cache', cache);
        const cachedResponse = await cache.match(src);

        if (cachedResponse) {
            const blob = await cachedResponse.blob();
            const objectURL = URL.createObjectURL(blob);
            img.src = objectURL;
            imageCache.set(src, objectURL);
            img.classList.add('loaded');
        } else {
            // If not in cache, load the image
            try {
                const response = await fetch(src);
                const blob = await response.blob();
                const objectURL = URL.createObjectURL(blob);

                img.src = objectURL;
                imageCache.set(src, objectURL);
                img.classList.add('loaded');

                // Cache the image for future use
                cache.put(src, new Response(blob));
            } catch (error) {
                console.error('Error loading image:', error);
                img.classList.add('error');
            }
        }
    };

    const observer = new IntersectionObserver(
        (entries) => {
            entries.forEach((entry) => {
                if (entry.isIntersecting) {
                    loadImage(entry.target as HTMLImageElement);
                    observer.unobserve(entry.target);
                }
            });
        },
        { rootMargin: '50px' }
    );

    const setupImages = () => {
        const images = document.querySelectorAll('img[data-src]');
        images.forEach((img) => {
            observer.observe(img);
        });
    };

    return setupImages;
});

export const useImageLoader = () => {
    // eslint-disable-next-line qwik/no-use-visible-task
    useVisibleTask$(async () => {
        const setup = await setupImageLoader();
        setup();
    });
};