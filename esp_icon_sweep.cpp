#include "esp_icon_sweep.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp_icon_sweep {

static const char *const TAG = "esp_icon_sweep";

void EspIconSweep::set_x_cb(void *object, int32_t value) {
  auto *self = static_cast<EspIconSweep *>(object);
  if (self == nullptr || self->widget_ == nullptr) return;
  lv_obj_set_x(self->widget_, value);
  self->update_trail_(value);
  self->check_effects_(value);
}

void EspIconSweep::set_scale_cb(void *object, int32_t value) {
  auto *self = static_cast<EspIconSweep *>(object);
  if (self == nullptr || self->widget_ == nullptr) return;
  // LVGL image zoom uses 256 = 100%; zero/negative zoom is invalid.
  if (value < 1) value = 1;
  if (value > 65535) value = 65535;
  lv_img_set_zoom(self->widget_, static_cast<uint16_t>(value));
}

void EspIconSweep::set_angle_cb(void *object, int32_t value) {
  auto *self = static_cast<EspIconSweep *>(object);
  if (self == nullptr || self->widget_ == nullptr) return;
  // LVGL image angle is expressed in tenths of a degree.
  lv_img_set_angle(self->widget_, value);
}

void EspIconSweep::set_jump_cb(void *object, int32_t value) {
  auto *self = static_cast<EspIconSweep *>(object);
  if (self == nullptr || self->widget_ == nullptr) return;
  // Jump only the icon. The trail stays on its normal horizontal path.
  lv_obj_set_y(self->widget_, self->home_y_ - value);
}

void EspIconSweep::set_icon_source_cb(void *object, int32_t value) {
  auto *effect = static_cast<EffectConfig *>(object);
  if (effect == nullptr || effect->owner == nullptr || effect->owner->widget_ == nullptr || effect->icon_count == 0) return;
  // The value equal to icon_count wraps neatly to the first image for endless cycling.
  const uint8_t index = static_cast<uint8_t>(value < 0 ? 0 : value % effect->icon_count);
  effect->icon_index = index;
  const void *src = effect->icons[index];
  if (src != nullptr) lv_img_set_src(effect->owner->widget_, src);
}

