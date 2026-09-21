// L6 CONTENT — CPU tarafli, sabit-kapasiteli parcacik (particle) sistemi.
// 500 madde listesinden BAGIMSIZ bir "gorsel" eklenti (kullanicinin acikca
// istedigi yon: render/materyal/parcacik gibi GORSEL katmanlar).
//
// **Rakiplerden farki (devrimsel degil, ama GERCEK ve OLCULEBILIR bir
// avantaj):** dogum-ani rastgelelik (hiz jitter'i, omur) core/math/random.hpp
// icindeki DETERMINISTIK Rng (xorshift32) ile uretilir -- gercek-zaman
// tohumlu (ör. std::random_device, sistem saati) TIPIK parcacik
// sistemlerinin aksine, AYNI tohum + AYNI emit() sirasi HER PLATFORMDA
// (x86_64/ARM) BIT-ES AYNI parcacik dizisini uretir. Bu, sim/replay.hpp
// (deterministik kayit/oynatma) ve sim/rollback.hpp (rollback netcode) ile
// parcacik efektlerinin de (patlama, iz, kivilcim) EK KOD OLMADAN uyumlu
// calismasi anlamina gelir -- coğu motorda VFX rollback/replay disinda
// tutulur (gorsel sacmalamasin diye elle senkronize edilir), Tulpar'da
// BEDAVA gelir.
//
// Bellek: init() SONRASI ayirma YOK (A2 "0 tahsis" kapisiyla ayni cizgi).
// Olu parcacik temizligi TAKAS-ILE-SILME (swap-with-last) O(1): dizi
// HER ZAMAN yogun (dense) tutulur, index bosluklari OLMAZ.
#pragma once
#include <algorithm>
#include <cstdint>

#include "core/math/noise.hpp"
#include "core/math/random.hpp"
#include "core/math/vec.hpp"
#include "core/memory/arena.hpp"

namespace tulpar::engine::content {

struct Particle {
  Vec3 pos{};
  Vec3 vel{};
  float age = 0.0f;
  float lifetime = 1.0f;
  float size_start = 1.0f;
  float size_end = 1.0f;
  float size = 1.0f; // guncel boyut -- update() tarafindan omur-oranindan hesaplanir
  Vec3 color_start{1.0f, 1.0f, 1.0f};
  Vec3 color_end{1.0f, 1.0f, 1.0f};
  Vec3 gravity{0.0f, 0.0f, 0.0f};
  bool has_custom_gravity = false;

  // 2026 Gelismis Fizik ve Turbulans
  float curl_noise_strength = 0.0f;
  float curl_noise_frequency = 1.0f;
  float drag = 0.0f; // Stokes surtunme katsayisi
  bool enable_collision = false;
  float collision_plane_y = 0.0f;
  float restitution = 0.6f; // carpisma esneklik katsayisi (sektirme)
  float friction = 0.1f;    // yanal surtunme
  uint32_t seed = 0;        // deterministik curl noise tohumu
  float subuv_frame = 0.0f; // 0..1 arasinda flipbook animasyon ilerlemesi
  uint32_t shape = 0;       // 0: Screen Quad, 1: Stretched Streak, 2: Horizontal, 3: Sphere, 4: Cube

  // Alt Yayici & Olay Zinciri (Event Chains)
  uint32_t spawn_on_death_count = 0;
  uint32_t spawn_on_collision_count = 0;
  class ParticleSystem *sub_emitter = nullptr;
  struct ParticleEmitterConfig *sub_emitter_cfg = nullptr;
};

enum class ParticleEmissionShape : uint8_t {
  Point = 0,             // Tek bir merkez noktadan cikis
  LinearBeam = 1,        // Tek eksende hizli dogrusal isin
  PlanarRing = 2,        // XZ duzleminde dairesel halka / disk
  VerticalCurtain = 3,   // XY duzleminde dikey perde / duvar
  ConicalFountain = 4,   // Yukari yonlu acisal konik huni
  SphericalVolume = 5,   // 3B esit kuresel hacim
};

enum class ParticleSimulationMode : uint8_t {
  Auto = 0,              // Parcacik sayisi ve carpismaya gore otomatik CPU/GPU
  Cpu = 1,               // Zorunlu CPU SIMD Arenasi (fizik/ribbon uyumlu)
  Gpu = 2,               // Zorunlu Vulkan Compute Pipeline
};

struct ParticleEmitterConfig {
  Vec3 spawn_pos{};
  Vec3 base_velocity{};
  Vec3 velocity_jitter{}; // her eksen [-jitter, +jitter) araliginda BAGIMSIZ ekleme
  float lifetime_min = 1.0f;
  float lifetime_max = 1.0f; // == lifetime_min ise omur SABIT (rastgelelik yok)
  float size_start = 1.0f;
  float size_end = 1.0f;
  Vec3 color_start{1.0f, 1.0f, 1.0f};
  Vec3 color_end{1.0f, 1.0f, 1.0f};
  Vec3 gravity{0.0f, 0.0f, 0.0f};
  bool custom_gravity = false;

