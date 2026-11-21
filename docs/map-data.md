# Map data and attribution

**Source:** State Scientific Production Enterprise Kartographia / OCHA HDX,
distributed in geoBoundaries' humanitarian dataset.

**License:** Creative Commons Attribution 3.0 IGO.

Upstream dataset: `https://data.humdata.org/dataset/cod-ab-ukr`

Pinned distribution:
`https://media.githubusercontent.com/media/wmgeolab/geoBoundaries/9469f09/releaseData/gbHumanitarian/UKR/ADM2/geoBoundaries-UKR-ADM2_simplified.geojson`

The source represents 2022 administrative boundaries. Its 139 features include
the Chornobyl exclusion zone and special-status cities. The generator joins the
exclusion-zone geometry into the Vyshhorod display area, producing 138 display
areas, with eight on the home view: seven districts and Kyiv city.

The generator triangulates polygons with holes, projects coordinates with a
fixed aspect ratio, rasterizes native 700 x 420 selection/color masks, and
compresses them with lossless run-length encoding. The same masks drive drawing
and hit testing, including zoom/pan inversion. This avoids thousands of LVGL draw
tasks and runtime GeoJSON allocation on ESP32.

Zoom enlarges the native masks; it does not download tiles or add geographic
detail. District boundaries remain independently selectable.

Tryvoha's location directory provides names and slugs. UA Siren's region tree
provides parent relationships after matching both identifiers and names.
Community alerts are attached to their actual district, not the entire oblast.
Kyiv city is a separate root.

Explicit aliases cover renamed districts and transliteration differences.
Source coverage is separate from geometry: unmapped/unsupported areas stay gray.
Crimea and Sevastopol remain visible within Ukraine's internationally recognized
borders. Some occupied-area data is unavailable from the selected directory.

`assets/map/source-manifest.json` records source URLs, hashes, license, area
mapping and compressed size. Generated code is `generated/map_assets.cpp`.
Regenerate it using `tools/map/generate.ts`; do not hand-edit generated masks.