void EspIconSweep::set_color_cb(void *object, int32_t value) {
  auto *effect = static_cast<EffectConfig *>(object);
  if (effect == nullptr || effect->owner == nullptr || effect->owner->widget_ == nullptr) return;
  if (value < 0) value = 0;
  if (value > 1000) value = 1000;
  const uint32_t from = effect->color_from;
  const uint32_t to = effect->color_to;
  const int32_t r = static_cast<int32_t>((from >> 16) & 0xFF) +
      (static_cast<int32_t>((to >> 16) & 0xFF) - static_cast<int32_t>((from >> 16) & 0xFF)) * value / 1000;
  const int32_t g = static_cast<int32_t>((from >> 8) & 0xFF) +
      (static_cast<int32_t>((to >> 8) & 0xFF) - static_cast<int32_t>((from >> 8) & 0xFF)) * value / 1000;
  const int32_t b = static_cast<int32_t>(from & 0xFF) +
      (static_cast<int32_t>(to & 0xFF) - static_cast<int32_t>(from & 0xFF)) * value / 1000;
  effect->current_color = (static_cast<uint32_t>(r) << 16) |
      (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
  lv_obj_set_style_img_recolor(effect->owner->widget_, lv_color_hex(effect->current_color), LV_PART_MAIN);
  lv_obj_set_style_img_recolor_opa(effect->owner->widget_, LV_OPA_COVER, LV_PART_MAIN);
}

void EspIconSweep::effect_ready_cb(lv_anim_t *anim) {
  auto *self = static_cast<EspIconSweep *>(anim->var);
  if (self == nullptr) return;
  // The main movement lifecycle resets all effects at the next cycle.
}

void EspIconSweep::add_effect(uint8_t type, uint8_t every_percent,
                              uint32_t transition_ms, int32_t from,
                              int32_t to, int32_t height, bool playback,
                              uint8_t bounces, uint8_t color_count, uint32_t color0, uint32_t color1,
                              uint32_t color2, uint32_t color3, uint32_t color4,
                              uint32_t color5, uint32_t color6, uint32_t color7) {
  if (effect_count_ >= MAX_EFFECTS) {
    ESP_LOGW(TAG, "Effect ignored: maximum of %u effects reached", MAX_EFFECTS);
    return;
  }
  EffectConfig &effect = effects_[effect_count_++];
  effect.type = type <= EFFECT_ICON ? static_cast<EffectType>(type) : EFFECT_SCALE;
  effect.every_percent = every_percent > 100 ? 100 : every_percent;
  effect.transition_ms = transition_ms > 0 ? transition_ms : 1;
  effect.from = from;
  effect.to = to;
  effect.height = height;
  effect.playback = playback;
  effect.bounces = bounces < 1 ? 1 : (bounces > 10 ? 10 : bounces);
  effect.last_trigger_bucket = 0;
  effect.owner = this;
  const uint32_t configured_colors[8] = {color0, color1, color2, color3,
                                        color4, color5, color6, color7};
  effect.color_count = color_count > 8 ? 8 : color_count;
  for (uint8_t i = 0; i < effect.color_count; i++) effect.colors[i] = configured_colors[i] & 0xFFFFFF;
  effect.color_index = 0;
  effect.icon_count = 0;
  effect.icon_index = 0;
  for (auto &icon : effect.icons) icon = nullptr;
  if (effect.color_count > 0) {
    effect.current_color = effect.colors[0];
    effect.color_from = effect.current_color;
    effect.color_to = effect.current_color;
  }
}

void EspIconSweep::add_icon(uint8_t effect_index, uint8_t icon_index, const void *icon) {
  if (effect_index >= effect_count_ || icon == nullptr) return;
  EffectConfig &effect = effects_[effect_index];
  if (effect.type != EFFECT_ICON || icon_index >= 8) return;
  effect.icons[icon_index] = icon;
  if (icon_index >= effect.icon_count) effect.icon_count = icon_index + 1;
}

void EspIconSweep::cancel_animations_() {
  // Lifecycle, scale, rotation and jump animations use this as their var.
  // Color and icon cycling use their EffectConfig as var, so cancel those too.
  lv_anim_delete(this, nullptr);
  for (uint8_t i = 0; i < effect_count_; i++) {
    lv_anim_delete(&effects_[i], nullptr);
  }
}

void EspIconSweep::reset_effects_() {
  // Effects use the component as LVGL's animation variable, so delete each
  // effect callback explicitly without touching the lifecycle animations.
  lv_anim_delete(this, &EspIconSweep::set_scale_cb);
  lv_anim_delete(this, &EspIconSweep::set_angle_cb);
  lv_anim_delete(this, &EspIconSweep::set_jump_cb);
  for (uint8_t i = 0; i < effect_count_; i++) {
    lv_anim_delete(&effects_[i], &EspIconSweep::set_color_cb);
    lv_anim_delete(&effects_[i], &EspIconSweep::set_icon_source_cb);
    effects_[i].last_trigger_bucket = 0;
    effects_[i].continuous_started = false;
    effects_[i].color_index = 0;
    effects_[i].icon_index = 0;
    if (effects_[i].icon_count > 0 && effects_[i].icons[0] != nullptr && widget_ != nullptr) {
      lv_img_set_src(widget_, effects_[i].icons[0]);
    }
    if (effects_[i].color_count > 0) {
      effects_[i].current_color = effects_[i].colors[0];
      effects_[i].color_from = effects_[i].current_color;
      effects_[i].color_to = effects_[i].current_color;
    }
  }
  if (widget_ != nullptr) {
    lv_img_set_zoom(widget_, static_cast<uint16_t>(home_zoom_));
    lv_img_set_angle(widget_, home_angle_);
    lv_obj_set_y(widget_, home_y_);
    // Keep the source image unchanged at home; color effects enable recolor.
    lv_obj_set_style_img_recolor_opa(widget_, LV_OPA_TRANSP, LV_PART_MAIN);
  }
}

void EspIconSweep::check_effects_(int32_t widget_x) {
  if (widget_ == nullptr || animation_width_ <= start_x_) return;
  const int32_t span = animation_width_ - start_x_;
  int32_t progress = ((widget_x - start_x_) * 100) / span;
  if (progress < 0) progress = 0;
  if (progress > 100) progress = 100;
  for (uint8_t i = 0; i < effect_count_; i++) {
    EffectConfig &effect = effects_[i];
    const bool continuous_rotate = effect.every_percent == 0 && effect.type == EFFECT_ROTATE;
    const bool continuous_icon = effect.every_percent == 0 && effect.type == EFFECT_ICON;
    if (effect.every_percent == 0 && !continuous_rotate && !continuous_icon) continue;
    if (continuous_rotate || continuous_icon) {
      if (effect.continuous_started) continue;
      effect.continuous_started = true;
    } else {
      const uint8_t bucket = static_cast<uint8_t>(progress / effect.every_percent);
      if (bucket == 0 || bucket <= effect.last_trigger_bucket) continue;
      effect.last_trigger_bucket = bucket;
    }

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, this);
    lv_anim_set_time(&anim, continuous_icon ? effect.transition_ms * effect.icon_count : effect.transition_ms);
    lv_anim_set_path_cb(&anim, (continuous_rotate || continuous_icon) ? lv_anim_path_linear : lv_anim_path_ease_in_out);
    lv_anim_set_ready_cb(&anim, (continuous_rotate || continuous_icon) ? nullptr : &EspIconSweep::effect_ready_cb);

    switch (effect.type) {
      case EFFECT_SCALE:
        lv_anim_set_exec_cb(&anim, &EspIconSweep::set_scale_cb);
        lv_anim_set_values(&anim, effect.from * 256 / 100, effect.to * 256 / 100);
        if (effect.playback) lv_anim_set_playback_time(&anim, effect.transition_ms);
        break;
      case EFFECT_ROTATE:
        lv_anim_set_exec_cb(&anim, &EspIconSweep::set_angle_cb);
        lv_anim_set_values(&anim, effect.from * 10, effect.to * 10);
        if (continuous_rotate) {
          // A zero interval means one endlessly repeating rotation animation.
          // Keep it linear so a full-turn (e.g. 0 -> 360) looks continuous.
          lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
        } else if (effect.playback) {
          lv_anim_set_playback_time(&anim, effect.transition_ms);
        }
        break;
      case EFFECT_JUMP:
        lv_anim_set_exec_cb(&anim, &EspIconSweep::set_jump_cb);
        lv_anim_set_values(&anim, 0, effect.height);
        if (effect.playback) {
          lv_anim_set_playback_time(&anim, effect.transition_ms);
          // One bounce is the up-and-down playback; additional bounces repeat it.
          lv_anim_set_repeat_count(&anim, static_cast<uint16_t>(effect.bounces));
        }
        break;
      case EFFECT_COLOR: {
        if (effect.color_count < 2) continue;
        // Retarget from the currently displayed color so short intervals remain smooth.
        lv_anim_delete(&effect, &EspIconSweep::set_color_cb);
        lv_anim_set_ready_cb(&anim, nullptr);
        effect.color_from = effect.current_color;
        effect.color_index = static_cast<uint8_t>((effect.color_index + 1) % effect.color_count);
        effect.color_to = effect.colors[effect.color_index];
        lv_anim_set_var(&anim, &effect);
        lv_anim_set_exec_cb(&anim, &EspIconSweep::set_color_cb);
        lv_anim_set_values(&anim, 0, 1000);
        break;
      }
      case EFFECT_ICON: {
        if (effect.icon_count < 2) continue;
        if (continuous_icon) {
          // transition is the interval per image; cycle through all icons forever.
          lv_anim_set_var(&anim, &effect);
          lv_anim_set_exec_cb(&anim, &EspIconSweep::set_icon_source_cb);
          lv_anim_set_values(&anim, 0, effect.icon_count);
          lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
          lv_anim_set_time(&anim, (effect.transition_ms > 0 ? effect.transition_ms : 1) * effect.icon_count);
        } else {
          effect.icon_index = static_cast<uint8_t>((effect.icon_index + 1) % effect.icon_count);
          const void *src = effect.icons[effect.icon_index];
          if (src != nullptr) lv_img_set_src(widget_, src);
          continue;
        }
        break;
      }
    }
    lv_anim_start(&anim);
  }
}

