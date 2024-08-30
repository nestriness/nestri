import { kvResponseCache } from "./caches";

import type { Filter } from "./types";
import type { Context, Env, MiddlewareHandler } from "hono";

type Namespace<E extends Env> = string | ((c: Context<E>) => string);
interface GetCacheKey<E extends Env> {
  (c: Context<E>): string;
}

type KVCacheOption<E extends Env & { Bindings: Record<string, unknown> }> = {
  key: keyof E["Bindings"];
  namespace: Namespace<E>;
  getCacheKey?: GetCacheKey<E>;
  options?: KVNamespacePutOptions;
};

export const defaultGetCacheKey = <E extends Env>(c: Context<E>) => c.req.url;

export const kvCaches =
<E extends Env & { Bindings: Record<string, unknown> }>({
    key: bindingKey,
    namespace,
    options,
    getCacheKey = defaultGetCacheKey,
  }: KVCacheOption<E>): MiddlewareHandler<E> =>
  async (c, next) => {
    const kv: KVNamespace = c.env?.[bindingKey] as KVNamespace;
    const kvNamespace = typeof namespace === "function" ? namespace(c) : namespace;

    const kvCaches = kvResponseCache(kv);
    const cache = kvCaches(kvNamespace);

    const key = getCacheKey(c);
    const response = await cache.match(key);
    if (response) {
      response.headers.set("X-KV-CACHE", "hit");

      return response;
    }

    await next();

    if (c.res.status >= 200 && c.res.status < 300) {
      c.executionCtx.waitUntil(cache.put(key, c.res.clone(), options));
    }
  };