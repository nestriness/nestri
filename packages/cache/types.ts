export type Filter<T extends Record<string, unknown>, V> = {
    [K in keyof T as T[K] extends V ? K : never]: T[K];
};