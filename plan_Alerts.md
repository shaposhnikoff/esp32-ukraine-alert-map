# Ukraine Alerts Implementation Plan

**Implementation amendments:** The owner's approved startup view is Kyiv city
and Kyiv oblast's seven districts; the national map is a second tab. Kyiv city
has an independent status. Drone layers are excluded. All configuration,
including Wi-Fi credentials, is supplied through a separate build-time JSON
file, with no settings/provisioning screen. These decisions supersede older
screen/settings requirements below. See `docs/implementation-scope.md`.

**Goal:** Develop a standalone touchscreen application that colors every Ukrainian district on an interactive map: red or yellow for the source-reported alert level, green when fresh supported data reports no active alert, and gray when status is unavailable.

**Architecture:** Target the Waveshare ESP32-S3-Touch-LCD-7B requested by the owner. Replace inverter telemetry and control with free, unauthenticated Tryvoha.online polling, a typed regional state model, and an offline vector map rendered with LVGL. Use UA Siren as a reserve provider through a separate adapter. A Raspberry Pi is not required.

**Tech stack:** C/C++, ESP-IDF, FreeRTOS, LVGL, ESP-IDF HTTPS client, NVS, CMake, host-side tests, and GitHub Actions. Prefer LVGL 9 after demonstrating compatibility with the exact 7B board. The manufacturer's examples include LVGL 8.x; document and test any port rather than mixing incompatible APIs. Pin the verified versions.

**Research date:** November 21, 2026.

**Current deliverable:** This development plan. Firmware implementation, device flashing, and deployment are separate execution steps.

---

## 1. Mission and scope

Develop **Ukraine Alerts**, a complete application for the 7-inch ESP32-S3-Touch-LCD-7B touchscreen.

The main screen must show a recognizable district-level map of Ukraine. Each district has its own status fill and visible boundary, rather than inheriting a single visual color from the oblast summary. Users must be able to select a district, inspect available alert details, zoom, pan, and return to the national view.

Use the owner's corrected 7B hardware target. A browser interface or mobile application can be a separately scoped extension.

Remove the original project's inverter-specific requirements:

- Deye telemetry, Modbus, serial adapters, and writable inverter registers.
- Battery, PV, grid, generator, and energy-flow screens.
- Inverter control commands, write confirmations, and register readback.
- Mandatory integration with the existing Raspberry Pi exporter.

Retain the applicable embedded requirements: display and touch initialization, Wi-Fi setup, background networking, persistent settings, diagnostics, reproducible builds, recovery behavior, and firmware update support.

### Minimum release

1. Interactive national map with individual district polygons, oblast outlines, and separate special-status city selection.
2. Red and yellow source-reported alert levels per district; green for no active alert according to fresh supported data.
3. Explicit partial-coverage, unknown, stale, and offline presentation.
4. Region selection and an alert-details panel.
5. Favorite region selection and a persistent home-region indicator.
6. Ukrainian UI by default, with English and Russian localization resources.
7. Wi-Fi provisioning without recompiling firmware; no registration, API key, or Authorization header required for either selected provider.
8. Connection status, last successful validation time, and data age.
9. Host simulation, deterministic fixtures, tests, and exact-board build.
10. Setup, flashing, recovery, and API-access documentation.

District boundary maps are mandatory for the minimum release. Historical statistics, push notifications, and audio alarms are outside that release. Do not require unverified audio hardware.

## 2. Verified free APIs and provider selection

### Primary: Tryvoha.online

Documentation: `https://tryvoha.online/api`

Source explanation: `https://tryvoha.online/how-it-works`

| Purpose | Method and URL | Application use |
| --- | --- | --- |
| All current alerts | `GET https://tryvoha.online/api/v1/alerts` | One national poll every 60 seconds |
| Region directory | `GET https://tryvoha.online/api/v1/regions` | Slugs, names, types, and parent mapping |
| Kyiv city | `GET https://tryvoha.online/api/v1/alerts/kyiv` | Separate city status and contract fixture |
| Kyiv oblast | `GET https://tryvoha.online/api/v1/alerts/kyivska` | Full/partial coverage verification |
| Selected region | `GET https://tryvoha.online/api/v1/alerts/{slug}` | Optional on-demand detail refresh |

No account, key, or Authorization header is required. The documented limit is 120 requests per minute per IP, with responses cached for 30–60 seconds. The project deliberately polls once per 60 seconds. Free commercial and non-commercial use requires credit to Tryvoha.online; availability is not guaranteed.

The national response is an object with `updated_at`, numeric `active`, and an `alerts` array. `active` is a count here, not a boolean. Each alert includes `slug`, `type`, names, `level`, and `started_at`; subordinate locations may provide `oblast_slug`.

The provider documents `yellow` as a drone threat and `red` as a missile threat or regular air raid alert. Display these as provider classifications. They are not independently calculated danger scores or guaranteed universal definitions shared by every source.

Example of the single-oblast contract:

```json
{
  "slug": "kyivska",
  "type": "state",
  "active": false,
  "level": null,
  "started_at": null,
  "districts_active": [
    {
      "slug": "brovarskii-raion",
      "level": "yellow",
      "started_at": "2026-11-21T12:44:29.000000Z"
    }
  ],
  "active_anywhere": true
}
```

