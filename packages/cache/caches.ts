const buildCacheKey = (namespace: string) => (key: string) => {
    return `${namespace}:${key}`;
  };
  
  export interface KVResponseCache {
    match(key: string): Promise<Response | null>;
    put(key: string, res: Response, options?: KVNamespacePutOptions): Promise<void>;
    delete(key: string): Promise<void>;
  }
  
  export const kvResponseCache =
    (kv: KVNamespace) =>
    (cacheName: string): KVResponseCache => {
      const cacheKey = buildCacheKey(cacheName);
  
      return {
        async match(key: string) {
          const [headers, status, body] = await Promise.all([
            kv.get(cacheKey(`${key}:headers`)),
            kv.get(cacheKey(`${key}:status`)),
            kv.get(cacheKey(`${key}:body`), "stream"),
          ]);
  
          if (headers === null || body === null || status === null) return null;
  
          return new Response(body, { headers: JSON.parse(headers), status: parseInt(status, 10) });
        },
        async put(key: string, res: Response, options?: KVNamespacePutOptions) {
          const headers = Array.from(res.headers.entries()).reduce((acc, [key, value]) => ({ ...acc, [key]: value }), {});
          const body = res.body;
          if (body === null) return;
  
          await Promise.all([
            kv.put(cacheKey(`${key}:headers`), JSON.stringify(headers), options),
            kv.put(cacheKey(`${key}:status`), `${res.status}`, options),
            kv.put(cacheKey(`${key}:body`), body, options),
          ]);
        },
        async delete(key: string) {
          await Promise.all([
            kv.delete(cacheKey(`${key}:headers`)),
            kv.delete(cacheKey(`${key}:status`)),
            kv.delete(cacheKey(`${key}:body`)),
          ]);
        },
      };
    };