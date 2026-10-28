#pragma once

#include "esphome/core/component.h"
#include "lvgl.h"

#include <cstdint>

namespace esphome {
namespace esp_icon_sweep {

/**
 * LVGL icon animation: blink at home, move right with a trail, fade out,
 * return to home at low opacity, then repeat.
 */
class EspIconSweep : public Component {
 public:
  void set_widget(lv_obj_t *widget) { widget_ = widget; }
  void set_sweep(uint32_t sweep_ms) { sweep_ms_ = sweep_ms; }
  void set_animation_width(int32_t value) { animation_width_ = value; }
  void set_start_x(int32_t value) { start_x_ = value; }
  void set_blink_count(uint8_t value) { blink_count_ = value; }
  void set_blink_transition(uint32_t value) { blink_transition_ms_ = value; }
  void set_fade_out_transition(uint32_t value) { fade_out_transition_ms_ = value; }
  void set_fade_in_transition(uint32_t value) { fade_in_transition_ms_ = value; }
  void set_home_opacity(uint8_t value) { home_opacity_ = value; }
  void set_trail_length(int32_t value) { trail_length_ = value; }
  void set_trail_width(int32_t value) { trail_width_ = value; }
  void set_trail_color(uint32_t value) { trail_color_ = value; }

  enum EffectType : uint8_t { EFFECT_SCALE, EFFECT_ROTATE, EFFECT_JUMP, EFFECT_COLOR, EFFECT_ICON };
  struct EffectConfig {
    EffectType type{EFFECT_SCALE};
    uint32_t animation_ms{500};
    uint32_t pause_ms{0};
    int32_t from{100};
    int32_t to{140};
    uint8_t bounces{2};
    bool alternate{false};
    bool started{false};
    uint8_t color_count{0};
    uint8_t color_index{0};
    uint8_t icon_count{0};
    uint8_t icon_index{0};
    const void *icons[8]{};
    uint32_t colors[8]{};
    uint32_t color_from{0xFFFFFF};
    uint32_t color_to{0xFFFFFF};
    uint32_t current_color{0xFFFFFF};
    EspIconSweep *owner{nullptr};
  };
  void add_effect(uint8_t type, uint32_t animation_ms, uint32_t pause_ms,
                  int32_t from, int32_t to, bool alternate, uint8_t bounces,
                  uint8_t color_count, uint32_t color0, uint32_t color1,
                  uint32_t color2, uint32_t color3, uint32_t color4,
                  uint32_t color5, uint32_t color6, uint32_t color7);
  void add_icon(uint8_t effect_index, uint8_t icon_index, const void *icon);

  // Restart immediately from the home position; this preempts any active animation.
  void start();
  // Cancel the current animation and fade the icon and trail out.
  void stop();

  void setup() override;
  void dump_config() override;

 protected:
  static void set_x_cb(void *object, int32_t value);
  static void set_icon_opa_cb(void *object, int32_t value);
  static void set_all_opa_cb(void *object, int32_t value);
  static void set_trails_opa_cb(void *object, int32_t value);
  static void blink_ready_cb(lv_anim_t *anim);
  static void move_ready_cb(lv_anim_t *anim);
  static void fade_out_ready_cb(lv_anim_t *anim);
  static void fade_in_ready_cb(lv_anim_t *anim);

  void start_blink_();
  void start_move_();
  void start_fade_out_();
  void start_fade_in_();
  void update_trail_(int32_t widget_x);
  void set_trails_opa_(lv_opa_t opa);
  void reset_effects_();
  void cancel_animations_();
  void check_effects_();
  void cancel_effect_animations_();
  static void set_scale_cb(void *object, int32_t value);
  static void set_angle_cb(void *object, int32_t value);
  static void set_color_cb(void *object, int32_t value);
  static void set_icon_source_cb(void *object, int32_t value);
  static void set_jump_cb(void *object, int32_t value);

  lv_obj_t *widget_{nullptr};
  lv_obj_t *trails_[2]{nullptr, nullptr};
  lv_grad_dsc_t trail_grad_{};

  int32_t start_x_{0};
  int32_t animation_width_{800};
  int32_t offscreen_x_{-68};
  int32_t trail_length_{240};
  int32_t trail_width_{8};
  uint32_t sweep_ms_{8000};
  uint32_t blink_transition_ms_{300};
  uint32_t fade_out_transition_ms_{1000};
  uint32_t fade_in_transition_ms_{500};
  uint32_t trail_color_{0xFFFFFF};
  uint8_t blink_count_{3};
  uint8_t home_opacity_{25};
  bool setup_complete_{false};
  bool running_{false};
  bool stop_requested_{true};
  static constexpr uint8_t MAX_EFFECTS = 8;
  EffectConfig effects_[MAX_EFFECTS]{};
  uint8_t effect_count_{0};
  int32_t home_y_{0};
  int32_t home_zoom_{256};
  int32_t home_angle_{0};
};

}  // namespace esp_icon_sweep
}  // namespace esphome
