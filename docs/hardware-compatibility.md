# Hardware compatibility

Target: Waveshare ESP32-S3-Touch-LCD-7B, 1024 x 600, RGB565, ESP32-S3,
16 MB Flash, 8 MB octal PSRAM, GT911 touch.

Board reference: Waveshare's official `17_lvgl_v9_demo` at revision
`c652c902db607f7ffb376257393cfd7657aa6428`.

The implementation uses the 7B-specific RGB data pins, 24 MHz pixel clock and
documented porch timing. Flash and PSRAM run at 80 MHz; experimental 120 MHz
settings from some vendor examples are intentionally not used.

I2C: GPIO8/GPIO9, expander address 0x24. Touch IRQ: GPIO4. The expander
provides touch reset, LCD power/backlight, PWM brightness, and USB/CAN selection.
UART and native USB are different physical connections.

## External buzzer

Use an active 3.3 V buzzer module with an active-high logic input. On connector
J8, connect GP6 to the module input, 3V3 to VCC and GND to GND. GPIO6 is a
control signal. The module draws power from VCC. The firmware pulses GPIO6
high for 180 ms when Kyiv city or a district in Kyiv oblast goes from clear to
an alert.

## UART connection

For the Type-C USB TO UART connector, select **UART1**. The manufacturer documents
UART1 as the path through the CH343 bridge and UART2 as the external UART
connector. The schematic identifies the routing control as `SW1` / `UART_SEL`.

Schematic:
`https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-7B/ESP32-S3-Touch-LCD-7B-Schematic.pdf`

For the manufacturer's cold-start download procedure, completely remove power,
hold BOOT, reconnect USB TO UART, then release BOOT after power is restored.
When already powered, hold BOOT while pressing/releasing RESET.
Use esptool's `--before no_reset` when probing a manually entered bootloader.

The consulted [schematic PDF](pdf/ESP32-S3-Touch-LCD-7B-Schematic.pdf) is saved in
the repository. The current setup guide is
`https://docs.waveshare.com/ESP32-S3-Touch-LCD-7B`.

## Verification boundary

ESP-IDF 5.5.2 built the firmware for the exact board. The application was
flashed via USB-to-UART. A 115200-baud UART capture confirmed the matching
application image, Wi-Fi association, DHCP and successful Tryvoha HTTPS polls.

The simulator validates the shared LVGL UI. See [validation evidence](test-results.md)
for the checks performed and the remaining hardware verification.
