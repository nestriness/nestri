// Gotten from https://github.com/sdorra/content-collections/blob/main/packages/vite/src/index.ts
import { type Plugin } from 'vite';
import * as path from "node:path";
import * as fs from "node:fs/promises";
// import type * as v from "valibot";

type Collection = {
    name: string;
    // schema: (valibot: typeof v) => { [key: string]: v.AnySchema };
    // schema: v.ObjectSchema<any, undefined>;
}

export default function nestriMdx({ collections }: { collections: Collection[] }): Plugin {

    return {
        name: 'vite-plugin-nestri-mdx',

        // async configResolved() {
        //     // Generate nestri-env.d.ts file
        //     const declarations = await generateDeclarations(collections);
        //     await fs.writeFile(path.resolve(process.cwd(), 'nestri-env.d.ts'), declarations);
        // },

        async resolveId(id) {
            if (id.endsWith('?blog')) {
                return id;
            }
        },

        async load(id) {
            if (id.endsWith('?blog')) {
                const code = await fs.readFile(id, 'utf-8');
                return {
                    code,
                    meta: {
                        // astro: createDefaultAstroMetadata(),
                        vite: {
                            lang: 'ts',
                        },
                    },
                };
            }
        },

        // transform(code, id) {
        //     return {
        //         code: code,
        //         map: null,
        //     };
        // },
    };
}


// async function generateDeclarations(collections: Collection[]): Promise<string> {
//     const declarations = await Promise.all(collections.map(async (collection) => {
//         const v = await import('valibot');
//         // const values = Object.entries(collection.schema).map(([key, value]) => `${key}: ${v.InferInput<typeof value>}`);
//         // @ts-ignore
//         const type = v.InferInput<typeof collection.schema>;

//         return `
//             declare module '*?${collection.name}' {
//                 type ${capitalize(collection.name)} = {
//                     ${type}
//                     content: string;
//                 }

//                 const all: ${capitalize(collection.name)}[];
//                 export default { all };
//             }
//             `;
//     }));

//     return declarations.join('\n');
// }

// function capitalize(s: string): string {
//     return s.charAt(0).toUpperCase() + s.slice(1);
// }