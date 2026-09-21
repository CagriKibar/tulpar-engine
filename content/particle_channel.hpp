// L6 CONTENT — Niagara Data Channels & Oynanis/Fizik Cift-Yonlu Etkilesim Koprusu.
// Unreal Engine Niagara Data Channels mimarisinin Tulpar Engine uyarlamasi:
// 1. Partikullerden oyun dunyasina (GAS / Health / Damage) veri aktarimi.
// 2. Jolt Physics ile iki yonlu itki (AddImpulse) kuplaji.
// 3. Dinamik Zemin Islanmasi (Ground Wetness Mask) 2B gridi.
// Sifir-tahsis (zero heap allocation) garantisi: Arena uzerinden sabit halka tampon.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/math/vec.hpp"
#include "core/memory/arena.hpp"

namespace tulpar::engine::content {

enum ParticleChannelType : uint8_t {
  kChannelDamage = 0,
  kChannelHeal = 1,
  kChannelPhysicsImpulse = 2,
  kChannelGroundWetness = 3,
};

struct ParticleDataEvent {
  Vec3 pos{};
  Vec3 impulse{};
  float value = 0.0f;
  uint32_t target_id = 0xFFFFFFFFu;
  ParticleChannelType channel = kChannelDamage;
};

class ParticleDataChannelQueue {
 public:
  bool init(Arena &arena, uint32_t capacity) {
    events_ = arena.alloc_array<ParticleDataEvent>(capacity);
    capacity_ = capacity;
    head_ = 0;
    count_ = 0;
    return events_ != nullptr;
  }

  void clear() {
    head_ = 0;
    count_ = 0;
  }

  uint32_t count() const { return count_; }
  uint32_t capacity() const { return capacity_; }

  bool push_damage(Vec3 pos, float damage, uint32_t target_id = 0xFFFFFFFFu) {
    if (count_ >= capacity_) return false;
    uint32_t idx = (head_ + count_) % capacity_;
    events_[idx] = {pos, {}, damage, target_id, kChannelDamage};
    count_++;
    return true;
  }

  bool push_impulse(Vec3 pos, Vec3 impulse, uint32_t body_id = 0xFFFFFFFFu) {
    if (count_ >= capacity_) return false;
    uint32_t idx = (head_ + count_) % capacity_;
    events_[idx] = {pos, impulse, length(impulse), body_id, kChannelPhysicsImpulse};
    count_++;
    return true;
  }

  bool pop(ParticleDataEvent &out_event) {
    if (count_ == 0) return false;
    out_event = events_[head_];
    head_ = (head_ + 1) % capacity_;
    count_--;
    return true;
  }

 private:
  ParticleDataEvent *events_ = nullptr;
  uint32_t capacity_ = 0;
  uint32_t head_ = 0;
  uint32_t count_ = 0;
};

// Dinamik Zemin Islanma Maskesi (Ground Wetness Map)
constexpr uint32_t kWetnessGridRes = 64;

class GroundWetnessMap {
 public:
  bool init(Arena &arena, Vec3 origin, float world_size) {
    origin_ = origin;
    world_size_ = world_size > 0.1f ? world_size : 1.0f;
    inv_cell_size_ = (float)kWetnessGridRes / world_size_;

    cells_ = arena.alloc_array<float>(kWetnessGridRes * kWetnessGridRes);
    if (!cells_) return false;

    clear();
    return true;
  }

  void clear() {
    const uint32_t total = kWetnessGridRes * kWetnessGridRes;
    for (uint32_t i = 0; i < total; i++) {
      cells_[i] = 0.0f;
    }
  }

  // Yagmur/su damlasi zemine vurdugunda islaklik ekler
  void add_splash(Vec3 hit_pos, float radius, float amount) {
    const float gx = (hit_pos.x - origin_.x) * inv_cell_size_;
    const float gz = (hit_pos.z - origin_.z) * inv_cell_size_;
    const float r_cells = radius * inv_cell_size_;

    const int32_t min_x = std::max(0, (int32_t)std::floor(gx - r_cells));
    const int32_t max_x = std::min((int32_t)kWetnessGridRes - 1, (int32_t)std::ceil(gx + r_cells));
    const int32_t min_z = std::max(0, (int32_t)std::floor(gz - r_cells));
    const int32_t max_z = std::min((int32_t)kWetnessGridRes - 1, (int32_t)std::ceil(gz + r_cells));

    const float r_sq = r_cells * r_cells;
    for (int32_t z = min_z; z <= max_z; z++) {
      for (int32_t x = min_x; x <= max_x; x++) {
        const float dx = (float)x - gx;
        const float dz = (float)z - gz;
        const float d_sq = dx * dx + dz * dz;
        if (d_sq <= r_sq) {
          const float falloff = 1.0f - std::sqrt(d_sq) / r_cells;
          const uint32_t idx = z * kWetnessGridRes + x;
          cells_[idx] = std::min(1.0f, cells_[idx] + amount * falloff);
        }
      }
    }
  }

  // Guneste kuruma / buharlasma adimi
  void update(float dt, float drying_rate = 0.05f) {
    const float factor = std::max(0.0f, 1.0f - drying_rate * dt);
    const uint32_t total = kWetnessGridRes * kWetnessGridRes;
    for (uint32_t i = 0; i < total; i++) {
      cells_[i] *= factor;
      if (cells_[i] < 0.001f) cells_[i] = 0.0f;
    }
  }

  // Dunya koordinatinda zemin islakligini ornekler [0..1]
  float sample_wetness(Vec3 pos) const {
    const float gx = (pos.x - origin_.x) * inv_cell_size_;
    const float gz = (pos.z - origin_.z) * inv_cell_size_;

    const int32_t x = (int32_t)std::floor(gx);
    const int32_t z = (int32_t)std::floor(gz);

    if (x < 0 || x >= (int32_t)kWetnessGridRes ||
        z < 0 || z >= (int32_t)kWetnessGridRes) {
      return 0.0f;
    }
    return cells_[z * kWetnessGridRes + x];
  }

 private:
  float *cells_ = nullptr;
  Vec3 origin_{};
  float world_size_ = 100.0f;
  float inv_cell_size_ = 0.64f;
};

} // namespace tulpar::engine::content