void EspIconSweep::set_icon_opa_cb(void *object, int32_t value) {
  auto *self = static_cast<EspIconSweep *>(object);
  if (self == nullptr || self->widget_ == nullptr) return;
  const lv_opa_t opa = static_cast<lv_opa_t>(value);
  lv_obj_set_style_opa(self->widget_, opa, LV_PART_MAIN);
}

void EspIconSweep::set_all_opa_cb(void *object, int32_t value) {
  auto *self = static_cast<EspIconSweep *>(object);
  if (self == nullptr) return;
  const lv_opa_t opa = static_cast<lv_opa_t>(value);
  if (self->widget_ != nullptr) lv_obj_set_style_opa(self->widget_, opa, LV_PART_MAIN);
  self->set_trails_opa_(opa);
}

void EspIconSweep::set_trails_opa_cb(void *object, int32_t value) {
  auto *self = static_cast<EspIconSweep *>(object);
  if (self == nullptr) return;
  self->set_trails_opa_(static_cast<lv_opa_t>(value));
}

void EspIconSweep::set_trails_opa_(lv_opa_t opa) {
  for (auto *trail : trails_) {
    if (trail != nullptr) lv_obj_set_style_opa(trail, opa, LV_PART_MAIN);
  }
}


void EspIconSweep::update_trail_(int32_t widget_x) {
  if (widget_ == nullptr || trails_[0] == nullptr) return;
  // Anchor the trail to the icon's home Y, not its animated Y, so jump
  // effects move only the icon and never bend or lift the trail.
  const int32_t widget_h = lv_obj_get_height(widget_);
  const int32_t trail_y = home_y_ + (widget_h - trail_width_) / 2;
  // The active trail follows the icon; the second is kept hidden and available.
  lv_obj_set_pos(trails_[0], widget_x - trail_length_, trail_y);
  lv_obj_set_pos(trails_[1], widget_x - trail_length_, trail_y);
}

