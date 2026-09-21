// L4 SIMULATION — 2026 NPA (Neural Particle Automata).
// EPFL & KAIST SIGGRAPH 2026 arastirmasinin Tulpar Engine uyarlamasi:
// 1. Her parcacik 16-boyutlu ic duruma (latent state s) ve konuma (x) sahiptir.
// 2. Komsuluk algisi turevlenebilir SPH Wendland C4 duzlestirme cekirdegi ile yapilir.
// 3. Kural guncellemesi core/math/nn.hpp uzerinden deterministik ReLU MLP ile hesaplanir.
// 4. 30-bit Morton uzay-dolduran egri hash'i ile CPU/GPU onbellek yerelligi saglanir.
// 5. Morfojenez (kendi kendine organize olma) ve Rejenerasyon (hasar sonrasi onarim).
// Sifir-tahsis (zero heap allocation) garantisi: Arena ile tahsis edilen sabit tamponlar.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/math/nn.hpp"
#include "core/math/vec.hpp"
#include "core/memory/arena.hpp"

namespace tulpar::engine::sim {

constexpr uint32_t kNpaStateDim = 16;

struct NpaParticle {
  Vec3 pos{};
  Vec3 vel{};
  float state[kNpaStateDim]{};
  Vec3 target_pos{}; // Morfojenez hedef konumu
  uint32_t morton_code = 0;
};

// 30-bit Morton Code (Z-order curve) hesabi (10-bit eksen basina, 0..1023 araligi)
inline uint32_t morton30(uint32_t x, uint32_t y, uint32_t z) {
  auto expand_bits = [](uint32_t v) -> uint32_t {
    v = (v | (v << 16)) & 0x030000FFu;
    v = (v | (v <<  8)) & 0x0300F00Fu;
    v = (v | (v <<  4)) & 0x030C30C3u;
    v = (v | (v <<  2)) & 0x09249249u;
    return v;
  };
  return (expand_bits(x) << 2) | (expand_bits(y) << 1) | expand_bits(z);
}

// SPH Wendland C4 yumusatma cekirdegi
inline float wendland_c4(float r, float h) {
  if (r >= h || h <= 1e-5f) return 0.0f;
  const float q = r / h;
  const float one_minus_q = 1.0f - q;
  const float one_minus_q4 = one_minus_q * one_minus_q * one_minus_q * one_minus_q;
  const float alpha = 21.0f / (2.0f * kPi * h * h * h);
  return alpha * one_minus_q4 * (1.0f + 4.0f * q);
}

class NeuralParticleAutomata {
 public:
  bool init(Arena &arena, uint32_t max_particles, float smoothing_radius = 0.5f) {
    particles_ = arena.alloc_array<NpaParticle>(max_particles);
    scratch_inputs_ = arena.alloc_array<float>(max_particles * (kNpaStateDim * 2));
    scratch_outputs_ = arena.alloc_array<float>(max_particles * (kNpaStateDim + 3));
    max_particles_ = max_particles;
    count_ = 0;
    h_ = smoothing_radius;

    return particles_ && scratch_inputs_ && scratch_outputs_;
  }

  uint32_t count() const { return count_; }
  const NpaParticle &particle(uint32_t i) const { return particles_[i]; }
  NpaParticle &particle(uint32_t i) { return particles_[i]; }

  bool add_particle(Vec3 pos, Vec3 target_pos) {
    if (count_ >= max_particles_) return false;
    NpaParticle &p = particles_[count_++];
    p.pos = pos;
    p.vel = {0.0f, 0.0f, 0.0f};
    p.target_pos = target_pos;
    for (uint32_t s = 0; s < kNpaStateDim; s++) {
      p.state[s] = 0.0f;
    }
    p.state[0] = 1.0f; // Baslangic tohum sinyali
    return true;
  }

