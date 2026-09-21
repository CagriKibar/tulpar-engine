// L6 CONTENT — Kalici Enkaz Birikimi (Nanite Tarzi Persistent World Baking & RVT Splatting).
// 2026 Dunya ve Malzeme Mimarisi:
// Yere dusup duran yapraklar, kul, moloz ve mermi kovanlari dinamik simulasyon havuzundan
// cikarilarak calisma zamani sanal dokusuna (RVT) veya arazi yukseklik haritasina tek
// seferlik damgalanir (bake edilir). Simulasyon yuku 0'a duserken enkaz kalici olur.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/math/vec.hpp"
#include "core/memory/arena.hpp"

namespace tulpar::engine::content {

struct RvtDebrisStamp {
  Vec3 pos{};
  float radius = 0.2f;
  Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
  float thickness = 0.02f;
};

class PersistentDebrisBaker {
 public:
  bool init(Arena &arena, uint32_t res_x, uint32_t res_y, Vec3 origin, float world_size) {
    res_x_ = res_x;
    res_y_ = res_y;
    origin_ = origin;
    world_size_ = world_size > 0.1f ? world_size : 1.0f;
    inv_world_size_ = (float)res_x / world_size_;

    const uint32_t total = res_x * res_y;
    debris_color_ = arena.alloc_array<Vec4>(total);
    debris_height_ = arena.alloc_array<float>(total);

    if (!debris_color_ || !debris_height_) return false;

    clear();
    return true;
  }

  void clear() {
    const uint32_t total = res_x_ * res_y_;
    for (uint32_t i = 0; i < total; i++) {
      debris_color_[i] = {0.0f, 0.0f, 0.0f, 0.0f};
      debris_height_[i] = 0.0f;
    }
  }

  // Duran parcacigi RVT enkaz tamponuna damgalar (fircalar)
  void bake_particle(Vec3 pos, float radius, Vec4 color, float thickness) {
    const float gx = (pos.x - origin_.x) * inv_world_size_;
    const float gy = (pos.z - origin_.z) * inv_world_size_;
    const float r_pix = radius * inv_world_size_;

    const int32_t min_x = std::max(0, (int32_t)std::floor(gx - r_pix));
    const int32_t max_x = std::min((int32_t)res_x_ - 1, (int32_t)std::ceil(gx + r_pix));
    const int32_t min_y = std::max(0, (int32_t)std::floor(gy - r_pix));
    const int32_t max_y = std::min((int32_t)res_y_ - 1, (int32_t)std::ceil(gy + r_pix));

    const float r_sq = r_pix * r_pix;
    for (int32_t y = min_y; y <= max_y; y++) {
      for (int32_t x = min_x; x <= max_x; x++) {
        const float dx = (float)x - gx;
        const float dy = (float)y - gy;
        const float d_sq = dx * dx + dy * dy;
        if (d_sq <= r_sq) {
          const float falloff = 1.0f - std::sqrt(d_sq) / r_pix;
          const uint32_t idx = y * res_x_ + x;

          // Yukseklik biriktirme
          debris_height_[idx] += thickness * falloff;

          // Renk harmanlama (alfa agirlikli)
          const float blend = color.w * falloff;
          debris_color_[idx] = debris_color_[idx] + (color - debris_color_[idx]) * blend;
        }
      }
    }
  }

  float sample_thickness(Vec3 world_pos) const {
    const float gx = (world_pos.x - origin_.x) * inv_world_size_;
    const float gy = (world_pos.z - origin_.z) * inv_world_size_;
    const int32_t x = (int32_t)std::floor(gx);
    const int32_t y = (int32_t)std::floor(gy);

    if (x < 0 || x >= (int32_t)res_x_ || y < 0 || y >= (int32_t)res_y_) {
      return 0.0f;
    }
    return debris_height_[y * res_x_ + x];
  }

  Vec4 sample_color(Vec3 world_pos) const {
    const float gx = (world_pos.x - origin_.x) * inv_world_size_;
    const float gy = (world_pos.z - origin_.z) * inv_world_size_;
    const int32_t x = (int32_t)std::floor(gx);
    const int32_t y = (int32_t)std::floor(gy);

    if (x < 0 || x >= (int32_t)res_x_ || y < 0 || y >= (int32_t)res_y_) {
      return {0.0f, 0.0f, 0.0f, 0.0f};
    }
    return debris_color_[y * res_x_ + x];
  }

 private:
  Vec4 *debris_color_ = nullptr;
  float *debris_height_ = nullptr;
  uint32_t res_x_ = 0;
  uint32_t res_y_ = 0;
  Vec3 origin_{};
  float world_size_ = 100.0f;
  float inv_world_size_ = 1.0f;
};

} // namespace tulpar::engine::content
