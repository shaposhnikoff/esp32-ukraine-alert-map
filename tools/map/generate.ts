import { createHash } from "node:crypto";
import { mkdir } from "node:fs/promises";
import { resolve } from "node:path";
import ky from "ky";
import { z } from "zod";
import { matchRegion, project } from "./geometry";
import type { Coordinate, Point } from "./geometry";
import { rasterize, encodeRuns } from "./raster";
import type { RasterArea } from "./raster";
import { enrichRegions } from "./hierarchy";

const root = resolve(import.meta.dir, "../..");
const cache = resolve(root, ".cache/map");
await mkdir(cache, { recursive: true });
await mkdir(resolve(root, "assets/map"), { recursive: true });
await mkdir(resolve(root, "generated"), { recursive: true });

const sources = {
  geometry: "https://media.githubusercontent.com/media/wmgeolab/geoBoundaries/9469f09/releaseData/gbHumanitarian/UKR/ADM2/geoBoundaries-UKR-ADM2_simplified.geojson",
  directory: "https://tryvoha.online/api/v1/regions",
  hierarchy: "https://siren.pp.ua/api/v3/regions",
} as const;
async function source(name: keyof typeof sources): Promise<string> {
  const file = Bun.file(resolve(cache, `${name}.json`));
  if (await file.exists()) return file.text();
  const text = await ky.get(sources[name], { timeout: 30000, retry: 0 }).text();
  await Bun.write(file, text);
  return text;
}
const coordinate = z.tuple([z.number(), z.number()]);
const polygon = z.array(z.array(coordinate).min(4)).min(1);
const geometrySchema = z.discriminatedUnion("type", [
  z.object({ type: z.literal("Polygon"), coordinates: polygon }),
  z.object({ type: z.literal("MultiPolygon"), coordinates: z.array(polygon).min(1) }),
]);
const featureCollection = z.object({
  features: z.array(z.object({
    properties: z.object({ shapeName: z.string(), shapeID: z.string() }),
    geometry: geometrySchema,
  })),
});
const directorySchema = z.object({ regions: z.array(z.object({
  id: z.number(),
  slug: z.string(), name_uk: z.string(), name_en: z.string(),
  type: z.string(), oblast_slug: z.string().nullish().transform((value) => value ?? ""),
})) });
const geometryText = await source("geometry");
const directoryText = await source("directory");
const hierarchyText = await source("hierarchy");
const features = featureCollection.parse(JSON.parse(geometryText)).features;
const parsedDirectory = directorySchema.parse(JSON.parse(directoryText));
const directory = { regions: enrichRegions(parsedDirectory.regions, JSON.parse(hierarchyText)) };
type Shape = {
  readonly slug: string;
  readonly name: string;
  readonly parent: string;
  readonly polygons: readonly (readonly (readonly Coordinate[])[])[];
  readonly home: boolean;
};
const shapes: Shape[] = [];
let exclusionZone: Shape["polygons"] = [];
for (const feature of features) {
  const polygons = feature.geometry.type === "Polygon"
    ? [feature.geometry.coordinates] : feature.geometry.coordinates;
  if (feature.properties.shapeName === "Chornobylska zona vidchuzhennia") {
    exclusionZone = polygons;
    continue;
  }
  const match = matchRegion(feature.properties.shapeName, directory.regions);
  shapes.push({
    slug: match?.slug ?? `unsupported-${feature.properties.shapeID}`,
    name: match?.name_uk ?? feature.properties.shapeName,
    parent: match?.oblast_slug ?? "",
    polygons,
    home: match?.slug === "kyiv" || match?.oblast_slug === "kyivska",
  });
}
const vyshhorod = shapes.findIndex((shape) => shape.slug === "visgorodskii-raion");
const northernDistrict = shapes[vyshhorod];
if (northernDistrict && exclusionZone.length)
  shapes[vyshhorod] = { ...northernDistrict, polygons: [...northernDistrict.polygons, ...exclusionZone] };
if (shapes.filter((shape) => shape.home).length !== 8)
  throw new RangeError("Expected Kyiv city and exactly seven Kyiv oblast districts");

