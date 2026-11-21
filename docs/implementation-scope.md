# Implementation scope

The current firmware and simulator cover the following features:

- The target is Waveshare ESP32-S3-Touch-LCD-7B, 1024 x 600.
- Startup opens Kyiv city and Kyiv oblast's seven districts.
- Kyiv city is independent of Kyiv oblast.
- A second tab shows Ukraine's district map.
- Fresh supported districts are red, yellow, or green. Unknown coverage is gray.
- Drone markers, trajectories, and their API are excluded.
- Configuration is supplied in a separate build-time file. There is no configuration screen, Wi-Fi provisioning portal, or runtime configuration editor.
- Firmware and desktop simulator share the actual LVGL interface and model.

- The firmware can drive an optional external buzzer on GPIO6 for new alerts in Kyiv city or Kyiv oblast.

See [validation evidence](test-results.md) for workstation and board checks.