This abbreviated fixture illustrates semantics, not a permanent current status. `active: false` means no oblast-wide alert; `active_anywhere: true` means some part is affected. Use `active_anywhere` for an "any alert in this oblast" indicator and `districts_active` for affected districts. Do not color the entire oblast as fully covered.

For the national poll, derive the same distinction from parent-level and subordinate records rather than requesting every oblast separately.

### Reserve: UA Siren

Endpoint: `GET https://siren.pp.ua/api/v3/alerts`

Provider page: `https://siren.pp.ua/`

The response is an array of regional records. Relevant fields observed:

```text
regionId, regionType, regionName, lastUpdate
  activeAlerts[]
    type
    lastUpdate
    activeAlertLevels[]
      alertLevel
      createdAt
```

Filter nested `activeAlerts` by `type == "AIR"`. Map observed `alertLevel == "Red"` and `"Yellow"` through this provider's adapter; do not include `ARTILLERY` or `URBAN_FIGHTS` as air raid alerts.

Preserve regional IDs, record timestamps, and source identity. The sampled response contained active records, not an exhaustive directory of every inactive region. Confirm full-snapshot semantics and parent relationships before interpreting an absent record as an all-clear. Unknown or unmapped locations remain unavailable.

UA Siren is a reserve transport/source adapter, not a vote against the primary. Both draw alert information from UkraineAlarm, so the reserve does not provide an independent underlying warning system. Tryvoha also describes a separate stream of processed public threat messages; that stream and its estimated trajectories are outside this application's scope.

### Checks actually performed

On November 21, 2026, at approximately 13:16 UTC, all four requests below returned HTTP 200 and JSON without authorization:

- Tryvoha national alerts: 54 active records, both `red` and `yellow`; response `updated_at` was approximately five seconds before the HTTP response time.
- Tryvoha region directory: 173 entries in the sampled response.
- Tryvoha Kyiv oblast: `active: false`, `active_anywhere: true`, with seven affected districts.
- UA Siren national alerts: 57 regional records; nested AIR levels included both `Red` and `Yellow`.

These counts are observations of one changing snapshot, not hardcoded contracts. Approximate serialized response sizes were 12 KB, 30 KB, and 24 KB for primary alerts, the directory, and reserve alerts respectively; they are not maximum sizes.

Tryvoha's sampled directory contained 118 `district`, 27 `state`, and 28 `community` records. This is not a complete nationwide administrative inventory: do not treat every district missing from the directory as inactive.

The directory did not include Crimea or Sevastopol in this check. Keep their geometry visible and status unavailable under this provider. The directory's `type: "state"` also includes some city-level locations; it is not sufficient by itself to identify the 24 oblasts.

Alerts.in.ua was also evaluated and returned 401 without its required personal token. It is not a dependency or a token-onboarding blocker for the selected free architecture.

### Integration acceptance

1. Reconfirm these contracts and terms when implementation begins.
2. Save dated JSON fixtures with provider identity and retrieval time.
3. Map known region slugs and IDs to the geographic manifest.
4. Test district grouping, inherited oblast-wide alerts, full/partial coverage, green inactive districts, unsupported coverage, and mixed-level states.
5. Verify reserve mappings and omission semantics before enabling automatic switch-over.
6. Check direct HTTPS operation on the actual ESP32-S3.

HTTP checks on the workstation do not constitute firmware or hardware integration tests.

## 3. Hardware and implementation constraints

Target the **Waveshare ESP32-S3-Touch-LCD-7B**, the 7-inch, 1024 × 600 touch variant.

Hardware baseline checked against the manufacturer's 7B documentation:

- ESP32-S3.
- 16 MB Flash and 8 MB PSRAM.
- RGB LCD and GT911 capacitive touch.
- 2.4 GHz Wi-Fi connectivity.
- Type-C 5 V power input.

Manufacturer documentation:

`https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7B`

Use the exact 7B demo resources linked from that page:

`https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7B#Resources`

Before implementing board initialization, verify the exact 7B revision, GPIO assignments, pixel clock, framebuffer arrangement, power sequencing, and compatible ESP-IDF/LVGL versions. Do not reuse a 5-inch or non-B board configuration just because the resolution matches.

The 7B pin table identifies I2C on GPIO8/GPIO9, touch interrupt on GPIO4, touch reset on EXIO1, display enable on EXIO2, and LCD voltage enable on EXIO6. Verify the expansion-chip driver and initialization order against the exact demo and schematic. Treat these as board-specific responsibilities, not UI code.

The vendor documents older ESP-IDF/LVGL examples and display-drift/performance considerations. Prove the selected version combination and RGB timing on this board; do not copy experimental memory-clock settings into production without validation.

Use native ESP-IDF networking and storage. Avoid Arduino dependencies unless the verified board support requires them and the decision is documented.

All LVGL changes must run in the UI task. Network tasks publish state updates through a bounded queue; they must not mutate LVGL objects.

## 4. Alert semantics and presentation

Keep these concepts independent:

```text
AlertLevel: red | yellow | unspecified
Coverage: full | partial | no_alert_reported | unknown
Freshness: fresh | stale | unavailable
Connection: connecting | online | retrying | reserve_active | blocked | offline
```

Red and yellow are source-reported levels, not a risk score calculated from neighboring regions, news, elapsed time, or aircraft activity.