void EspIconSweep::blink_ready_cb(lv_anim_t *anim) {
  auto *self = static_cast<EspIconSweep *>(anim->var);
  if (self != nullptr && self->running_ && !self->stop_requested_) self->start_move_();
}

void EspIconSweep::move_ready_cb(lv_anim_t *anim) {
  auto *self = static_cast<EspIconSweep *>(anim->var);
  if (self != nullptr && self->running_ && !self->stop_requested_) self->start_fade_out_();
}

void EspIconSweep::fade_out_ready_cb(lv_anim_t *anim) {
  auto *self = static_cast<EspIconSweep *>(anim->var);
  if (self == nullptr || self->widget_ == nullptr) return;

  // Clear effects and restore the icon to its home position.
  self->reset_effects_();
  lv_obj_set_x(self->widget_, self->start_x_);
  lv_obj_set_y(self->widget_, self->home_y_);
  self->update_trail_(self->start_x_);
  self->set_trails_opa_(LV_OPA_TRANSP);

  if (self->stop_requested_) {
    // When stopped, keep the home icon visible at the configured home opacity.
    const lv_opa_t home_opa = static_cast<lv_opa_t>((255U * self->home_opacity_) / 100U);
    lv_obj_set_style_opa(self->widget_, home_opa, LV_PART_MAIN);
    self->running_ = false;
    self->stop_requested_ = false;
    ESP_LOGD(TAG, "Icon Sweep stopped");
    return;
  }

  if (self->running_) self->start_fade_in_();
}

void EspIconSweep::fade_in_ready_cb(lv_anim_t *anim) {
  auto *self = static_cast<EspIconSweep *>(anim->var);
  if (self != nullptr && self->running_ && !self->stop_requested_) self->start_blink_();
}

void EspIconSweep::start_blink_() {
  if (widget_ == nullptr || !running_ || stop_requested_) return;
  if (blink_count_ == 0) {
    start_move_();
    return;
  }
  // A full blink is home-opacity -> full opacity -> home-opacity.
  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, this);
  lv_anim_set_exec_cb(&anim, &EspIconSweep::set_icon_opa_cb);
  lv_anim_set_values(&anim, static_cast<int32_t>((255U * home_opacity_) / 100U), LV_OPA_COVER);
  const uint32_t half = blink_transition_ms_ / 2U;
  lv_anim_set_time(&anim, half > 0 ? half : 1);
  lv_anim_set_playback_time(&anim, half > 0 ? half : 1);
  lv_anim_set_repeat_count(&anim, static_cast<uint16_t>(blink_count_ - 1U));
  lv_anim_set_ready_cb(&anim, &EspIconSweep::blink_ready_cb);
  lv_anim_start(&anim);
}

void EspIconSweep::start_move_() {
  if (widget_ == nullptr || !running_ || stop_requested_) return;
  // Every cycle starts moving from the home position after blinking there.
  reset_effects_();
  const int32_t from_x = start_x_;
  lv_obj_set_x(widget_, from_x);
  lv_obj_set_y(widget_, home_y_);
  update_trail_(from_x);
  set_trails_opa_(LV_OPA_COVER);
  lv_obj_set_style_opa(widget_, LV_OPA_COVER, LV_PART_MAIN);

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, this);
  lv_anim_set_exec_cb(&anim, &EspIconSweep::set_x_cb);
  lv_anim_set_values(&anim, from_x, animation_width_);
  lv_anim_set_time(&anim, transition_ms_ > 0 ? transition_ms_ : 1);
  lv_anim_set_path_cb(&anim, lv_anim_path_linear);
  lv_anim_set_ready_cb(&anim, &EspIconSweep::move_ready_cb);
  lv_anim_start(&anim);
}

