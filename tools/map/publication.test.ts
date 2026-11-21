import { expect, test } from "bun:test";
import { z } from "zod";

const publicConfig = z.object({
  wifi: z.object({ ssid: z.literal(""), password: z.literal("") }),
});

test.each(["alerts.example.json", "demo.json"])(
  "published %s contains no Wi-Fi credentials",
  async (name) => {
    const value: unknown = await Bun.file(
      new URL(`../../config/${name}`, import.meta.url),
    ).json();
    expect(publicConfig.safeParse(value).success).toBe(true);
  },
);
