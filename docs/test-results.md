# Validation evidence

## Executed on the workstation

- Native C++ model tests: passed, including buzzer startup suppression and alert transition detection.
- Actual LVGL UI contract: passed, including Kyiv startup, national tab,
  independent city state, demo label and stale banner.
- Scheduler contract: passed, including single flight, retained alert on error,
  reserve switching, throttling delay and monotonic freshness.
- TypeScript typecheck, lint and eight map and publication tests: passed.
- Asset generation: 138 display areas, eight home areas, 102732 compressed mask bytes.
- Live Tryvoha request in the desktop simulator: passed; parsed 38 active records
  in that changing snapshot and rendered the home screen.
- ESP-IDF 5.5.2 exact-target firmware build: passed with the buzzer and TLS changes.
- Both 1024 × 600 [demo screenshots](../README.md#screenshots) were captured
  from the compiled simulator and reviewed.

Tests first exposed absent implementations. Later regression tests reproduced
unknown-level precedence and community-to-district mapping defects before the
production fixes; both now pass.

## Executed on the board

- The updated firmware was written to the application partition. The ELF hash
  printed by the board matched the local build.
- UART confirmed startup, Wi-Fi initialization, DHCP and multiple successful
  Tryvoha snapshots. Moving TLS allocations into PSRAM resolved the observed
  certificate verification failure during the recorded checks.
- The first accepted snapshot produced no buzzer event in the UART log.

## Not yet verified

- Audible buzzer output with a connected module and measured GPIO6 pulse timing.
- Physical display and touch interaction, OTA installation and rollback.
- Extended simulator and device soak tests.
