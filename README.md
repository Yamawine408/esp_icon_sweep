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

## How It Works

ESP Icon Sweep separates the moving icon from its trail effects.

The icon can move and apply optional transformations while the trail
remains independent. For example, an icon can jump vertically while
its trail continues along a horizontal path.  

This separation allows animation effects to be combined without
requiring the trail to reproduce every movement of the icon. 

## Example

```yaml
  - id: icon_sweep_animation
    widget: icon_sweep_widget
    
    start_x: 40
    animation_width: 800
    home_opacity: 40
    blink_count: 4
    blink_transition: 300ms
    fade_out_transition: 1s
    
    transition: 30s
    trail_length: 150
    trail_width: 2
    trail_color: 0xFFFFFF
    effects:
      - type: scale
        every: 10
        transition: 100ms
        from: 100 
        to: 120
        playback: true
```

In this example, the icon jumps when it reaches the positions
specified by the `every` number which is the percentage of the `animation_width`.
The animation settings are intended to let you customize properties such as:

- `widget` - LVGL widget ID to animate
- `start_x` —Horizontal starting position of the animation
- `animation_width` - Rightmost X-coordinate reached by the icon's left edge
- `home_opacity` - Icon opacity at the home position, as a percentage (0–100)
- `blink_count` - Number of blinks at the home position before the icon starts moving
- `blink_transition` - Duration of one blink cycle
- `fade_out_transition` - Duration of the fade-out animation when stopping.
- `transision` - Duration of one complete sweep animation
- `trail_length` - Length of the trail behind the icon (pixels)
- `trail_width` - (Vertical) Width of trail (pixels)
- `effects` - List of animation effects to apply while the icon moves

![ESP Icon Sweep animation](dcos/esp-icon-sweep.png)

## Effects

Multiple effects can be combined, but only one `icon` effect and one
`color` effect are allowed. 

### Common parameters

| Name | Type | Default | Description |
| ---- | :--: | :-----: | ----------- |
| `type` | string | (none) | effect type (`color`\|`icon`\|`jump`\|`rotate`\|`scale`) |
| `every` | integer | (none) | every position in percentage of `animation_width` to start animation |
| `playback` | boolean | (none) | if `true`, then the animation takes place only once |

### `color`

Change icon color at `every` position.

```yaml
    effects:
      - type: color
        every: 10
        colors:
          - color: 0xFF0000
          - color: 0x00FF00
          - color: 0x0000FF
```

### `icon`

Alternate icons.

```yaml
    effects:
      - type: icon
        every: 10
        icons:
          - image: icon1
          - image: icon2
```

### `jump`

Jump icon 2 times at `every` position with the jump height of 5 pixels.

```yaml
    effects:
      - type: jump
        every: 10
        transition: 200ms
        height: 5
        bounces: 2
        playback: true
```

### `rotation`

Rotating the icon without stopping the rotation. Its rotation speed is
1 round per second in this example.

```yaml
    effects:
      - type: rotate
        every: 0
        transition: 1s
        playback: true
        from: 0
        to: 360
    ```

### `scale`

In this example, the icon scales from 100% to 120% of its original
size when it reaches 10% of the animation path. The scaling animation
then returns to its original size because playback is set to true. 

```yaml
    effects:
      - type: scale
        every: 10
        transition: 3s
        from: 90
        to: 180
        playback: true
```

## License

This project is distributed under the license included in the
repository. See [LICENSE](LICENSE) for details. 

## Contributing

Issues, suggestions, and improvements are welcome. Please include your
ESPHome version, LVGL version, display configuration, and relevant
logs when reporting problems. 