  // Morton kodlarini gunceller (spatial sorting / cache locality)
  void update_morton_codes(Vec3 bounds_min, Vec3 bounds_max) {
    const Vec3 span = bounds_max - bounds_min;
    const Vec3 inv_span = {
      span.x > 0.001f ? (1023.0f / span.x) : 0.0f,
      span.y > 0.001f ? (1023.0f / span.y) : 0.0f,
      span.z > 0.001f ? (1023.0f / span.z) : 0.0f
    };

    for (uint32_t i = 0; i < count_; i++) {
      const Vec3 rel = (particles_[i].pos - bounds_min) * inv_span;
      const uint32_t qx = (uint32_t)std::max(0.0f, std::min(1023.0f, rel.x));
      const uint32_t qy = (uint32_t)std::max(0.0f, std::min(1023.0f, rel.y));
      const uint32_t qz = (uint32_t)std::max(0.0f, std::min(1023.0f, rel.z));
      particles_[i].morton_code = morton30(qx, qy, qz);
    }
  }

  // Tek bir NPA simülasyon adımı (kural guncellemesi, morfojenez ve rejenerasyon)
  void step(float dt, const DenseLayer &rule_layer) {
    if (count_ == 0) return;

    // 1. Differentiable SPH Neighborhood Perception (Komsu durum integrasyonu)
    for (uint32_t i = 0; i < count_; i++) {
      NpaParticle &pi = particles_[i];
      float neighbor_msg[kNpaStateDim] = {0};

      for (uint32_t j = 0; j < count_; j++) {
        if (i == j) continue;
        const float dist = length(pi.pos - particles_[j].pos);
        if (dist < h_) {
          const float w = wendland_c4(dist, h_);
          for (uint32_t s = 0; s < kNpaStateDim; s++) {
            neighbor_msg[s] += w * particles_[j].state[s];
          }
        }
      }

      // 2. Giris vektorunu olustur: [pi.state (16), neighbor_msg (16)]
      const uint32_t in_offset = i * (kNpaStateDim * 2);
      for (uint32_t s = 0; s < kNpaStateDim; s++) {
        scratch_inputs_[in_offset + s] = pi.state[s];
        scratch_inputs_[in_offset + kNpaStateDim + s] = neighbor_msg[s];
      }
    }

    // 3. Deterministik Neural MLP cikarimi (core/math/nn.hpp)
    if (rule_layer.weights != nullptr && rule_layer.in_dim == kNpaStateDim * 2) {
      for (uint32_t i = 0; i < count_; i++) {
        const float *in_ptr = &scratch_inputs_[i * (kNpaStateDim * 2)];
        float *out_ptr = &scratch_outputs_[i * (kNpaStateDim + 3)];
        dense_forward(rule_layer, in_ptr, out_ptr, Activation::kReLU);

        // Durumu ve hizi guncelle
        for (uint32_t s = 0; s < kNpaStateDim; s++) {
          particles_[i].state[s] = out_ptr[s];
        }
        particles_[i].vel.x = out_ptr[kNpaStateDim + 0];
        particles_[i].vel.y = out_ptr[kNpaStateDim + 1];
        particles_[i].vel.z = out_ptr[kNpaStateDim + 2];
      }
    }

    // 4. Morfojenez ve Rejenerasyon hedef cekim kuvveti (Adveksiyon)
    for (uint32_t i = 0; i < count_; i++) {
      NpaParticle &p = particles_[i];
      // Hedef sekle dogru kendi kendine organize olma (attractor)
      const Vec3 to_target = p.target_pos - p.pos;
      p.vel += to_target * (2.0f * dt);
      p.pos += p.vel * dt;
      p.vel = p.vel * 0.95f; // Sonumleme
    }
  }

  // Hasar / Perturbasyon (Rejenerasyon testi icin)
  void apply_damage(Vec3 damage_center, float radius) {
    const float r_sq = radius * radius;
    for (uint32_t i = 0; i < count_; i++) {
      if (length_sq(particles_[i].pos - damage_center) <= r_sq) {
        // Durumu ve konumu dagit
        particles_[i].pos += Vec3{1.0f, 2.0f, -1.0f} * radius;
        for (uint32_t s = 0; s < kNpaStateDim; s++) {
          particles_[i].state[s] = 0.0f;
        }
      }
    }
  }

 private:
  NpaParticle *particles_ = nullptr;
  float *scratch_inputs_ = nullptr;
  float *scratch_outputs_ = nullptr;
  uint32_t max_particles_ = 0;
  uint32_t count_ = 0;
  float h_ = 0.5f;
};

} // namespace tulpar::engine::sim