  // 2026 Gelismis Fizik & Animasyon Yapilandirmasi
  float curl_noise_strength = 0.0f;
  float curl_noise_frequency = 1.0f;
  float drag = 0.0f;
  bool enable_collision = false;
  float collision_plane_y = 0.0f;
  float restitution = 0.6f;
  float friction = 0.1f;
  uint32_t seed = 1337u;
  uint32_t flipbook_cols = 1;
  uint32_t flipbook_rows = 1;
  uint32_t shape = 0; // 0: Screen Quad, 1: Stretched Streak, 2: Horizontal, 3: Sphere, 4: Cube

  // 2026 Gelismis Geometri ve Yayilim Bicimi (Godot GPUParticles Standardi)
  ParticleEmissionShape emission_shape = ParticleEmissionShape::Point;
  float emission_radius = 1.0f;
  float emission_inner_radius = 0.0f; // PlanarRing ic yaricap
  float emission_spread = 0.5f;       // Conical angle / curtain genislik
  bool affects_gameplay = false;
  ParticleSimulationMode simulation_mode = ParticleSimulationMode::Auto;

  // Alt Yayici Yapilandirmasi
  uint32_t spawn_on_death_count = 0;
  uint32_t spawn_on_collision_count = 0;
  class ParticleSystem *sub_emitter = nullptr;
  ParticleEmitterConfig *sub_emitter_cfg = nullptr;
};

inline ParticleSimulationMode resolve_simulation_mode(const ParticleEmitterConfig &cfg, uint32_t active_count) {
  if (cfg.simulation_mode != ParticleSimulationMode::Auto) return cfg.simulation_mode;
  if (cfg.enable_collision || active_count < 2000) return ParticleSimulationMode::Cpu;
  return ParticleSimulationMode::Gpu;
}

class ParticleSystem {
 public:
  bool init(Arena &arena, uint32_t max_particles, Vec3 gravity) {
    particles_ = arena.alloc_array<Particle>(max_particles);
    capacity_ = max_particles;
    count_ = 0;
    gravity_ = gravity;
    return particles_ != nullptr;
  }

  void set_gravity(Vec3 g) { gravity_ = g; }
  void clear() { count_ = 0; }
  uint32_t capacity() const { return capacity_; }

  // count kadar YENI parcacik dogurmaya CALISIR; havuzda yer kalmazsa
  // SESSIZCE KESILMEZ -- GERCEKTEN dogan sayi doner (cagiran kontrol eder,
  // core/jobs/job_graph.hpp / sim/replay.hpp'deki AYNI "sessiz basarisizlik
  // YOK" disiplini).
  uint32_t emit(const ParticleEmitterConfig &cfg, uint32_t count, Rng &rng) {
    uint32_t spawned = 0;
    for (uint32_t i = 0; i < count; i++) {
      if (count_ >= capacity_) break;
      Particle &p = particles_[count_++];

      Vec3 shape_offset{0.0f, 0.0f, 0.0f};
      Vec3 shape_vel_add{0.0f, 0.0f, 0.0f};

      switch (cfg.emission_shape) {
        case ParticleEmissionShape::Point:
          break;
        case ParticleEmissionShape::LinearBeam: {
          const float t_beam = rng.next_float() * 2.0f - 1.0f;
          shape_offset = Vec3{t_beam * cfg.emission_radius, 0.0f, 0.0f};
          break;
        }
        case ParticleEmissionShape::PlanarRing: {
          const float theta = rng.next_float() * 6.28318530718f;
          const float r = cfg.emission_inner_radius +
                          rng.next_float() * (cfg.emission_radius - cfg.emission_inner_radius);
          shape_offset = Vec3{r * std::cos(theta), 0.0f, r * std::sin(theta)};
          break;
        }
        case ParticleEmissionShape::VerticalCurtain: {
          const float cx = (rng.next_float() * 2.0f - 1.0f) * cfg.emission_radius;
          const float cy = (rng.next_float() * 2.0f - 1.0f) * cfg.emission_spread;
          shape_offset = Vec3{cx, cy, 0.0f};
          break;
        }
        case ParticleEmissionShape::ConicalFountain: {
          const float phi = rng.next_float() * 6.28318530718f;
          const float angle = rng.next_float() * cfg.emission_spread;
          const float sin_a = std::sin(angle);
          const float cos_a = std::cos(angle);
          shape_vel_add = Vec3{sin_a * std::cos(phi), cos_a - 1.0f, sin_a * std::sin(phi)} * 2.0f;
          break;
        }
        case ParticleEmissionShape::SphericalVolume: {
          const float u = rng.next_float();
          const float v = rng.next_float();
          const float theta = u * 6.28318530718f;
          const float phi = std::acos(std::max(-1.0f, std::min(1.0f, 2.0f * v - 1.0f)));
          const float r = std::cbrt(rng.next_float()) * cfg.emission_radius;
          const float sin_p = std::sin(phi);
          shape_offset = Vec3{r * sin_p * std::cos(theta), r * std::cos(phi), r * sin_p * std::sin(theta)};
          break;
        }
      }

      p.pos = cfg.spawn_pos + shape_offset;
      const Vec3 jitter{
          (rng.next_float() * 2.0f - 1.0f) * cfg.velocity_jitter.x,
          (rng.next_float() * 2.0f - 1.0f) * cfg.velocity_jitter.y,
          (rng.next_float() * 2.0f - 1.0f) * cfg.velocity_jitter.z,
      };
      p.vel = cfg.base_velocity + jitter + shape_vel_add;
      p.age = 0.0f;
      p.lifetime = cfg.lifetime_min + rng.next_float() * (cfg.lifetime_max - cfg.lifetime_min);
      p.size_start = cfg.size_start;
      p.size_end = cfg.size_end;
      p.size = cfg.size_start;
      p.color_start = cfg.color_start;
      p.color_end = cfg.color_end;
      p.gravity = cfg.gravity;
      p.has_custom_gravity = cfg.custom_gravity;

      p.curl_noise_strength = cfg.curl_noise_strength;
      p.curl_noise_frequency = cfg.curl_noise_frequency;
      p.drag = cfg.drag;
      p.enable_collision = cfg.enable_collision;
      p.collision_plane_y = cfg.collision_plane_y;
      p.restitution = cfg.restitution;
      p.friction = cfg.friction;
      p.seed = cfg.seed + rng.next_u32();
      p.subuv_frame = 0.0f;
      p.shape = cfg.shape;

      p.spawn_on_death_count = cfg.spawn_on_death_count;
      p.spawn_on_collision_count = cfg.spawn_on_collision_count;
      p.sub_emitter = cfg.sub_emitter;
      p.sub_emitter_cfg = cfg.sub_emitter_cfg;
      spawned++;
    }
    return spawned;
  }

