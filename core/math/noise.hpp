// L1 CORE — Deterministik deger gurultusu (value noise) + fraktal Brownian
// motion (fBm). core/math/random.hpp'nin (500 madde listesi #438 "PCG")
// DOGAL UZANTISI: terrain yukseklik haritasi, bulut/varyasyon gibi
// prosedurel icerigin standart ikinci tas'i. Perlin'in DEGRADE gurultusu
// DEGIL (daha basit deger-gurultusu -- kafes noktalarinda hash DEGERLERI,
// gradyan degil -- ilk dilim icin yeterli, daha ucuz).
//
// TAM SAYI hash + smoothstep enterpolasyonu -- kayan nokta/libm'e yalnizca
// floor/interpolasyonda ihtiyac var, HASH'in kendisi tam sayi bit
// operasyonlari (motorun determinizm ilkesiyle ayni ruh, core/math/random.hpp
// gibi -- ayni (nokta,seed) HER platformda AYNI degeri verir).
#pragma once
#include <cmath>
#include <cstdint>
#include "core/math/vec.hpp"

namespace tulpar::engine {

namespace detail {
inline float smoothstep01(float t) { return t * t * (3.0f - 2.0f * t); }
} // namespace detail

// Tam sayi kafes noktasini (ix,iy) [0,1) araliginda deterministik bir
// degere hash'ler -- ayni core/math/random.hpp'deki next_float() gibi ust
// 24 bit kullanilir (float mantissa hassasiyetiyle eslesir).
inline float lattice_hash(int32_t ix, int32_t iy, uint32_t seed) {
  uint32_t h = seed;
  h ^= (uint32_t)ix * 0x27d4eb2fu;
  h = (h ^ (h >> 15)) * 0x85ebca6bu;
  h ^= (uint32_t)iy * 0x165667b1u;
  h = (h ^ (h >> 13)) * 0xc2b2ae35u;
  h ^= h >> 16;
  return (float)(h >> 8) * (1.0f / 16777216.0f);
}

// 3B tam sayi kafes noktasini deterministik [0,1) degerine hash'ler.
inline float lattice_hash_3d(int32_t ix, int32_t iy, int32_t iz, uint32_t seed) {
  uint32_t h = seed;
  h ^= (uint32_t)ix * 0x27d4eb2fu;
  h = (h ^ (h >> 15)) * 0x85ebca6bu;
  h ^= (uint32_t)iy * 0x165667b1u;
  h = (h ^ (h >> 13)) * 0xc2b2ae35u;
  h ^= ((uint32_t)iz * 0x9e3779b9u) ^ 0x31415927u;
  h = (h ^ (h >> 16)) * 0x85ebca6bu;
  h ^= h >> 16;
  return (float)(h >> 8) * (1.0f / 16777216.0f);
}

// 2B deger gurultusu: x,y herhangi bir kayan nokta konumu. Cevredeki 4
// kafes noktasinin hash degerleri arasinda smoothstep ile yumusak
// enterpolasyon -- TAM bir kafes noktasinda (x,y tam sayi) SONUC O
// noktanin lattice_hash'ine BIREBIR esittir (enterpolasyon agirligi o
// kosede 1, digerlerinde 0 olur -- bu, testlerde ELLE kanitlanan ozelliktir).
// Donus HER ZAMAN [0,1) araliginda (4 kose degerinin konveks kombinasyonu).
inline float value_noise_2d(float x, float y, uint32_t seed) {
  const int32_t ix0 = (int32_t)std::floor(x);
  const int32_t iy0 = (int32_t)std::floor(y);
  const int32_t ix1 = ix0 + 1, iy1 = iy0 + 1;
  const float fx = x - (float)ix0;
  const float fy = y - (float)iy0;

  const float v00 = lattice_hash(ix0, iy0, seed);
  const float v10 = lattice_hash(ix1, iy0, seed);
  const float v01 = lattice_hash(ix0, iy1, seed);
  const float v11 = lattice_hash(ix1, iy1, seed);

  const float sx = detail::smoothstep01(fx);
  const float sy = detail::smoothstep01(fy);

  const float a = v00 + (v10 - v00) * sx;
  const float b = v01 + (v11 - v01) * sx;
  return a + (b - a) * sy;
}

// 3B deger gurultusu: x,y,z konumu etrafindaki 8 kafes noktasinin trilinear
// smoothstep enterpolasyonu. Tam kafes noktalarinda lattice_hash_3d ile birebirdir.
inline float value_noise_3d(float x, float y, float z, uint32_t seed) {
  const int32_t ix0 = (int32_t)std::floor(x);
  const int32_t iy0 = (int32_t)std::floor(y);
  const int32_t iz0 = (int32_t)std::floor(z);
  const int32_t ix1 = ix0 + 1, iy1 = iy0 + 1, iz1 = iz0 + 1;

  const float fx = x - (float)ix0;
  const float fy = y - (float)iy0;
  const float fz = z - (float)iz0;

  const float v000 = lattice_hash_3d(ix0, iy0, iz0, seed);
  const float v100 = lattice_hash_3d(ix1, iy0, iz0, seed);
  const float v010 = lattice_hash_3d(ix0, iy1, iz0, seed);
  const float v110 = lattice_hash_3d(ix1, iy1, iz0, seed);
  const float v001 = lattice_hash_3d(ix0, iy0, iz1, seed);
  const float v101 = lattice_hash_3d(ix1, iy0, iz1, seed);
  const float v011 = lattice_hash_3d(ix0, iy1, iz1, seed);
  const float v111 = lattice_hash_3d(ix1, iy1, iz1, seed);

  const float sx = detail::smoothstep01(fx);
  const float sy = detail::smoothstep01(fy);
  const float sz = detail::smoothstep01(fz);

  const float a0 = v000 + (v100 - v000) * sx;
  const float a1 = v010 + (v110 - v010) * sx;
  const float b0 = v001 + (v101 - v001) * sx;
  const float b1 = v011 + (v111 - v011) * sx;

  const float y0 = a0 + (a1 - a0) * sy;
  const float y1 = b0 + (b1 - b0) * sy;

  return y0 + (y1 - y0) * sz;
}

// Fraktal Brownian Motion 2B
inline float fbm_2d(float x, float y, uint32_t seed, int octaves, float lacunarity = 2.0f,
                     float gain = 0.5f) {
  float sum = 0.0f, amplitude = 1.0f, freq = 1.0f, max_amp = 0.0f;
  for (int i = 0; i < octaves; i++) {
    sum += value_noise_2d(x * freq, y * freq, seed + (uint32_t)i * 0x9E3779B9u) * amplitude;
    max_amp += amplitude;
    amplitude *= gain;
    freq *= lacunarity;
  }
  return max_amp > 0.0f ? sum / max_amp : 0.0f;
}

// Fraktal Brownian Motion 3B
inline float fbm_3d(Vec3 p, uint32_t seed, int octaves, float lacunarity = 2.0f,
                    float gain = 0.5f) {
  float sum = 0.0f, amplitude = 1.0f, freq = 1.0f, max_amp = 0.0f;
  for (int i = 0; i < octaves; i++) {
    sum += value_noise_3d(p.x * freq, p.y * freq, p.z * freq,
                          seed + (uint32_t)i * 0x9E3779B9u) * amplitude;
    max_amp += amplitude;
    amplitude *= gain;
    freq *= lacunarity;
  }
  return max_amp > 0.0f ? sum / max_amp : 0.0f;
}

// 3B Curl Noise (Divergence-Free / Korunumlu Akiskan Turbulansi)
// Matematiksel temeli: v = curl(A) = nabla x A.
// Vektor potansiyeli A uc bagimsiz skaler gurultuden (fbm_3d) olusur.
// curl operatorunun diverjansi ozdes olarak sifirdir: div(curl(A)) == 0.
// Bu sayede parcaciklar kumeleşmez veya hacim kaybetmez; dogal duman/girdap hissi verir.
inline Vec3 curl_noise_3d(Vec3 p, uint32_t seed, int octaves = 3, float eps = 0.001f) {
  const float inv_2eps = 1.0f / (2.0f * eps);
  const uint32_t seed_x = seed;
  const uint32_t seed_y = seed + 0x68bc21u;
  const uint32_t seed_z = seed + 0xd23e59u;

  // A_z'nin y'ye gore turevi
  const float az_py = fbm_3d({p.x, p.y + eps, p.z}, seed_z, octaves);
  const float az_ny = fbm_3d({p.x, p.y - eps, p.z}, seed_z, octaves);
  const float dAz_dy = (az_py - az_ny) * inv_2eps;

  // A_y'nin z'ye gore turevi
  const float ay_pz = fbm_3d({p.x, p.y, p.z + eps}, seed_y, octaves);
  const float ay_nz = fbm_3d({p.x, p.y, p.z - eps}, seed_y, octaves);
  const float dAy_dz = (ay_pz - ay_nz) * inv_2eps;

  // A_x'in z'ye gore turevi
  const float ax_pz = fbm_3d({p.x, p.y, p.z + eps}, seed_x, octaves);
  const float ax_nz = fbm_3d({p.x, p.y, p.z - eps}, seed_x, octaves);
  const float dAx_dz = (ax_pz - ax_nz) * inv_2eps;

  // A_z'nin x'e gore turevi
  const float az_px = fbm_3d({p.x + eps, p.y, p.z}, seed_z, octaves);
  const float az_nx = fbm_3d({p.x - eps, p.y, p.z}, seed_z, octaves);
  const float dAz_dx = (az_px - az_nx) * inv_2eps;

  // A_y'nin x'e gore turevi
  const float ay_px = fbm_3d({p.x + eps, p.y, p.z}, seed_y, octaves);
  const float ay_nx = fbm_3d({p.x - eps, p.y, p.z}, seed_y, octaves);
  const float dAy_dx = (ay_px - ay_nx) * inv_2eps;

  // A_x'in y'ye gore turevi
  const float ax_py = fbm_3d({p.x, p.y + eps, p.z}, seed_x, octaves);
  const float ax_ny = fbm_3d({p.x, p.y - eps, p.z}, seed_x, octaves);
  const float dAx_dy = (ax_py - ax_ny) * inv_2eps;

  return {
    dAz_dy - dAy_dz,
    dAx_dz - dAz_dx,
    dAy_dx - dAx_dy
  };
}

} // namespace tulpar::engine
