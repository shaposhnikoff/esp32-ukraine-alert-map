import { triangulate } from "./geometry";
import type { Coordinate } from "./geometry";

export type RasterArea = {
  readonly id: number;
  readonly polygons: readonly (readonly (readonly Coordinate[])[])[];
};
type RasterInput = {
  readonly width: number;
  readonly height: number;
  readonly areas: readonly RasterArea[];
};

export function rasterize(input: RasterInput): Uint16Array {
  const { width, height } = input;
  const pixels = new Uint16Array(width * height);
  for (const area of input.areas) {
    for (const polygon of area.polygons) {
      const vertices = triangulate(polygon);
      for (let i = 0; i < vertices.length; i += 3) {
        const a = vertices[i], b = vertices[i + 1], c = vertices[i + 2];
        if (!a || !b || !c) throw new RangeError("Incomplete triangle");
        const left = Math.max(0, Math.floor(Math.min(a.x, b.x, c.x)));
        const right = Math.min(width - 1, Math.ceil(Math.max(a.x, b.x, c.x)));
        const top = Math.max(0, Math.floor(Math.min(a.y, b.y, c.y)));
        const bottom = Math.min(height - 1, Math.ceil(Math.max(a.y, b.y, c.y)));
        for (let y = top; y <= bottom; y++) {
          for (let x = left; x <= right; x++) {
            const px = x + 0.5, py = y + 0.5;
            const p = (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
            const q = (c.x - b.x) * (py - b.y) - (c.y - b.y) * (px - b.x);
            const r = (a.x - c.x) * (py - c.y) - (a.y - c.y) * (px - c.x);
            if ((p >= 0 && q >= 0 && r >= 0) || (p <= 0 && q <= 0 && r <= 0))
              pixels[y * width + x] = area.id;
          }
        }
      }
    }
  }
  const outlined = pixels.slice();
  for (let y = 1; y < height - 1; y++) {
    for (let x = 1; x < width - 1; x++) {
      const index = y * width + x;
      const id = pixels[index] ?? 0;
      if (id && [index - 1, index + 1, index - width, index + width]
        .some((neighbor) => pixels[neighbor] !== id)) outlined[index] = id | 256;
    }
  }
  return outlined;
}

export function encodeRuns(pixels: Uint16Array): number[] {
  const runs: number[] = [];
  let previous = pixels[0], count = 0;
  for (const pixel of pixels) {
    if (pixel === previous && count < 65535) count++;
    else {
      if (count) runs.push(count, previous ?? 0);
      previous = pixel;
      count = 1;
    }
  }
  if (count) runs.push(count, previous ?? 0);
  return runs;
}