  // Yari-kapali (semi-implicit/symplectic) Euler guncellemesi
  void update(float dt, Rng *rng = nullptr) {
    uint32_t i = 0;
    while (i < count_) {
      Particle &p = particles_[i];
      const Vec3 g = p.has_custom_gravity ? p.gravity : gravity_;

      // Curl Noise turbulans kuvveti
      if (p.curl_noise_strength > 0.0f) {
        const Vec3 curl = curl_noise_3d(p.pos * p.curl_noise_frequency, p.seed, 2);
        p.vel += curl * (p.curl_noise_strength * dt);
      }

      // Stokes aerodinamik surtunme (drag)
      if (p.drag > 0.0f) {
        const float drag_factor = std::max(0.0f, 1.0f - p.drag * dt);
        p.vel = p.vel * drag_factor;
      }

      p.vel += g * dt;
      p.pos += p.vel * dt;

      // Zemin / Duzlem carpismasi
      if (p.enable_collision && p.pos.y <= p.collision_plane_y && p.vel.y < 0.0f) {
        p.pos.y = p.collision_plane_y;
        p.vel.y = -p.vel.y * p.restitution;
        const float lateral_factor = std::max(0.0f, 1.0f - p.friction);
        p.vel.x *= lateral_factor;
        p.vel.z *= lateral_factor;

        // Spawn on Collision (kivilcim/sicrama alt-yayicisi)
        if (p.sub_emitter && p.sub_emitter_cfg && p.spawn_on_collision_count > 0 && rng) {
          ParticleEmitterConfig sub_cfg = *p.sub_emitter_cfg;
          sub_cfg.spawn_pos = p.pos;
          p.sub_emitter->emit(sub_cfg, p.spawn_on_collision_count, *rng);
        }
      }

      p.age += dt;
      if (p.age >= p.lifetime) {
        // Spawn on Death (olum ani patlamasi / alt-yayici)
        if (p.sub_emitter && p.sub_emitter_cfg && p.spawn_on_death_count > 0 && rng) {
          ParticleEmitterConfig sub_cfg = *p.sub_emitter_cfg;
          sub_cfg.spawn_pos = p.pos;
          p.sub_emitter->emit(sub_cfg, p.spawn_on_death_count, *rng);
        }

        particles_[i] = particles_[count_ - 1];
        count_--;
        continue; // i SABIT -- yeni takas edilen eleman bu karede TEKRAR islenir
      }
      const float t = p.lifetime > 0.0001f ? (p.age / p.lifetime) : 1.0f;
      p.size = p.size_start + (p.size_end - p.size_start) * t;
      p.subuv_frame = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
      i++;
    }
  }

  uint32_t alive_count() const { return count_; }
  const Particle &particle(uint32_t i) const { return particles_[i]; }
  // Renk-uzerinden-omur gibi CAGIRANA ozel gradyanlar icin (bu sinif
  // renk/materyal bilmez -- tek sorumluluk).
  float life_fraction(uint32_t i) const { return particles_[i].age / particles_[i].lifetime; }
  Vec3 color(uint32_t i) const {
    const float t = particles_[i].lifetime > 0.0001f ? (particles_[i].age / particles_[i].lifetime) : 0.0f;
    const float ct = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return particles_[i].color_start + (particles_[i].color_end - particles_[i].color_start) * ct;
  }

 private:
  Particle *particles_ = nullptr;
  uint32_t capacity_ = 0;
  uint32_t count_ = 0;
  Vec3 gravity_{0.0f, -9.8f, 0.0f};
};

} // namespace tulpar::engine::content
