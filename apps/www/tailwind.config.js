// import colors from "tailwindcss/colors";
import baseConfig from "@nestri/ui/tailwind.config";
/** @type {import('tailwindcss').Config} */

module.exports = {
  content: [
    "./{src,components,app}/**/*.{ts,tsx,html}",
    "../../packages/ui/src/**/*.{ts,tsx}",
  ],
  presets: [baseConfig],
  plugins: [],
};