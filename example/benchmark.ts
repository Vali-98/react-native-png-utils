// Generate a real RGB PNG with random pixels, unfiltered rows and stored
// DEFLATE blocks. No image decoder, canvas or extra package is needed.
const alphabet =
  'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
const crcTable = Array.from({ length: 256 }, (_, index) => {
  let crc = index;
  for (let bit = 0; bit < 8; bit++)
    crc = (crc >>> 1) ^ (crc & 1 ? 0xedb88320 : 0);
  return crc >>> 0;
});
export const pause = () =>
  new Promise<void>(resolve => setTimeout(resolve, 16));
export const mib = (bytes: number) => `${(bytes / 1048576).toFixed(2)} MiB`;
export const base64Bytes = (data: string) =>
  (data.length / 4) * 3 -
  (data.endsWith('==') ? 2 : data.endsWith('=') ? 1 : 0);

function be32(out: Uint8Array, offset: number, value: number) {
  out[offset] = value >>> 24;
  out[offset + 1] = value >>> 16;
  out[offset + 2] = value >>> 8;
  out[offset + 3] = value;
}
function crc(out: Uint8Array, start: number, end: number) {
  let value = 0xffffffff;
  for (let i = start; i < end; i++)
    value = (value >>> 8) ^ crcTable[(value ^ out[i]!) & 255]!;
  be32(out, end, (value ^ 0xffffffff) >>> 0);
}
async function encode(bytes: Uint8Array) {
  const parts: string[] = [];
  // Bound temporary arrays and argument counts. Chunk sizes are multiples of 3.
  for (let start = 0; start < bytes.length; start += 3072) {
    const end = Math.min(start + 3072, bytes.length);
    const codes: number[] = [];
    for (let i = start; i < end; i += 3) {
      const value =
        (bytes[i]! << 16) | ((bytes[i + 1] ?? 0) << 8) | (bytes[i + 2] ?? 0);
      codes.push(
        alphabet.charCodeAt(value >>> 18),
        alphabet.charCodeAt((value >>> 12) & 63),
        i + 1 < end ? alphabet.charCodeAt((value >>> 6) & 63) : 61,
        i + 2 < end ? alphabet.charCodeAt(value & 63) : 61,
      );
    }
    parts.push(String.fromCharCode(...codes));
    if ((start / 3072) % 64 === 0) await pause();
  }
  return parts.join('');
}
export async function randomPNG(sizeMiB: number) {
  const width = 512;
  const stride = width * 3 + 1;
  const height = Math.max(1, Math.ceil((sizeMiB * 1048576) / stride));
  const rawSize = stride * height;
  const blocks = Math.ceil(rawSize / 65535);
  const idatSize = 2 + blocks * 5 + rawSize + 4;
  // Allocate only the finished PNG, not another complete RGB/deflate buffer.
  const out = new Uint8Array(57 + idatSize);
  out.set([137, 80, 78, 71, 13, 10, 26, 10]);
  be32(out, 8, 13);
  out.set([73, 72, 68, 82], 12);
  be32(out, 16, width);
  be32(out, 20, height);
  out.set([8, 2, 0, 0, 0], 24);
  crc(out, 12, 29);
  be32(out, 33, idatSize);
  out.set([73, 68, 65, 84, 120, 1], 37);
  let offset = 43;
  let state = 0x12345678; // Repeatable random pixels make runs comparable.
  let a = 1;
  let b = 0;
  for (let start = 0; start < rawSize; start += 65535) {
    const length = Math.min(65535, rawSize - start);
    out[offset++] = start + length === rawSize ? 1 : 0;
    out[offset++] = length & 255;
    out[offset++] = length >>> 8;
    out[offset++] = ~length & 255;
    out[offset++] = (~length >>> 8) & 255;
    for (let i = 0; i < length; i++) {
      state ^= state << 13;
      state ^= state >>> 17;
      state ^= state << 5;
      const value = (start + i) % stride === 0 ? 0 : state & 255;
      out[offset++] = value;
      a += value;
      if (a >= 65521) a -= 65521;
      b += a;
      if (b >= 65521) b -= 65521;
    }
    if ((start / 65535) % 4 === 0) await pause();
  }
  be32(out, offset, ((b << 16) | a) >>> 0);
  offset += 4;
  crc(out, 37, offset);
  offset += 4;
  be32(out, offset, 0);
  out.set([73, 69, 78, 68], offset + 4);
  crc(out, offset + 4, offset + 8);
  return { base64: await encode(out), width, height, bytes: out.length };
}
export function summary(samples: number[]) {
  const sorted = [...samples].sort((a, b) => a - b);
  const mid = Math.floor(sorted.length / 2);
  return {
    median:
      sorted.length % 2 ? sorted[mid]! : (sorted[mid - 1]! + sorted[mid]!) / 2,
    p95: sorted[Math.ceil(sorted.length * 0.95) - 1]!,
    mean: samples.reduce((sum, time) => sum + time, 0) / samples.length,
  };
}
