import earcut from "earcut";

export type Coordinate = readonly [number, number];
export type Point = { readonly x: number; readonly y: number };
export type Region = {
  readonly slug: string;
  readonly name_uk: string;
  readonly name_en?: string | undefined;
  readonly type: string;
  readonly oblast_slug?: string | undefined;
};

export function project(coordinate: Coordinate): Point {
  return { x: (coordinate[0] - 22) * 656, y: (53 - coordinate[1]) * 1000 };
}

export function triangulate(rings: readonly (readonly Coordinate[])[]): readonly Point[] {
  const points: Point[] = [];
  const holes: number[] = [];
  for (const [index, ring] of rings.entries()) {
    if (index > 0) holes.push(points.length);
    for (const coordinate of ring) points.push({ x: coordinate[0], y: coordinate[1] });
  }
  const indices = earcut(points.flatMap((p) => [p.x, p.y]), holes, 2);
  return indices.map((index) => {
    const point = points[index];
    if (!point) throw new RangeError("Triangulation returned an invalid vertex");
    return point;
  });
}

const renamed = {
  Novomoskovskyi: "Samarskyi",
  "Novohrad-Volynskyi": "Zviahelskyi",
  Chervonohradskyi: "Sheptytskyi",
  Cnernivetskyi: "Chernivetskyi",
  Yavorivskyi: "Iavorivskyi",
} as const;

export function matchRegion(name: string, directory: readonly Region[]): Region | undefined {
  if (name === "Kyivska") return directory.find((region) => region.slug === "kyiv");
  const alias = Object.entries(renamed).find(([oldName]) => oldName === name)?.[1] ?? name;
  const normalize = (value: string) => value.toLowerCase().replace(/district|raion|rayon|[^a-z]/g, "");
  return directory.find((region) =>
    region.type === "district" && normalize(region.name_en ?? "") === normalize(alias));
}
