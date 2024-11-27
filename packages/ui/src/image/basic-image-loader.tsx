/* eslint-disable qwik/no-use-visible-task */
import { cn } from '@nestri/ui/design';
import { component$, useSignal, useTask$, useStyles$, useVisibleTask$ } from '@builder.io/qwik';

interface ImageLoaderProps {
  src: string;
  alt: string;
  width?: number;
  height?: number;
  class?: string;
}

export const BasicImageLoader = component$((props: ImageLoaderProps) => {
  const imageLoaded = useSignal(false);
  const hasError = useSignal(false);
  const imgRef = useSignal<HTMLImageElement>();

  useStyles$(`
    @keyframes gradientShift {
      0% { background-position: 200% 0; }
      100% { background-position: -200% 0; }
    }
    .loading-animation {
      animation: gradientShift 1.5s infinite linear;
      background-size: 200% 100%;
    }
  `);

  useTask$(({ track }) => {
    track(() => props.src);
    imageLoaded.value = false;
    hasError.value = false;
  });


  useVisibleTask$(async ({ cleanup }) => {
    const img = imgRef.value;
    if (!img) return;
    // const imageData = await imageGetter();

    const checkImageLoaded = async () => {
      if (img.complete && img.naturalHeight !== 0) {
        imageLoaded.value = true;
      }
    };

    // Check immediately in case the image is already loaded
    await checkImageLoaded();

    // Set up event listeners
    const loadHandler = async () => {
      imageLoaded.value = true;
    };
    const errorHandler = () => {
      hasError.value = true;
    };

    img.addEventListener('load', loadHandler);
    img.addEventListener('error', errorHandler);

    // Use MutationObserver to detect src changes
    const observer = new MutationObserver(checkImageLoaded);
    observer.observe(img, { attributes: true, attributeFilter: ['src'] });

    cleanup(() => {
      img.removeEventListener('load', loadHandler);
      img.removeEventListener('error', errorHandler);
      observer.disconnect();
    });
  });

  return (
    <>
      {!imageLoaded.value && !hasError.value && (
        <div
          class={
            cn(
              " w-full h-full loading-animation bg-gradient-to-r from-gray-200 via-gray-300 to-gray-200 dark:from-gray-800 dark:via-gray-900 dark:to-gray-800",
              props.class
            )
          }
        />
      )}
      <img
        src={props.src}
        draggable={false}
        alt={props.alt}
        width={props.width}
        // crossOrigin='anonymous'
        height={props.height}
        ref={imgRef}
        class={{
          'z-[5] relative': imageLoaded.value,
          'hidden': !imageLoaded.value && !hasError.value,
          'w-full h-full': imageLoaded.value,
          'w-16 h-16 text-red-500': hasError.value,
          [props.class || '']: !!props.class,
        }}
      />
      {hasError.value && (
        <p class="text-red-500 text-sm" >
          Error loading image
        </p>
      )}
    </>
  );
});