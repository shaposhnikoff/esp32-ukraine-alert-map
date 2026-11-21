import { expect, test } from "bun:test";
import { rasterize, encodeRuns } from "./raster";

test("rasterization leaves a polygon hole empty", () => {
  const pixels = rasterize({
    width: 10, height: 10,
    areas: [{id: 1, polygons: [[
      [[0, 0], [10, 0], [10, 10], [0, 10], [0, 0]],
      [[3, 3], [3, 7], [7, 7], [7, 3], [3, 3]],
    ]]}],
  });
  expect((pixels[55] ?? 0) & 255).toBe(0);
  expect((pixels[22] ?? 0) & 255).toBe(1);
});

test("run length encoding preserves all pixels and splits oversized runs", () => {
  const pixels = new Uint16Array(70000).fill(258);
  const encoded = encodeRuns(pixels);
  expect(encoded).toEqual([65535, 258, 4465, 258]);
});
