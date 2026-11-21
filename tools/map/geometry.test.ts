import { expect, test } from "bun:test";
import { project, triangulate, matchRegion } from "./geometry";

test("Kyiv lies east of Bucha and south of Vyshhorod", () => {
  const city = project([30.52, 50.45]);
  expect(city.x).toBeGreaterThan(project([30.22, 50.54]).x);
  expect(city.y).toBeGreaterThan(project([30.49, 50.58]).y);
});

test("triangulation preserves a hole instead of filling it", () => {
  const result = triangulate([
    [[0, 0], [10, 0], [10, 10], [0, 10], [0, 0]],
    [[3, 3], [3, 7], [7, 7], [7, 3], [3, 3]],
  ]);
  let area = 0;
  for (let i = 0; i < result.length; i += 3) {
    const a = result[i], b = result[i + 1], c = result[i + 2];
    if (!a || !b || !c) throw new RangeError("incomplete triangle");
    area += Math.abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) / 2;
  }
  expect(area).toBeCloseTo(84);
});

test("city and renamed districts map explicitly", () => {
  const directory = [
    {slug: "kyiv", name_uk: "м. Київ", name_en: "Kyiv", type: "state"},
    {slug: "samarskii-raion", name_uk: "Самарівський район", name_en: "Samarskyi district", type: "district"},
  ];
  expect(matchRegion("Kyivska", directory)?.slug).toBe("kyiv");
  expect(matchRegion("Novomoskovskyi", directory)?.slug).toBe("samarskii-raion");
  expect(matchRegion("Sevastopilska", directory)).toBeUndefined();
});
