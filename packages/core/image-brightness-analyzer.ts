// image-brightness-analyzer.js

export class ImageBrightnessAnalyzer {
    canvas: HTMLCanvasElement;
    ctx: CanvasRenderingContext2D ;

    constructor() {
      this.canvas = document.createElement('canvas');
      this.ctx = this.canvas.getContext('2d')!;
    }
  
    analyze(imgElement: HTMLImageElement) {
      if (!(imgElement instanceof HTMLImageElement)) {
        throw new Error('Input must be an HTMLImageElement');
      }
  
      this.canvas.width = imgElement.width;
      this.canvas.height = imgElement.height;
      this.ctx.drawImage(imgElement, 0, 0);
  
      const imageData = this.ctx.getImageData(0, 0, this.canvas.width, this.canvas.height);
      const data = imageData.data;
  
      let brightestPixel = { value: 0, x: 0, y: 0 };
      let dullestPixel = { value: 765, x: 0, y: 0 }; // 765 is the max value (255 * 3)
  
      for (let y = 0; y < this.canvas.height; y++) {
        for (let x = 0; x < this.canvas.width; x++) {
          const index = (y * this.canvas.width + x) * 4;
          const brightness = data[index] + data[index + 1] + data[index + 2];
  
          if (brightness > brightestPixel.value) {
            brightestPixel = { value: brightness, x, y };
          }
          if (brightness < dullestPixel.value) {
            dullestPixel = { value: brightness, x, y };
          }
        }
      }
  
      return {
        brightest: {
          x: brightestPixel.x,
          y: brightestPixel.y,
          color: this.getPixelColor(data, brightestPixel.x, brightestPixel.y)
        },
        dullest: {
          x: dullestPixel.x,
          y: dullestPixel.y,
          color: this.getPixelColor(data, dullestPixel.x, dullestPixel.y)
        }
      };
    }
  
    getPixelColor(data: any[] | Uint8ClampedArray, x: number, y: number) {
      const index = (y * this.canvas.width + x) * 4;
      return {
        r: data[index],
        g: data[index + 1],
        b: data[index + 2]
      };
    }
  }
  
//   // Export the class for use in browser environments
//   if (typeof window !== 'undefined') {
//     window.ImageBrightnessAnalyzer = ImageBrightnessAnalyzer;
//   }
  
//   // Export for module environments (if using a bundler)
//   if (typeof module !== 'undefined' && typeof module.exports !== 'undefined') {
//     module.exports = ImageBrightnessAnalyzer;
//   }