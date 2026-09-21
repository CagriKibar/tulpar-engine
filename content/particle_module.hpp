// L6 CONTENT — Moduler VFX Boru Hatti ve IParticleModule Bilesen Mimarisi.
// Unreal Niagara Modulleri ve Valve Source 2 Operatör Mimarisi Uyarlandi.
// Sifir-tahsis (0 alloc/frame) disiplini: Sabit dizi pipeline ve nesne havuzlama.
#pragma once
#include <algorithm>
#include <cstdint>

#include "content/particles.hpp"
#include "content/particle_channel.hpp"
#include "core/math/noise.hpp"

namespace tulpar::engine::content {

class IParticleModule {
 public:
  virtual ~IParticleModule() = default;
  virtual void on_emitter_spawn(ParticleEmitterConfig &) {}
  virtual void on_particle_spawn(Particle &, const ParticleEmitterConfig &, Rng &) {}
  virtual void on_particle_update(Particle &, float, const ParticleEmitterConfig &) {}
  virtual void on_particle_death(const Particle &, ParticleDataChannelQueue *) {}
};

// 1. Akiskan Turbulansi (Curl Noise) Modulu
class CurlNoiseModule final : public IParticleModule {
 public:
  float strength = 2.0f;
  float frequency = 1.0f;

  void on_particle_update(Particle &p, float dt, const ParticleEmitterConfig &) override {
    if (strength <= 0.0f) return;
    const Vec3 curl = curl_noise_3d(p.pos * frequency, p.seed, 2);
    p.vel += curl * (strength * dt);
  }
};

// 2. Stokes Aerodinamik Surtunme (Drag) Modulu
class StokesDragModule final : public IParticleModule {
 public:
  float drag = 0.2f;

  void on_particle_update(Particle &p, float dt, const ParticleEmitterConfig &) override {
    if (drag <= 0.0f) return;
    const float factor = std::max(0.0f, 1.0f - drag * dt);
    p.vel = p.vel * factor;
  }
};

// 3. Analitik Zemin Carpismasi ve Geri Sekme Modulu
class GroundCollisionModule final : public IParticleModule {
 public:
  float plane_y = 0.0f;
  float restitution = 0.6f;
  float friction = 0.1f;

  void on_particle_update(Particle &p, float, const ParticleEmitterConfig &) override {
    if (p.pos.y <= plane_y && p.vel.y < 0.0f) {
      p.pos.y = plane_y;
      p.vel.y = -p.vel.y * restitution;
      const float lat_factor = std::max(0.0f, 1.0f - friction);
      p.vel.x *= lat_factor;
      p.vel.z *= lat_factor;
    }
  }
};

// 4. Renk Rampasi (Color Gradient) Modulu (Multi-stop)
class ColorRampModule final : public IParticleModule {
 public:
  Vec3 color_start{1.0f, 1.0f, 1.0f};
  Vec3 color_mid{1.0f, 0.5f, 0.1f};
  Vec3 color_end{0.1f, 0.1f, 0.1f};
  float mid_point = 0.3f;

  void on_particle_update(Particle &p, float, const ParticleEmitterConfig &) override {
    const float t = p.lifetime > 0.0001f ? (p.age / p.lifetime) : 1.0f;
    const float ct = std::max(0.0f, std::min(1.0f, t));
    if (ct < mid_point && mid_point > 0.001f) {
      const float sub_t = ct / mid_point;
      p.color_start = color_start * (1.0f - sub_t) + color_mid * sub_t;
    } else {
      const float sub_t = (ct - mid_point) / std::max(0.001f, 1.0f - mid_point);
      p.color_start = color_mid * (1.0f - sub_t) + color_end * sub_t;
    }
  }
};

// 5. Boyut Evrimi (Size Over Life) Modulu
class SizeOverLifeModule final : public IParticleModule {
 public:
  float size_start = 0.2f;
  float size_end = 0.02f;

  void on_particle_update(Particle &p, float, const ParticleEmitterConfig &) override {
    const float t = p.lifetime > 0.0001f ? (p.age / p.lifetime) : 1.0f;
    const float ct = std::max(0.0f, std::min(1.0f, t));
    p.size = size_start + (size_end - size_start) * ct;
  }
};

// 6. Olay ve Alt-Yayici (Niagara Events / Data Channel) Modulu
class DataChannelDeathModule final : public IParticleModule {
 public:
  float damage_radius = 2.0f;
  float damage_amount = 25.0f;
  Vec3 impulse_vector{0.0f, 5.0f, 0.0f};

  void on_particle_death(const Particle &p, ParticleDataChannelQueue *events) override {
    if (!events) return;
    events->push_damage(p.pos, damage_amount, 0xFFFFFFFFu);
    events->push_impulse(p.pos, impulse_vector, 0xFFFFFFFFu);
  }
};

// Sabit-Kapasiteli VfxModulePipeline (0 dinamik tahsis)
class VfxModulePipeline {
 public:
  static constexpr uint32_t kMaxModules = 8;

  bool add_module(IParticleModule *mod) {
    if (count_ >= kMaxModules || !mod) return false;
    modules_[count_++] = mod;
    return true;
  }

  void clear() { count_ = 0; }
  uint32_t count() const { return count_; }

  void execute_emitter_spawn(ParticleEmitterConfig &cfg) {
    for (uint32_t i = 0; i < count_; i++) {
      modules_[i]->on_emitter_spawn(cfg);
    }
  }

  void execute_particle_spawn(Particle &p, const ParticleEmitterConfig &cfg, Rng &rng) {
    for (uint32_t i = 0; i < count_; i++) {
      modules_[i]->on_particle_spawn(p, cfg, rng);
    }
  }

  void execute_particle_update(Particle &p, float dt, const ParticleEmitterConfig &cfg) {
    for (uint32_t i = 0; i < count_; i++) {
      modules_[i]->on_particle_update(p, dt, cfg);
    }
  }

  void execute_particle_death(const Particle &p, ParticleDataChannelQueue *events) {
    for (uint32_t i = 0; i < count_; i++) {
      modules_[i]->on_particle_death(p, events);
    }
  }

 private:
  IParticleModule *modules_[kMaxModules]{};
  uint32_t count_ = 0;
};

} // namespace tulpar::engine::content