| Normalized input | Map presentation | Text meaning |
| --- | --- | --- |
| Fresh full red | Red fill plus alert icon | Source reports a full red alert |
| Fresh full yellow | Yellow fill plus distinct icon | Source reports a full yellow alert |
| Fresh partial red/yellow | Severity-colored hatch plus partial badge | Alert affects only subordinate locations |
| Fresh partial, level missing | Neutral hatch plus partial badge | Partial alert; level unavailable |
| Fresh supported district with no effective alert | Green fill | No active alert reported by source |
| Invalid, unmapped, or unsupported status | Gray fill plus unknown pattern | Status unavailable |
| Stale previously known status | Retain last color under stale overlay | Last known status; show age |
| No validated data since startup | Unknown map | Waiting for data |

Coverage and severity are different fields. A partial alert can be red or yellow.

For partial coverage, show affected districts or communities and their reported levels. If an oblast-wide yellow alert coexists with a red district alert, retain the full yellow base plus a red partial overlay and explain both in the panel. Store `full_alert_level` and `subregion_max_level` separately.

If detailed records contain both levels, list the records separately. Any summary that prioritizes red over yellow must retain the affected location and partial-coverage label.

Green means "no active alert reported by this source", not "guaranteed safe". Unknown data, unavailable coverage, or lost connectivity must never turn a district green.

Loss of connectivity is not an all-clear event. Do not clear an existing alert because a request failed, a provider changed, or a response could not be parsed.

For a record with an absent or unknown level, retain `unspecified`. Do not derive a level from optional free text. Label provider-specific level descriptions and reserve mode explicitly.

### District status calculation

For each geographic district:

1. Find its verified provider mapping and parent oblast.
2. Collect active alerts addressed directly to that district.
3. Include any active oblast-wide alert that covers the district.
4. Keep subordinate community/city alerts as partial coverage of that district where the parent mapping is verified.
5. Choose the highest known applicable level, with red taking precedence over yellow, while retaining contributing records and their actual scope.
6. Use solid red/yellow for full district coverage; use the same severity color with hatching and a partial label when only subordinate locations are affected.
7. Use green only if the district is supported, the complete snapshot is valid and fresh, and no direct, inherited, or subordinate applicable alert is reported.
8. Use gray for unsupported or unresolved mappings, unavailable source status, or unclassified active alerts. An uncertain active record must never become green.

When an oblast-wide red alert is active, every covered district must remain red even if that district has no separate record in the national active list. Conversely, one district alert must not color all neighboring districts red.

The main map must expose district colors at country scale with thin district borders and stronger oblast borders. An oblast overview may be added as a secondary mode; it cannot replace the required district view.

## 5. Interactive map and geographic data

### Geographic coverage

Represent the current administrative district boundaries across Ukraine, grouped within the 24 oblasts and the Autonomous Republic of Crimea, with Kyiv city and Sevastopol handled separately. Document how special-status cities and any administrative units without provider coverage are represented.

Show Ukraine within its internationally recognized borders, including Crimea. Alert-source coverage and administrative boundaries are separate concerns; unavailable status must not remove a location from the map.

Kyiv city must be selectable separately from Kyiv oblast. Sevastopol must be selectable separately from Crimea. Use labeled callouts or enlarged selection targets where polygons are too small.

### District geometry source and pipeline

Use bundled vector geometry rather than live map tiles or a browser map engine.

Use an ADM2 dataset that matches the district structure after the 2020 administrative reform. An oblast-only ADM1 map cannot satisfy the district-color requirement.

Candidate metadata checked during planning:

`https://www.geoboundaries.org/api/current/gbHumanitarian/UKR/ADM2/`

It identifies Ukrainian ADM2 boundaries representing 2022, 139 administrative features, data sourced from Kartographia/HDX, and a CC BY 3.0 IGO license. These feature counts may include special administrative units; verify the actual geometry and mapping rather than assuming every feature is an ordinary district.

The metadata points to the upstream dataset:

`https://data.humdata.org/dataset/cod-ab-ukr`

Reject the currently published `gbOpen/UKR/ADM2` candidate for this project: its metadata identifies 2006 boundaries and 495 units, which do not match the required contemporary district structure.

The candidate geometry itself has not been downloaded or mapped in this planning task. Before adoption, inspect current boundaries, administrative codes, Crimea, special cities, source attribution, and the exact license requirements. Select another documented source if any required boundary or mapping is missing.

Build-time processing must:

1. Pin the dataset revision and record its checksum and license.
2. Select contemporary district boundaries and their parent oblasts, plus required special city geometry.
3. Review Crimea, Sevastopol, Kyiv city, and Kyiv oblast explicitly.
4. Preserve multipolygons, holes, disconnected parts, and shared borders.
5. Simplify geometry without visible gaps or overlaps.
6. Project to a documented local coordinate system with preserved aspect ratio.
7. Generate render geometry, selection geometry, and bounding boxes.
8. Produce compact C assets stored in Flash, with stronger oblast outlines and per-district fills.
9. Generate two or more geometry detail levels for the national view and zoomed district selection, with a measured vertex/memory budget.

Keep source geometry out of runtime JSON parsing. Implement a reproducible converter with Bun/TypeScript and pinned dependencies only where needed.

### Location mapping