void EspIconSweep::start_fade_out_() {
  if (widget_ == nullptr || !running_) return;

  if (stop_requested_) {
    // On STOP, fade icon and trail from their own current opacity. This avoids
    // making a hidden trail flash on when STOP arrives during the home blink.
    lv_anim_t icon_anim;
    lv_anim_init(&icon_anim);
    lv_anim_set_var(&icon_anim, this);
    lv_anim_set_exec_cb(&icon_anim, &EspIconSweep::set_icon_opa_cb);
    const lv_opa_t home_opa = static_cast<lv_opa_t>((255U * home_opacity_) / 100U);
    lv_anim_set_values(&icon_anim, lv_obj_get_style_opa(widget_, LV_PART_MAIN), LV_OPA_TRANSP); //home_opa);
    lv_anim_set_time(&icon_anim, fade_out_transition_ms_ > 0 ? fade_out_transition_ms_ : 1);
    lv_anim_set_ready_cb(&icon_anim, &EspIconSweep::fade_out_ready_cb);
    lv_anim_start(&icon_anim);

    if (trails_[0] != nullptr) {
      lv_anim_t trail_anim;
      lv_anim_init(&trail_anim);
      lv_anim_set_var(&trail_anim, this);
      lv_anim_set_exec_cb(&trail_anim, &EspIconSweep::set_trails_opa_cb);
      lv_anim_set_values(&trail_anim, lv_obj_get_style_opa(trails_[0], LV_PART_MAIN), LV_OPA_TRANSP);
      lv_anim_set_time(&trail_anim, fade_out_transition_ms_ > 0 ? fade_out_transition_ms_ : 1);
      lv_anim_start(&trail_anim);
    }
    return;
  }

  // Normal cycle: icon and trail are both fully visible before this fade.
  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, this);
  lv_anim_set_exec_cb(&anim, &EspIconSweep::set_all_opa_cb);
  lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
  lv_anim_set_time(&anim, fade_out_transition_ms_ > 0 ? fade_out_transition_ms_ : 1);
  lv_anim_set_ready_cb(&anim, &EspIconSweep::fade_out_ready_cb);
  lv_anim_start(&anim);
}

void EspIconSweep::start_fade_in_() {
  if (widget_ == nullptr || !running_ || stop_requested_) return;
  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, this);
  lv_anim_set_exec_cb(&anim, &EspIconSweep::set_icon_opa_cb);
  lv_anim_set_values(&anim, LV_OPA_TRANSP, static_cast<int32_t>((255U * home_opacity_) / 100U));
  lv_anim_set_time(&anim, fade_in_transition_ms_ > 0 ? fade_in_transition_ms_ : 1);
  lv_anim_set_ready_cb(&anim, &EspIconSweep::fade_in_ready_cb);
  lv_anim_start(&anim);
}

void EspIconSweep::start() {
  if (!setup_complete_ || widget_ == nullptr) {
    ESP_LOGW(TAG, "START ignored: Icon Sweep is not initialized yet");
    return;
  }

  // START always wins: cancel movement, effects, and any in-progress stop fade.
  cancel_animations_();
  stop_requested_ = false;
  running_ = true;
  reset_effects_();
  lv_obj_set_x(widget_, start_x_);
  lv_obj_set_y(widget_, home_y_);
  lv_obj_set_style_opa(widget_, static_cast<lv_opa_t>((255U * home_opacity_) / 100U), LV_PART_MAIN);
  update_trail_(start_x_);
  set_trails_opa_(LV_OPA_TRANSP);
  start_blink_();
  ESP_LOGI(TAG, "Icon Sweep START");
}

void EspIconSweep::stop() {
  if (!setup_complete_ || widget_ == nullptr || (!running_ && !stop_requested_)) return;
  // Repeated STOP requests do not restart the fade. A later START preempts it.
  if (stop_requested_) return;
  stop_requested_ = true;
  cancel_animations_();
  start_fade_out_();
  ESP_LOGI(TAG, "Icon Sweep STOP requested");
}

