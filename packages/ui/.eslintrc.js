/** @type {import("eslint").Linter.Config} */
module.exports = {
  root: true,
  extends: ["@nestri/eslint-config/qwik.js"],
  parser: "@typescript-eslint/parser",
  parserOptions: {
    project: "./tsconfig.lint.json",
    tsconfigRootDir: __dirname,
  },
};
