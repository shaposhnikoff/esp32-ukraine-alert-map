import { expect, test } from "bun:test";
import { enrichRegions } from "./hierarchy";

test("community inherits its district while Kyiv remains independent", () => {
  const result = enrichRegions([
    {id: 31, slug: "kyiv", name_uk: "м. Київ", name_en: "Kyiv", type: "state"},
    {id: 14, slug: "kyivska", name_uk: "Київська область", name_en: "Kyivska", type: "state"},
    {id: 74, slug: "brovarskii-raion", name_uk: "Броварський район", name_en: "Brovarskyi district", type: "district"},
    {id: 711, slug: "example-community", name_uk: "Громада", name_en: "Community", type: "community"},
  ], {states: [
    {regionId: "31", regionName: "м. Київ", regionType: "State"},
    {regionId: "14", regionName: "Київська область", regionType: "State", regionChildIds: [
      {regionId: "74", regionName: "Броварський район", regionType: "District", regionChildIds: [
        {regionId: "711", regionName: "Громада", regionType: "Community"},
      ]},
    ]},
  ]});
  expect(result.find((r) => r.slug === "kyiv")?.parent_slug).toBe("");
  expect(result.find((r) => r.slug === "example-community")?.parent_slug).toBe("brovarskii-raion");
  expect(result.find((r) => r.slug === "example-community")?.reserve_id).toBe("711");
});
