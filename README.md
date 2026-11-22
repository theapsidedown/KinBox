# KinBox

Pokémon x Tamagotchi handheld on the ESP32-S3 (DevKitC N16R8) with Zephyr.

## Build

    west build -b esp32s3_devkitc/esp32s3/procpu KinBox
    west flash

Host-side logic tests (battle engine, moves, types, traits, items, walk drops; no Zephyr needed):

    tests/host/run.sh

Regenerate sprites after changing art in `assets/`:  `python3 tools/gen_assets.py`

## Wiring

| Part | GPIO |
|---|---|
| D-pad Up/Down/Left/Right | 4 / 5 / 6 / 7 (to GND) |
| A / B | 8 / 9 (to GND) |
| **Power key** | **2** (to GND; RTC pin, wakes deep sleep) |
| LCD SCK / MOSI / CS | 12 / 11 / 10 |
| LCD DC / RESET / backlight | 13 / 14 / 15 |
| DS3231 SDA / SCL (optional) | 16 / 17 |

GPIO 26-37 (flash + octal PSRAM) and 19/20 (USB) are avoided on purpose.

## Layout

    src/main.c          event loop, care tick, steps, save timer
    kindrals/           species, moves, traits, types (chart), abilities, items, inventory
    battle/battle.c     deterministic turn engine (same code + seed on both devices)
    net/ble_link.c      BLE discovery + GATT link;  net/social.c  handshake, greet, gift, trade
    net/battle_session.c  snapshot/seed/action exchange for a battle
    storage/            NVS saves, wall clock (DS3231 or temporary)
    actions/            buttons, power key, steps + walk item drops
    ui/                 LVGL scenes (home, menu, keyboard, battle, trade, ...)
    sprites/            generated image data (48x48 kindrals, 32x32 items, 16x16 badges)

## Controls

D-pad moves, **A** selects, **B** goes back / opens the menu (backspace on the keyboard).
Power key: tap = screen on/off, hold 1.5 s = power off, tap while off = power on.
Factory reset: Settings > Factory reset (asks twice), or hold **A+B** while powering on for 3 s.

## Saving (NVS)

Three small records (meta, kindral, inventory) on the `storage_partition`. Routine changes are
written every `KINBOX_SAVE_INTERVAL_S` (300 s) and only if dirty; naming, battle end, trade, gift
and power-off save immediately. Nothing is written until trainer name + nickname are chosen.
NVS rotates through all sectors, so wear is spread out (about 300 writes/day is decades of life).

## Things to check on first build (I could not compile against Zephyr here)

1. `storage_partition` exists in your device tree (see the note at the bottom of the overlay).
2. ST7735 panel settings (`madctl`, offsets, inversion) in the overlay match your module's tab colour.
3. Deep sleep: needs `esp_sleep.h` / `driver/rtc_io.h` from the ESP HAL. If they are missing set
   `CONFIG_KINBOX_DEEP_SLEEP=n` for a "soft off".
4. BLE Zephyr API names (`BT_LE_ADV_OPT_CONNECTABLE`, ...) follow Zephyr 4.x.
5. LVGL 9 API (`lv_image_*`, `lv_screen_load`, `LV_COLOR_FORMAT_RGB565A8`). If colours are
   byte-swapped, check the display's pixel format.
6. `KIN_001.png` in `assets/` is fully transparent, so placeholder art is generated for now.
