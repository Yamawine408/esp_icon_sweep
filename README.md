# ESP Icon Sweep

**A configurable icon animation component for ESPHome and LVGL.**

ESP Icon Sweep adds animated icon effects to ESPHome-based displays using LVGL. Icons move horizontally across the screen, with optional visual effects such as trails, blinking, scaling, jumping, and rotation.

It is designed as an independent component that can be used alongside ESPHome dashboards and other custom components.

## Features

- **Horizontal sweep animation** — move icons across the display.
- **Trail effects** — create a fading trail behind moving icons.
- **Configurable animation** — customize the starting position, icon width, and animation duration.
- **Optional effects** — enable blinking, scaling, jumping, and rotation.
- **YAML-based configuration** — configure animations through ESPHome.
- **Multiple icons** — animate different icons independently.
- **LVGL integration** — designed for ESPHome displays using LVGL.

## Requirements

- ESPHome
- An ESPHome-compatible display
- LVGL
- PNG icons or other image assets supported by the configured LVGL image widgets

This component was developed and tested on an ESP32-P4 board with an 800 × 1280 MIPI-DSI display and LVGL 9.5.0. Other configurations may require additional testing.

## Installation

Add the component to your ESPHome YAML configuration:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/Yamawine408/esp_icon_sweep
    components: [esp_icon_sweep]
```

> **Note:** The configuration above assumes the component source files are located at the root of the `esp_icon_sweep` repository. Adjust the source configuration if the repository layout changes.

## Configuration

ESP Icon Sweep is configured through ESPHome YAML.

The animation settings are intended to let you customize properties such as:

- `start_x` — horizontal starting position.
- `width` — icon width.
- `duration` — animation duration.
- `blink` — blinking effect.
- `scale` — icon scaling effect.
- `jump` — vertical jumping effect.
- `rotation` — icon rotation.

The exact option names, supported values, and defaults depend on the component's current implementation. Refer to the component configuration schema and example YAML for the definitive list.

## How It Works

ESP Icon Sweep separates the moving icon from its trail effects.

The icon can move and apply optional transformations while the trail remains independent. For example, an icon can jump vertically while its trail continues along a horizontal path.

This separation allows animation effects to be combined without requiring the trail to reproduce every movement of the icon.

## Example Use Cases

- Animated energy or power icons
- CO₂ and air-quality indicators
- Door-lock status animations
- Toilet or bathroom activity indicators
- Decorative animations for smart-home dashboards

## License

This project is distributed under the license included in the repository. See [LICENSE](LICENSE) for details.

## Contributing

Issues, suggestions, and improvements are welcome. Please include your ESPHome version, LVGL version, display configuration, and relevant logs when reporting problems.