Create a versioned manifest mapping:

```text
geographic_region_id -> geometry_id -> localized_name
provider + canonical_slug_or_id -> geographic_region_id + parent_region_id
```

Build a separate mapping for each provider. Tryvoha uses canonical slugs and optional parent slugs; UA Siren uses its own IDs and region types. Validate aliases such as the optional `-oblast` suffix using the API's canonical responses. Do not infer parentage from translated names or assume provider IDs are interchangeable.

Maintain stable administrative IDs for districts and parent oblasts. Keep a reviewed explicit alias table for provider slugs, renamed districts, and transliteration differences; matching names alone is not a release-quality mapping.

Validate the full district inventory from the approved geometry source and its 27 top-level geographic containers, while allowing provider mappings to be explicitly unsupported. Include tests for Kyiv city versus Kyiv oblast, absent Crimea/Sevastopol coverage, city-level `state` records, district renaming, and subordinate aliases. Missing coverage must render gray rather than green.

### Required interactions

- Tap an individual district to select it and open its details.
- Drag to pan when zoomed.
- Use large `+` and `-` buttons for zoom.
- Use a Home button to restore the full-country view.
- Preserve selection during background data updates.
- Provide oblast-to-district list navigation and search for districts too small to tap at country scale.
- Highlight the selected district boundary and name without hiding its current status color.

Pinch zoom is optional; basic navigation must not depend on multitouch.

Use one map transform and its inverse for rendering and hit testing. Test selection after zoom, pan, clipping, and display offset changes. Resolve shared-edge hits deterministically.

## 6. Screens and UI requirements

### Map overview

Use most of the 1024 × 600 screen for the map. Include:

- Source name and connection indicator.
- Last successful validation time and current data age.
- Red alert, yellow alert, green no-active-alert, partial hatching, and gray unavailable legend entries.
- Selected region name and current status.
- Compact home-region status.
- Zoom and national-view controls.
- A visible demo/stale/offline banner when applicable.

Avoid continuous full-map animations. Update only changed districts and relevant labels. Test dense district boundaries at native resolution; zooming must expose clear, selectable district geometry without gaps.

### Region details

Display:

- Localized district name, parent oblast, and provider slug or ID.
- Effective level and coverage as separate values, with direct, inherited oblast-wide, and subordinate contributors identified.
- Source-reported start time when available.
- Elapsed duration only when a valid start time exists.
- Optional source-reported threat types and descriptions.
- Record update time, detail retrieval time, and map validation time.
- Favorite/home-region action.

Display source-provided `started_at` or the corresponding reserve level timestamp only when its semantics are verified. Do not substitute the time of the first local observation.

National map data and optional region details may come from different requests. Keep their timestamps and source identities separate. If details disagree with a newer national response, label the details as older and schedule a permitted refresh.

### Active-alert list

Provide a scrollable list with search or region filtering. Order red, yellow, and unspecified entries consistently while preserving location names and partial coverage.

Allow filtering by oblast and selecting individual districts from the list. The user must be able to reach every mapped district without precisely tapping a tiny polygon.

The primary map and alert list concern air raid alerts and the selected provider's red/yellow classifications. The reserve adapter accepts only nested `type == "AIR"`. Supporting other threat categories requires explicit labels and tests.

### Network and API setup

Support Wi-Fi provisioning, primary/reserve status, connection testing, and network credential replacement. Neither selected API needs token-entry UI.

Use an established ESP-IDF provisioning approach or a temporary setup access point with an on-screen pairing secret. Do not expose an unauthenticated permanent configuration portal.

### Settings and diagnostics

Provide theme, brightness, dimming, language, favorite region, firmware version, restart, update, and confirmed factory reset.

Diagnostics must show Wi-Fi RSSI, free heap, available PSRAM, uptime, active provider, API response category, request budget, retry delay, source timestamp, last successful validation, and geometry/manifest version. Never expose Wi-Fi credentials.

Use readable Cyrillic fonts, large touch targets, and non-color status cues. Store timestamps in UTC; display local time using verified Europe/Kyiv timezone rules.

## 7. Application architecture

```text
Tryvoha HTTPS / UA Siren HTTPS
        |
        v
Shared request scheduler
        |
        v
Provider-specific parser and region adapter
        |
        v
Validated typed snapshots
        |
        v
Bounded state-update queue
        |
        v
UI task: state, map, details, status banners
```

Suggested modules:

```text
app/             Startup and lifecycle
board/           Exact 7B display, power sequencing, and touch support
network/         Wi-Fi, HTTPS, time synchronization
alerts_api/      Scheduler, provider parsers, reserve switching
alerts_model/    Levels, coverage, snapshots, location manifest
map/             Geometry, transforms, rendering, hit testing
ui/              Screens, widgets, themes, localization
storage/         Device settings and credentials
ota/             Firmware update and rollback
diagnostics/     Redacted logs and health information
```

Use a single owner for each mutable state structure. Publish complete validated snapshots rather than updating visible regions while a response is still being parsed.

Suggested application state:

```text
AppState
  ConnectionState
  NationalSnapshot
    provider
    status_by_location
    effective_status_by_district
    full_alert_level_by_location
    subregion_max_level_by_location
    received_at_utc
    validated_at_monotonic
    source_updated_at
    snapshot_revision
  DetailSnapshot
    air_raid_records
    fetched_at_utc
    validated_at_monotonic
  SelectedLocation
  MapViewport
  UserSettings
  Diagnostics
```

