/** @type {import("eslint").Linter.Config} */
module.exports = {
  root: true,
  extends: ["@nestri/eslint-config/react-internal.js"],
  parser: "@typescript-eslint/parser",
  parserOptions: {
    //Find some way to use the lint tsconfig
    project: "./tsconfig.json",
    tsconfigRootDir: __dirname,
  },
};
