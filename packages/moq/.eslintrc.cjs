/* eslint-env node */
module.exports = {
	extends: [
		"eslint:recommended",
		"plugin:@typescript-eslint/recommended",
		"plugin:@typescript-eslint/recommended-requiring-type-checking",
		"plugin:@typescript-eslint/strict",
		"prettier",
	],
	parser: "@typescript-eslint/parser",
	plugins: ["@typescript-eslint", "prettier", "solid"],
	root: true,
	env: {
		browser: true,
		es2022: true,
		worker: true,
	},
	ignorePatterns: ["dist", "node_modules", ".eslintrc.cjs"],
	rules: {
		// Allow the ! operator because typescript can't always figure out when something is not undefined
		"@typescript-eslint/no-non-null-assertion": "off",

		// Allow `any` because Javascript was not designed to be type safe.
		"@typescript-eslint/no-explicit-any": "off",

		// Requring a comment in empty function is silly
		"@typescript-eslint/no-empty-function": "off",

		// Warn when an unused variable doesn't start with an underscore
		"@typescript-eslint/no-unused-vars": [
			"warn",
			{
				argsIgnorePattern: "^_",
				varsIgnorePattern: "^_",
				caughtErrorsIgnorePattern: "^_",
			},
		],

		// The no-unsafe-* rules are a pain an introduce a lot of false-positives.
		// Typescript will make sure things are properly typed.
		"@typescript-eslint/no-unsafe-call": "off",
		"@typescript-eslint/no-unsafe-argument": "off",
		"@typescript-eslint/no-unsafe-call": "off",
		"@typescript-eslint/no-unsafe-member-access": "off",
		"@typescript-eslint/no-unsafe-assignment": "off",
		"@typescript-eslint/no-unsafe-return": "off",

		// Make formatting errors into warnings
		"prettier/prettier": 1,
	},

	parserOptions: {
		project: true,
		tsconfigRootDir: __dirname,
	},
}