Retain provider identifiers without loss and keep their namespaces separate. Do not equate a UA Siren `regionId` with a Tryvoha directory ID.

Do not save a new snapshot to NVS on every poll. Initially keep live snapshots in RAM; after reboot show unknown until revalidated. Persist only user settings and credentials.

## 8. Polling, freshness, and recovery

### Request budget

Initial application defaults:

- Primary national poll: once every 60 seconds.
- Region directory: bootstrap and infrequent refresh, with a versioned bundled manifest available for startup.
- Optional region detail refresh: coalesced, at most once every 60 seconds.
- Application cap: at most four outgoing attempts in any rolling minute across polling, reserve checks, setup tests, and user refreshes.
- One request in flight.
- Eight-second request timeout as an initial configurable value.
- Failed attempts, setup tests, and manual refreshes use the same scheduler.

These are conservative project defaults, not additional provider guarantees. Revalidate published limits before implementation. UA Siren quota/SLA was not established; use the same conservative polling interval and respect any returned throttling headers.

Do not issue 27 requests to update 27 regions. One national response updates the whole map.

Conditional requests are optional: only enable them if the selected endpoint actually supplies validators and their behavior is verified. Keep validators separate by provider and URL. A 304 may refresh validation time only if a valid cached representation exists for that exact request.

A successful 304 confirms that the provider has not changed the representation; it does not prove that the upstream information has no delay.

### Failure handling

- On 401, show an unexpected provider-contract/access error; do not invent a token-registration requirement for the free APIs.
- On 403, show blocked-access status and require explicit configuration testing.
- On 429, honor `Retry-After` if present and back off without exceeding the budget.
- On timeout, disconnect, or 5xx, use bounded exponential backoff with jitter.
- On malformed JSON, wrong root type, invalid required fields, or inconsistent counts, reject the complete snapshot and keep the last valid one.
- Cap response size and parser memory. Determine bounds from the verified schema and measured fixtures, and document the chosen limits.
- TLS certificate or clock-validation failures must not trigger insecure HTTP fallback.

After reconnection, obtain a fresh snapshot before removing stale indicators. Do not replay old transitions as new alerts.

### Reserve policy

After two consecutive primary transport/server failures, try one reserve request through the same scheduler. A malformed required schema is also a provider failure; an honest no-alert or unsupported location is not.

Switch only after validating the reserve snapshot and mappings. Replace state atomically, show "UA Siren: reserve", and mark unsupported regions unavailable. Do not merge old primary records into a new reserve snapshot or treat a missing reserve record as all-clear without verified omission semantics.

While using the reserve, poll it every 60 seconds and probe the primary at most once every five minutes. Return after two consecutive valid primary probes; retain source labels and timestamps through the switch.

If both fail, show the explicit "Data unavailable" banner and mark retained last-known values stale.

### Freshness

Use a monotonic clock to measure time since successful validation.

Initial stale threshold: 180 seconds, allowing for 60-second polling and provider caching. Until the first valid snapshot, status is unavailable. After the threshold, keep the last known level and show a persistent stale overlay.

Keep source snapshot time, individual record timestamps, HTTP metadata, receipt time, and validation age separate. Reject or flag a primary snapshot whose documented `updated_at` exceeds the source-age threshold even if HTTP succeeds. Verify whether that field represents refresh or content-change time before enforcing this rule.

UA Siren record `lastUpdate` and level `createdAt` may legitimately be old for an ongoing alert; they are not verified snapshot heartbeats. An unchanged long-running alert is not stale solely because its start/record timestamp is old. Show "source freshness unavailable" where an upstream heartbeat is absent.

The displayed status must never imply that the application can detect all delays inside the provider's upstream sources.

## 9. Resource management and reliability

A single RGB565 framebuffer at 1024 × 600 occupies 1,228,800 bytes. Two occupy 2,457,600 bytes before map buffers, UI assets, networking, and task stacks.

Create a measured PSRAM/heap budget before choosing buffering and map rendering strategies. Triangulate complex region shapes at build time when necessary; do not assume a renderer supports arbitrary concave polygons with holes.

The district map contains substantially more polygons than an oblast map. Budget geometry, hit testing, status arrays, and draw buffers for the complete approved district inventory, not just 27 polygons.

Requirements:

- Bounded queues, input bodies, geometry, and detail records.
- No blocking HTTPS work in the UI task.
- No full widget-tree reconstruction after every poll.
- No steady increase in heap usage during screen switching.
- Touch input remains usable while requests time out.
- Reconnection does not create duplicate polling tasks.
- Brownout/reboot starts in unknown status until fresh data arrives.
- OTA updates use verified images and a tested rollback path.

Use a host simulator to develop the UI and feed fixtures without exhausting live API quotas.

## 10. Security and provider conditions

- Provision Wi-Fi credentials at runtime.
- Do not introduce a shared API key; both selected providers work without one.
- Redact setup credentials and sensitive diagnostic exports.
- Verify HTTPS certificates.
- Assess encrypted NVS, Flash encryption, and secure boot against the actual hardware and provisioning process; do not claim physical extraction resistance without enabling and testing it.
- Limit the lifetime of setup mode and require physical/on-screen authorization.
- Do not collect precise user location; choosing a favorite region is sufficient.
- Document the provider's terms and source attribution.

