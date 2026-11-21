# API contract

## Primary

`GET https://tryvoha.online/api/v1/alerts`

No registration or Authorization header. Expected root: `updated_at` UTC
timestamp, numeric `active` count, `alerts` array. Count must match the array.
Each record needs `slug`; `level` maps red/yellow, otherwise unknown.
Optional start timestamps remain unavailable when absent.

The build-time directory contains verified parent relationships. An oblast-wide
alert applies to its districts; a district alert does not spread to neighbors.
An active community record marks its parent district partially affected.

Kyiv city is independent. No configuration or API key is editable on screen.

## Reserve

`GET https://siren.pp.ua/api/v3/alerts`

Filter nested `activeAlerts` to `type == "AIR"`. Read `Red`/`Yellow` from
`activeAlertLevels`. Prefer verified provider IDs; preserve source identity.

The current adapter confirms positively matched active records only. Missing
records remain unknown because complete inactive-state semantics have not been
established. Reserve mode cannot silently turn an absent district green.

## Transport and scheduler

- HTTPS certificate verification is mandatory.
- Body limit: 256 KiB.
- Default timeout: 8 seconds.
- Primary poll interval: 60 seconds.
- Maximum four attempts in any rolling minute; one request in flight.
- Two consecutive primary failures schedule the reserve.
- Primary recovery is probed every five minutes; two successes restore it.
- HTTP 429 delays the next request using a numeric Retry-After value when supplied.
- A malformed response leaves the previous validated snapshot intact.
- After 180 seconds without validation, a persistent stale banner is shown.

HTTP receipt freshness does not prove that upstream information has no delay.
Per-record start/update times are not snapshot heartbeats.
The app does not use Tryvoha's drone/event or estimated-trajectory endpoints.
