/* eslint-disable qwik/no-use-visible-task */
import { cn } from '@nestri/ui/design';
import { component$, useSignal, useTask$, useStyles$, useVisibleTask$, $ } from '@builder.io/qwik';

interface ImageLoaderProps {
  src: string;
  alt: string;
  width?: number;
  height?: number;
  class?: string;
}

interface Color {
  r: number;
  g: number;
  b: number;
}

export const ImageLoader = component$((props: ImageLoaderProps) => {
  const imageLoaded = useSignal(false);
  const hasError = useSignal(false);
  const imgRef = useSignal<HTMLImageElement>();
  const shadowColor = useSignal<string>('');
  const imageUrl = `http://localhost:8787/image/cover/${props.src}.avif?width=${props.width}&height=${props.height}&quality=100`;

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
    shadowColor.value = '';
  });

  const analyzeImage = $((img: HTMLImageElement) => {
    const canvas = document.createElement('canvas');
    const ctx = canvas.getContext('2d', { willReadFrequently: true });
    if (!ctx) return;

    // img.crossOrigin = "anonymous"
    canvas.width = img.width;
    canvas.height = img.height;
    ctx.drawImage(img, 0, 0, canvas.width, canvas.height);

    const sampleSize = 20;
    const colors: Color[] = [];

    for (let x = 0; x < sampleSize; x++) {
      for (let y = 0; y < sampleSize; y++) {
        const px = Math.floor((x / sampleSize) * canvas.width);
        const py = Math.floor((y / sampleSize) * canvas.height);
        const pixelData = ctx.getImageData(px, py, 1, 1).data;
        colors.push({ r: pixelData[0], g: pixelData[1], b: pixelData[2] });
      }
    }

    // Function to calculate color saturation
    const calculateSaturation = (color: Color) => {
      const max = Math.max(color.r, color.g, color.b);
      const min = Math.min(color.r, color.g, color.b);
      return max === 0 ? 0 : (max - min) / max;
    };

    // Function to calculate color brightness
    const calculateBrightness = (color: Color) => {
      return (color.r * 299 + color.g * 587 + color.b * 114) / 1000;
    };

    // Find the color with high saturation and brightness
    const vibrantColor = colors.reduce((mostVibrant, color) => {
      const saturation = calculateSaturation(color);
      const brightness = calculateBrightness(color);
      const currentSaturation = calculateSaturation(mostVibrant);
      const currentBrightness = calculateBrightness(mostVibrant);

      // Prefer colors with high saturation and high brightness
      if (saturation > 0.5 && brightness > 100 && (saturation + brightness * 0.01) > (currentSaturation + currentBrightness * 0.01)) {
        return color;
      }
      return mostVibrant;
    }, colors[0]);

    // Increase the brightness of the selected color
    const enhancedColor = {
      r: Math.min(255, vibrantColor.r * 1.2),
      g: Math.min(255, vibrantColor.g * 1.2),
      b: Math.min(255, vibrantColor.b * 1.2)
    };

    shadowColor.value = `rgb(${Math.round(enhancedColor.r)},${Math.round(enhancedColor.g)},${Math.round(enhancedColor.b)})`;
  });

  useVisibleTask$(async ({ cleanup }) => {
    const img = imgRef.value;
    if (!img) return;
    // const imageData = await imageGetter();

    const checkImageLoaded = async () => {
      if (img.complete && img.naturalHeight !== 0) {
        imageLoaded.value = true;
        await analyzeImage(img);
      }
    };

    // Check immediately in case the image is already loaded
    await checkImageLoaded();

    // Set up event listeners
    const loadHandler = async () => {
      imageLoaded.value = true;
      await analyzeImage(img);
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
    <div
      style={{
        width: props.width ? `${props.width}px` : '100%',
        height: props.height ? `${props.height}px` : 'auto',
        "--shadow-color": shadowColor.value ? shadowColor.value : 'none',
        transition: 'box-shadow 0.3s ease-in-out',
        aspectRatio: props.width && props.height ? `${props.width} / ${props.height}` : 'auto'
      }}
      class={cn("relative overflow-hidden", props.class, "dark:shadow-[var(--shadow-color)]")}>
      {!imageLoaded.value && !hasError.value && (
        <div
          class={cn("relative x-[20] inset-0 h-full loading-animation bg-gradient-to-r from-gray-200 via-gray-300 to-gray-200 dark:from-gray-800 dark:via-gray-900 dark:to-gray-800", props.class)}
          style={{
            height: props.height,
            aspectRatio: props.width && props.height ? `${props.width} / ${props.height}` : 'auto'
          }}
        />
      )}
      {/* {imageLoaded.value && (
        <div
          class="dark:block hidden k w-full h-full absolute z-0 inset-0 blur-lg left-0 right-0 bottom-0 top-0 scale-105 opacity-50"
          style={{
            backgroundImage: `url(${imageUrl})`,
            backgroundSize: 'cover',
            backgroundPosition: 'center',
          }}
        />
      )} */}
      <img
        src={imageUrl}
        draggable={false}
        alt={props.alt}
        width={props.width}
        height={props.height}
        ref={imgRef}
        style={{
          transition: 'box-shadow 0.3s ease-in-out'
        }}
        class={{
          'z-[5] relative': imageLoaded.value,
          'hidden': !imageLoaded.value && !hasError.value,
          'w-full h-full': imageLoaded.value,
          'w-16 h-16 text-red-500': hasError.value,
          [props.class || '']: !!props.class,
          'dark:shadow-[var(--shadow-color)]': shadowColor.value
        }}
      />
      {hasError.value && (
        <p class="text-red-500 text-sm" >
          Error loading image
        </p>
      )}
    </div>
  );
});