void EspIconSweep::setup() {
  if (widget_ == nullptr) {
    ESP_LOGE(TAG, "No LVGL widget configured; animation disabled");
    mark_failed();
    return;
  }
  lv_obj_t *parent = lv_obj_get_parent(widget_);
  if (parent == nullptr) {
    ESP_LOGE(TAG, "Animation widget has no parent; animation disabled");
    mark_failed();
    return;
  }

  lv_obj_update_layout(widget_);
  const int32_t widget_width = lv_obj_get_width(widget_);
  if (widget_width <= 0 || animation_width_ <= start_x_) {
    ESP_LOGE(TAG, "Invalid animation dimensions: widget=%ld start_x=%ld animation_width=%ld",
             static_cast<long>(widget_width), static_cast<long>(start_x_),
             static_cast<long>(animation_width_));
    mark_failed();
    return;
  }
  offscreen_x_ = -widget_width;
  home_y_ = lv_obj_get_y(widget_);
  home_zoom_ = 256;
  home_angle_ = 0;
  // Keep scale and rotation centered on the icon.
  lv_img_set_pivot(widget_, widget_width / 2, lv_obj_get_height(widget_) / 2);

  trail_grad_ = {};
  trail_grad_.dir = LV_GRAD_DIR_HOR;
  trail_grad_.stops_count = 2;
  trail_grad_.stops[0].color = lv_color_hex(trail_color_);
  trail_grad_.stops[0].opa = LV_OPA_TRANSP;
  trail_grad_.stops[0].frac = 0;
  trail_grad_.stops[1].color = lv_color_hex(trail_color_);
  trail_grad_.stops[1].opa = LV_OPA_COVER;
  trail_grad_.stops[1].frac = 255;

  for (uint8_t i = 0; i < 2; i++) {
    lv_obj_t *trail = lv_obj_create(parent);
    trails_[i] = trail;
    lv_obj_remove_style_all(trail);
    lv_obj_set_size(trail, trail_length_, trail_width_);
    lv_obj_set_style_border_width(trail, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(trail, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(trail, lv_color_hex(trail_color_), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(trail, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(trail, &trail_grad_, LV_PART_MAIN);
    lv_obj_set_style_opa(trail, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(trail, LV_OBJ_FLAG_CLICKABLE);
  }

  lv_obj_set_x(widget_, start_x_);
  lv_obj_set_style_opa(widget_, static_cast<lv_opa_t>((255U * home_opacity_) / 100U), LV_PART_MAIN);
  update_trail_(start_x_);
  set_trails_opa_(LV_OPA_TRANSP);
  lv_obj_move_foreground(widget_);

  reset_effects_();
  setup_complete_ = true;
  ESP_LOGI(TAG, "Icon Sweep configured: start_x=%ld width=%ld transition=%u ms blink=%u x %u ms home_opacity=%u%% effects=%u",
           static_cast<long>(start_x_), static_cast<long>(animation_width_),
           static_cast<unsigned>(transition_ms_), static_cast<unsigned>(blink_count_),
           static_cast<unsigned>(blink_transition_ms_), static_cast<unsigned>(home_opacity_),
           static_cast<unsigned>(effect_count_));
}

void EspIconSweep::dump_config() {
  ESP_LOGCONFIG(TAG, "Icon Sweep:");
  ESP_LOGCONFIG(TAG, "  Start/home X: %ld", static_cast<long>(start_x_));
  ESP_LOGCONFIG(TAG, "  Animation width: %ld", static_cast<long>(animation_width_));
  ESP_LOGCONFIG(TAG, "  Transition: %u ms", static_cast<unsigned>(transition_ms_));
  ESP_LOGCONFIG(TAG, "  Blink: %u cycles, %u ms each", static_cast<unsigned>(blink_count_),
                static_cast<unsigned>(blink_transition_ms_));
  ESP_LOGCONFIG(TAG, "  Fade out/in: %u/%u ms", static_cast<unsigned>(fade_out_transition_ms_),
                static_cast<unsigned>(fade_in_transition_ms_));
  ESP_LOGCONFIG(TAG, "  Home opacity: %u%%", static_cast<unsigned>(home_opacity_));
  ESP_LOGCONFIG(TAG, "  Trail: %ld x %ld, color=0x%06lX", static_cast<long>(trail_length_),
                static_cast<long>(trail_width_), static_cast<unsigned long>(trail_color_));
  ESP_LOGCONFIG(TAG, "  Started: %s", running_ ? "yes" : "no");
}

}  // namespace esp_icon_sweep
}  // namespace esphome
