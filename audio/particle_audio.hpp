// L3 AUDIO — Fiziksel Partikul Sonifikasyonu (Kinetic Particle Sonification).
// 2026 Gercek Zamanli Fiziksel Modelleme:
// Sabit .wav donguleri yerine partikul carpisma anindaki kinetik enerji (E = 0.5 * m * v^2)
// ve carpilan yuzey materyali (Metal, Ahsap, Cam, Toprak, Su) ile anlik rezonans tonu sentezler.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/math/vec.hpp"

namespace tulpar::engine::audio {

enum class ParticleSurfaceMaterial : uint8_t {
  kWater = 0,
  kWood = 1,
  kStone = 2,
  kMetal = 3,
  kGlass = 4
};

struct ParticleImpactAcoustics {
  float frequency_hz = 1000.0f;
  float amplitude = 0.5f;
  float decay_time = 0.05f; // Saniyede sonumlenme
};

class ParticleSonifier {
 public:
  // Kinetik enerji ve materyalden akustik parametreleri sentezler
  static ParticleImpactAcoustics evaluate_impact(float mass, float velocity_mag,
                                                ParticleSurfaceMaterial mat) {
    const float kinetic_energy = 0.5f * mass * (velocity_mag * velocity_mag);
    const float energy_norm = std::min(1.0f, kinetic_energy * 0.1f);

    ParticleImpactAcoustics result;
    result.amplitude = std::min(1.0f, std::sqrt(energy_norm));

    switch (mat) {
      case ParticleSurfaceMaterial::kWater:
        // Su damlasi: Minik kabarcik rezonansi (500 - 800 Hz)
        result.frequency_hz = 500.0f + 300.0f * (1.0f - energy_norm);
        result.decay_time = 0.03f;
        break;
      case ParticleSurfaceMaterial::kWood:
        // Ahsap: Tok rezonans (400 - 1200 Hz)
        result.frequency_hz = 400.0f + 800.0f * energy_norm;
        result.decay_time = 0.08f;
        break;
      case ParticleSurfaceMaterial::kStone:
        // Tas / Kaya: Sert orta frekans (1200 - 2400 Hz)
        result.frequency_hz = 1200.0f + 1200.0f * energy_norm;
        result.decay_time = 0.04f;
        break;
      case ParticleSurfaceMaterial::kMetal:
        // Metal: Parlak, tiz rezonans (2500 - 4500 Hz)
        result.frequency_hz = 2500.0f + 2000.0f * energy_norm;
        result.decay_time = 0.20f;
        break;
      case ParticleSurfaceMaterial::kGlass:
        // Cam: Cok yuksek frekansli tıkırtı (4000 - 6500 Hz)
        result.frequency_hz = 4000.0f + 2500.0f * energy_norm;
        result.decay_time = 0.12f;
        break;
    }

    return result;
  }

  // Tek kutuplu rezonator dalga ornekleyicisi (modal sentezleyici)
  static float generate_sample(float t, const ParticleImpactAcoustics &acoustics) {
    if (t > acoustics.decay_time) return 0.0f;
    const float env = std::exp(-t * (4.0f / acoustics.decay_time));
    const float phase = 2.0f * kPi * acoustics.frequency_hz * t;
    return std::sin(phase) * env * acoustics.amplitude;
  }
};

} // namespace tulpar::engine::audio