Credit Tryvoha.online visibly in About/help and provide its source URL. Include a concise source-and-delay notice; the provider offers no availability guarantee. Do not market the device as a guaranteed emergency notification channel.

If a shared service is later needed, one upstream poller and cache can reduce duplicate requests. A Raspberry Pi may host that optional gateway, but must not be coupled to inverter infrastructure.

## 11. Proposed repository layout

```text
alarms_monitor/
  plan.md
  plan_Alerts.md
  README.md
  LICENSE
  CMakeLists.txt
  sdkconfig.defaults
  dependencies.lock
  main/
    CMakeLists.txt
    app_main.c
  components/
    app/
    board/
    network/
    alerts_api/
    alerts_model/
    map/
    ui/
    storage/
    ota/
    diagnostics/
  assets/
    map/
      sources.json
      locations.json
      generated/
    fonts/
  tools/
    map/
      generate.ts
      package.json
      bun.lock
  simulator/
  tests/
    CMakeLists.txt
    unit/
    integration/
    fixtures/
  docs/
    api-contract.md
    api-access.md
    hardware-compatibility.md
    map-data.md
    ui-design.md
    test-plan.md
    test-results.md
    build.md
    flashing.md
    troubleshooting.md
    diagrams/
      alerts-data-flow.md
    adr/
  .github/
    workflows/
      ci.yml
```

Create files when their implementation phase needs them. Preserve the original `plan.md`. Do not create empty modules merely to match this suggested tree.

## 12. Execution roles

The implementer may perform these roles sequentially. Use subagents only when permitted by the execution environment and task instructions.

| Role | Responsibility | Evidence |
| --- | --- | --- |
| Research and integration | Free-provider contracts, regional coverage, reserve mappings, limits | API contract and dated fixtures |
| Embedded architecture | Board support, task ownership, memory, recovery | Build and architecture decisions |
| Map/UI engineering | Contemporary ADM2 geometry, district colors, interactions, localization | Simulator captures and per-district selection tests |
| Firmware implementation | Provisioning, polling, state, settings, OTA | Working modules and integration tests |
| QA and release | Failure scenarios, CI, hardware checks, packaging | Test results and release artifacts |
| Review | Correctness, semantics, credentials, concurrency | Findings with severity and resolution |

Avoid overlapping file ownership. Use isolated worktrees for parallel writing tasks. Commit and deploy only when separately authorized.

## 13. Implementation phases and acceptance

For each task, add a deterministic failing test where practical, implement the smallest working change, run the relevant checks, and record evidence. Documentation and board bring-up use review/build/hardware evidence where a host test cannot prove the result.

### Phase 0: Environment and contracts

Files: `docs/hardware-compatibility.md`, `docs/api-access.md`, `docs/api-contract.md`, `docs/adr/`.

1. Inspect repository instructions, existing files, toolchains, and attached hardware.
2. Verify the 7B display examples, power sequencing, and compatible dependency versions.
3. Record both free API contracts, terms, and live sample observations.
4. Verify the contemporary district geometry inventory, map districts/parents to provider slugs and IDs, and record unsupported coverage.
5. Pin the initial dependency set and define host-test commands.

Acceptance: documented hardware configuration, version choices, district/oblast manifest contract, candidate geometry license and year, and a clear distinction between researched and live-tested API behavior.

### Phase 1: Firmware and simulator bootstrap

Files: root build files, `main/app_main.c`, `components/board/`, `components/app/`, `simulator/`.

1. Create the exact-board firmware project.
2. Initialize display, touch, LVGL, and the UI task.
3. Render a labeled startup screen and unknown connection state.
4. Add a host simulator with the same UI/model code where practical.
5. Record framebuffer, heap, and PSRAM allocation.

Acceptance: reproducible ESP32-S3 build, working simulator, and display/touch checked on hardware when available. Explicitly mark hardware checks pending if no device is attached.

### Phase 2: Typed model and provider parsers

Files: `components/alerts_model/`, `components/alerts_api/`, `assets/map/locations.json`, `tests/unit/`, `tests/fixtures/`.

1. Add fixtures for red, yellow, partial, green inactive, gray unsupported, unknown-level, and malformed responses.
2. Test manifest coverage, aliases, parent mapping, and distinct provider namespaces.
3. Implement lossless parsing and atomic snapshot replacement.
4. Implement level, coverage, freshness, source identity, and connection state separately.
5. Test national grouping, numeric versus boolean `active`, and `active_anywhere`.
6. Filter reserve records by `AIR` and test Red/Yellow mapping.
7. Test that an oblast-wide alert propagates to covered districts and a district alert does not affect its neighbors.
8. Test that failure or unsupported mapping never turns a district green.

Acceptance: all parser/state tests pass, including red/yellow distinction, district inheritance, partial coverage, green only for known inactive districts, and gray unsupported districts.

### Phase 3: Geometry and interactive map

Files: `tools/map/`, `assets/map/`, `components/map/`, `docs/map-data.md`, `tests/unit/`.

