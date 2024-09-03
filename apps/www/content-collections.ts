import { defineCollection, defineConfig } from "@content-collections/core";
import { compileMarkdown } from "@content-collections/markdown";
import path from 'node:path';
import fs from 'node:fs';
import { spawn } from 'cross-spawn';

const cache = new Map<string, Date>();

function getGitTimestamp(file: string): Promise<Date | undefined> {
    const cachedTimestamp = cache.get(file);
    if (cachedTimestamp) return Promise.resolve(cachedTimestamp);

    return new Promise((resolve, reject) => {
        const cwd = path.dirname(file);
        if (!fs.existsSync(cwd)) {
            resolve(undefined);
            return;
        }
        const fileName = path.basename(file);
        const child = spawn('git', ['log', '-1', '--pretty="%ai"', fileName], {
            cwd,
        });

        let output: Date | undefined;
        child.stdout.on('data', (d) => (output = new Date(String(d))));
        child.on('close', () => {
            if (output) cache.set(file, output);
            resolve(output);
        });
        child.on('error', reject);
    });
}

const blogs = defineCollection({
    name: "blogs",
    directory: "src/blogs",
    include: "**/*.md",
    schema: (z) => ({
        title: z.string(),
        summary: z.string(),
        slug: z.string(),
        createdAt: z.string(),
        thumbnail: z.string()
    }),
    transform: async (document, context) => {
        const html = await context.cache(context, async (ctx) => {
            return await compileMarkdown(ctx, document);
        });

        const lastModified = await context.cache(document._meta.filePath, async (filePath) => {
            const date = await getGitTimestamp(filePath);
            return date ? new Date(date).toISOString() : new Date().toISOString();
        });

        return {
            ...document,
            lastModified,
            html,
        };
    },

});

export default defineConfig({
    collections: [blogs],
});