import color from "tinycolor2";

function fnv1a(str: string) {
    let hash = 0x811c9dc5;
    for (let i = 0; i < str.length; i++) {
        hash ^= str.charCodeAt(i);
        hash += (hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) + (hash << 24);
    }
    return hash >>> 0;
}

export function generateGradient(username: string) {
    const hash = fnv1a(username);
    const hue1 = hash % 360;
    const hue2 = (hash >> 16) % 360;

    const c1 = color({ h: hue1, s: 0.8, l: 0.6 });
    const c2 = color({ h: hue2, s: 0.8, l: 0.5 });

    return {
        fromColor: c1.toHexString(),
        toColor: c2.toHexString(),
    };
}