1. Verify a licensed post-reform ADM2 source with all required district boundaries and administrative identifiers.
2. Generate pinned, simplified render and hit-test geometry.
3. Test holes, multipolygons, city/oblast separation, and shared edges.
4. Implement independent red/yellow/green/gray district fills, partial hatching, oblast outlines, selection, pan, zoom, and reset.
5. Test inverse transforms at several scales and offsets.
6. Review simulator captures at native 1024 × 600 resolution.

Acceptance: every district in the approved inventory is represented and selectable, district status changes independently, red/yellow/green are distinct, unsupported coverage is gray, small districts/cities remain accessible, and geometry/attribution are reproducible.

### Phase 4: Wi-Fi, free API integration, and reserve switching

Files: `components/network/`, `components/alerts_api/`, `components/storage/`, `tests/integration/`, `docs/api-access.md`.

1. Add Wi-Fi setup and redacted diagnostics.
2. Add certificate validation and time synchronization.
3. Implement the single shared request scheduler and rolling budget.
4. Implement 60-second national polling, retry state, and validated reserve switching.
5. Test 200, 304, 401, 403, 429, timeout, invalid data, and reconnect.
6. Run unauthenticated smoke tests for both providers on the device when available.
7. Verify reserve completeness/mapping semantics and primary return behavior.

Acceptance: network failures do not block touch; request attempts stay within budget; no API key is required; provider switches are visible; demo data is never silently shown as live.

### Phase 5: Details, localization, and settings

Files: `components/ui/`, detailed parser in `components/alerts_api/`, `components/storage/`, `tests/unit/`, `tests/integration/`.

1. Parse optional detailed fields and retain only the selected air raid classifications.
2. Add on-demand detail caching within the same request budget.
3. Implement district details, oblast-to-district navigation, active list, home district, and the complete color legend.
4. Add Ukrainian, English, and Russian resources with Cyrillic fonts.
5. Test stale detail timestamps, missing levels, mixed subordinate levels, and partial coverage.
6. Persist settings without persisting every network snapshot.

Acceptance: all core screens work in the simulator, unknown values remain explicit, and older detail data cannot silently override a newer national snapshot.

### Phase 6: Reliability, updates, and diagrams

Files: `components/ota/`, `components/diagnostics/`, `tests/`, `docs/diagrams/alerts-data-flow.md`, `docs/test-results.md`.

1. Test stale detection using a fake monotonic clock.
2. Test Wi-Fi loss, primary/reserve failures, malformed bodies, reboot, and recovery.
3. Test repeated navigation and selection during updates.
4. Implement and test the selected OTA/rollback approach.
5. Run an eight-hour simulator soak and, when available, a 24-hour device soak.
6. Record observed memory behavior and unresolved hardware checks.
7. Save a Mermaid diagram of polling, validation, state updates, and stale/recovery transitions.

Acceptance: no crash or monotonic memory-growth trend, no duplicate pollers, no secret leakage, and no all-clear caused by an error.

### Phase 7: CI, documentation, and release

Files: `.github/workflows/ci.yml`, `README.md`, `docs/build.md`, `docs/flashing.md`, `docs/troubleshooting.md`, `docs/test-results.md`.

1. Add format/static checks, host tests, geometry validation, and exact-board firmware build.
2. Pin toolchains and actions, set explicit job timeouts, and keep CI independent of live services.
3. Publish build artifacts with version, checksum, board variant, and dependency metadata.
4. Document free API access, attribution, Wi-Fi setup, flashing, recovery, map semantics, and provider delays.
5. Review code, licenses, request limits, and remaining findings.
6. Produce a report separating simulated, live-provider, and hardware validation.

Acceptance: deterministic CI passes, documentation supports a clean setup, and every unverified item is named.

## 14. Required test scenarios

| Scenario | Expected result |
| --- | --- |
| No alert -> yellow -> red -> no alert | Correct source-reported transitions, without treating transport loss as all-clear |
| Red -> yellow | Remain in an active source-reported state with yellow presentation |
| Partial alert with no level | Partial badge and unavailable level |
| Partial alert with red and yellow subordinate records | Separate detailed entries; preserve partial coverage |
| One district red, adjacent district yellow, third district inactive | Independent red, yellow, and green fills; no oblast-wide replacement color |
| Active oblast-wide red, no separate district record | All supported districts within that alert scope are red |
| Active subordinate community only | Parent district shows partial severity hatching; unaffected neighbors remain unchanged |
| Fresh complete snapshot, mapped district without any applicable alert | Green with no-active-alert label |
| District missing from the provider directory | Gray unavailable, even if absent from the active list |
| Oblast `active: false`, `active_anywhere: true` | Partial alert from `districts_active`; not an all-clear |
| National `active: 54` | Parse as count, not regional boolean |
| Directory contains city-level `type: "state"` | Manifest decides geographic scope; do not merge with the oblast |
| Reserve contains ARTILLERY alongside AIR | Only AIR affects the air raid state |
| Kyiv city changes, Kyiv oblast does not | Independent regions update |
| Crimea/Sevastopol status unavailable | Both remain on the map with explicit unavailable status |
| Wrong root type, malformed JSON, or invalid required fields | Reject snapshot without corrupting regional assignments |
| 304 after valid cached data | Refresh validation age without clearing the state |
| 304 without cached data | Remain unavailable and schedule a permitted full request |
| 401 or 403 | Visible access error; no token-registration loop or request storm |
| 429 with retry information | Respect retry delay and local budget |
| Wi-Fi loss during red alert | Retain red last-known state; show stale/offline indication |
| Clock correction | Freshness remains correct through monotonic timing |
| Missing start time or level | Show unavailable value, not a fabricated duration or color |
| Different ages of national and detailed responses | Show ages separately; newer national state is authoritative for overview |
| Primary fails twice | Validate and switch to reserve; display source name |
| Both providers fail | Show Data unavailable and stale last-known state |
| Reserve omits an unmapped location | Unknown, never an inferred all-clear |
| Primary recovers | Return after validated probes without mixing snapshots |
| Unknown future threat type | Display a labeled unknown type without crashing |
| Zoom/pan and district selection | Correct district selected using inverse transform; selection matches the rendered boundary |
| Reboot without network | Unknown status until revalidation; persisted settings restored |
| Demo mode | Persistent demo label; no implication of live status |
| Repeated manual refresh | Shared scheduler still enforces the rolling budget |

