// content-collections.ts
import { defineCollection, defineConfig } from "@content-collections/core";
import { compileMarkdown } from "@content-collections/markdown";
import path from "node:path";
import fs from "node:fs";
import { spawn } from "cross-spawn";
var cache = /* @__PURE__ */ new Map();
function getGitTimestamp(file) {
  const cachedTimestamp = cache.get(file);
  if (cachedTimestamp) return Promise.resolve(cachedTimestamp);
  return new Promise((resolve, reject) => {
    const cwd = path.dirname(file);
    if (!fs.existsSync(cwd)) {
      resolve(void 0);
      return;
    }
    const fileName = path.basename(file);
    const child = spawn("git", ["log", "-1", '--pretty="%ai"', fileName], {
      cwd
    });
    let output;
    child.stdout.on("data", (d) => output = new Date(String(d)));
    child.on("close", () => {
      if (output) cache.set(file, output);
      resolve(output);
    });
    child.on("error", reject);
  });
}
var blogs = defineCollection({
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
      return date ? new Date(date).toISOString() : (/* @__PURE__ */ new Date()).toISOString();
    });
    return {
      ...document,
      lastModified,
      html
    };
  }
});
var content_collections_default = defineConfig({
  collections: [blogs]
});
export {
  content_collections_default as default
};
