//copied from https://github.com/napolab/kv-response-cache with some minor changes
import { kvResponseCache } from "./caches";
import { kvCaches, defaultGetCacheKey } from "./middleware";

import type { KVResponseCache } from "./caches";

export type { KVResponseCache };
export { kvResponseCache, kvCaches, defaultGetCacheKey as getCacheKey };