## 15. Verification commands to implement

These commands define the intended developer interface. The project currently contains a plan, so these are future acceptance commands, not commands already executed.

Firmware:

```bash
idf.py set-target esp32s3
idf.py build
```

Host tests:

```bash
cmake -S tests -B build-host
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Geometry validation:

```bash
bun install --cwd tools/map --frozen-lockfile
bun run --cwd tools/map check
```

Define the `check` script to validate source checksums, licenses, manifest completeness, geometry integrity, and generated-asset reproducibility.

Firmware flashing is a hardware action requiring separate authorization. Document the actual port-specific command after the board is identified.

No CI job should require network access to live alert services. Run live-provider smoke tests explicitly and use dated fixtures for ordinary tests.

## 16. Definition of done and final report

The application is ready for its intended personal-device use only when:

- The exact 7B firmware builds reproducibly.
- Display and touch are checked on the actual device.
- The complete district map supports individual district selection, pan, zoom, and reset.
- Every supported district independently displays red, yellow, or green according to fresh applicable data.
- Unsupported, stale, or unknown coverage is visibly distinguished and never appears as fresh green.
- Red and yellow come from the provider contract, including correctly inherited oblast-wide alerts.
- Partial coverage is not confused with yellow severity.
- Contemporary district geometry, parent oblasts, and special cities have explicit supported/unsupported mappings per provider.
- Live requests work without keys and follow documented terms and conservative polling.
- Reserve switching and primary recovery are validated and visibly identify the active source.
- Stale, unknown, blocked, and offline states remain visible.
- Wi-Fi setup works without recompiling.
- Tests, map checks, CI, and the recorded stability checks pass.
- Firmware update/recovery and credential handling are documented and tested.
- Source geometry, licensing, provider access, and limitations are documented.

Report evidence in distinct categories:

1. Implemented and tested on hardware with live free APIs.
2. Implemented and tested in the simulator or host tests.
3. Implemented but not yet checked on hardware.
4. Blocked by unverified provider semantics, geometry validation, or hardware.
5. Outside the agreed release scope.

Include exact build/test commands and outcomes, firmware artifact paths, hardware revision, API contract date, map source revision, request settings, and remaining blockers.

At the time this plan was written, no firmware or simulator had been implemented or tested. Documentation research and successful unauthenticated JSON checks of both selected providers were completed on the workstation.

## 17. Reference ledger

Primary sources consulted during planning:

| Source | URL | What was checked |
| --- | --- | --- |
| Tryvoha API documentation | `https://tryvoha.online/api` | Free access, endpoints, red/yellow definitions, limits, caching, attribution |
| Tryvoha source explanation | `https://tryvoha.online/how-it-works` | UkraineAlarm status source and separate processed public-threat stream |
| Tryvoha national alerts | `https://tryvoha.online/api/v1/alerts` | HTTP 200 without authorization, current timestamps and both levels |
| Tryvoha region directory | `https://tryvoha.online/api/v1/regions` | IDs, slugs, parent links, actual directory coverage |
| Tryvoha Kyiv oblast | `https://tryvoha.online/api/v1/alerts/kyivska` | Partial coverage through active_anywhere and districts_active |
| UA Siren | `https://siren.pp.ua/` | Provider page links the UkraineAlarm Swagger and wrapper |
| UA Siren alerts | `https://siren.pp.ua/api/v3/alerts` | HTTP 200 without authorization, AIR records and Red/Yellow level objects |
| Waveshare 7B documentation | `https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7B` | 7-inch 1024 × 600 display, memory, RGB/touch, power and board-specific pins |
| LVGL documentation through Context7 | `https://github.com/lvgl/docs` | Custom drawing and threading guidance; implementation must consult the pinned LVGL version |
| geoBoundaries humanitarian ADM2 metadata | `https://www.geoboundaries.org/api/current/gbHumanitarian/UKR/ADM2/` | 2022 representation, 139 features, source and CC BY 3.0 IGO license; actual geometry still to validate |
| geoBoundaries open ADM2 metadata | `https://www.geoboundaries.org/api/current/gbOpen/UKR/ADM2/` | Rejected candidate: 2006 representation, 495 units |

Recheck provider contracts and terms before implementation. One successful HTTP check establishes availability at that moment, not an uptime guarantee or completed on-device integration.
