import { z } from "zod";
import type { Region } from "./geometry";

type Node = {
  readonly regionId: string;
  readonly regionName: string;
  readonly regionType: string;
  readonly regionChildIds?: readonly Node[] | undefined;
};
const node: z.ZodType<Node> = z.lazy(() => z.object({
  regionId: z.string(),
  regionName: z.string(),
  regionType: z.string(),
  regionChildIds: z.array(node).optional(),
}));
type DirectoryRegion = Region & { readonly id?: number | undefined };
export function enrichRegions(regions: readonly DirectoryRegion[], input: unknown) {
  const tree = z.object({ states: z.array(node) }).parse(input);
  const hierarchy = new Map<string, { readonly node: Node; readonly parent: string }>();
  const visit = (items: readonly Node[], parent: string): void => {
    for (const item of items) {
      hierarchy.set(item.regionId, { node: item, parent });
      visit(item.regionChildIds ?? [], item.regionId);
    }
  };
  visit(tree.states, "");
  const normalize = (value: string) => value.replace(/^м\.\s*/, "").trim();
  const matches = new Map<string, DirectoryRegion>();
  for (const region of regions) {
    const candidate = hierarchy.get(String(region.id));
    if (candidate && normalize(candidate.node.regionName) === normalize(region.name_uk))
      matches.set(candidate.node.regionId, region);
  }
  return regions.map((region) => {
    const verified = matches.has(String(region.id));
    const parent = hierarchy.get(String(region.id))?.parent ?? "";
    return {
      ...region,
      parent_slug: verified ? matches.get(parent)?.slug ?? "" : region.oblast_slug ?? "",
      reserve_id: verified ? String(region.id) : "",
    };
  });
}