function fitted(visible: readonly Shape[]): {
  readonly areas: readonly RasterArea[];
  readonly centers: ReadonlyMap<string, Point>;
} {
  const points = visible.flatMap((s) => s.polygons.flatMap((p) => p.flatMap((r) => r.map(project))));
  const minX = Math.min(...points.map((p) => p.x)), maxX = Math.max(...points.map((p) => p.x));
  const minY = Math.min(...points.map((p) => p.y)), maxY = Math.max(...points.map((p) => p.y));
  const scale = Math.min(672 / (maxX - minX), 392 / (maxY - minY));
  const transform = (point: Coordinate): Coordinate => {
    const p = project(point);
    return [(p.x - (minX + maxX) / 2) * scale + 350,
      (p.y - (minY + maxY) / 2) * scale + 210];
  };
  const centers = new Map<string, Point>();
  const areas = visible.map((shape): RasterArea => {
    const polygons = shape.polygons.map((p) => p.map((r) => r.map(transform)));
    const outer = polygons.flatMap((p) => p[0] ?? []);
    const xs = outer.map((p) => p[0]), ys = outer.map((p) => p[1]);
    centers.set(shape.slug, { x: (Math.min(...xs) + Math.max(...xs)) / 2,
      y: (Math.min(...ys) + Math.max(...ys)) / 2 });
    return { id: shapes.indexOf(shape) + 1, polygons };
  });
  return { areas, centers };
}
const home = fitted(shapes.filter((s) => s.home));
const national = fitted(shapes);
const homeRuns = encodeRuns(rasterize({ width: 700, height: 420, areas: home.areas }));
const nationalRuns = encodeRuns(rasterize({ width: 700, height: 420, areas: national.areas }));
const array = (name: string, values: readonly number[]) =>
  `static const uint16_t ${name}[] = {\n${values.reduce<string[]>((lines, n, i) => {
    if (i % 32 === 0) lines.push("");
    const last = lines.length - 1;
    lines[last] = (lines[last] ?? "") + `${n},`;
    return lines;
  }, []).join("\n")}\n};\n`;
const rows = shapes.map((shape) => {
  const center = (shape.home ? home : national).centers.get(shape.slug);
  if (!center) throw new RangeError(`Missing center for ${shape.slug}`);
  return `{${JSON.stringify(shape.slug)},${JSON.stringify(shape.name)},${JSON.stringify(shape.parent)},` +
    `{${center.x.toFixed(2)}f,${center.y.toFixed(2)}f},${shape.home}},`;
});
const generated = `// Generated map masks. Source and CC BY 3.0 IGO attribution: assets/map/source-manifest.json.\n` +
  `#include "alerts/geometry.hpp"\nnamespace alerts::geo {\n` +
  array("home_runs", homeRuns) + array("national_runs", nationalRuns) +
  `const Area areas[] = {\n${rows.join("\n")}\n};\n` +
  `const size_t area_count = sizeof(areas)/sizeof(areas[0]);\n` +
  `const MapMask home_mask{home_runs,sizeof(home_runs)/sizeof(home_runs[0]),700,420};\n` +
  `const MapMask national_mask{national_runs,sizeof(national_runs)/sizeof(national_runs[0]),700,420};\n` +
  `const char directory_json[] = ${JSON.stringify(JSON.stringify(directory))};\n}\n`;
await Bun.write(resolve(root, "generated/map_assets.cpp"), generated);
await Bun.write(resolve(root, "assets/map/source-manifest.json"), JSON.stringify({
  source: sources.geometry, directory: sources.directory,
  sha256: createHash("sha256").update(geometryText).digest("hex"),
  directory_sha256: createHash("sha256").update(directoryText).digest("hex"),
  hierarchy: sources.hierarchy,
  hierarchy_sha256: createHash("sha256").update(hierarchyText).digest("hex"),
  license: "CC BY 3.0 IGO",
  attribution: "State Scientific Production Enterprise Kartographia / OCHA HDX; distributed by geoBoundaries",
  source_dataset: "https://data.humdata.org/dataset/cod-ab-ukr",
  boundary_year: 2022, source_features: features.length,
  areas: shapes.map(({ slug, name, parent, home: isHome }) => ({ slug, name, parent, home: isHome })),
  home_areas: 8, width: 700, height: 420,
  mask_bytes: (homeRuns.length + nationalRuns.length) * 2,
}, null, 2) + "\n");
console.log(`Generated ${shapes.length} areas, 8 home areas, ${(homeRuns.length + nationalRuns.length) * 2} mask bytes`);
