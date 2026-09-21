// L3 RENDERER — WBOIT (Weighted Blended Order-Independent Transparency) & Hareket Vektorleri.
// 2026 Gelismis Optik: Morgan McGuire & Louis Bavoil (JCGT 2013) siralamasiz seffaflik cozumu.
// Siralama (O(N log N)) gerektirmeden GPU uzerinde tek geciste kusursuz alev, duman ve cam harmanlama.
// Hedefler:
//   1. Accumulation Buffer (RGBA16F): Renk ve agirlikli seffaflik toplami
//   2. Revealage Buffer (R8): Kalan arka plan gecirgenligi carpimi (R = prod(1 - alpha))
//   3. Composite Pass: C_final = (Accum.rgb / max(Accum.a, 1e-4)) * (1 - R) + C_bg * R
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/math/vec.hpp"

namespace tulpar::engine::renderer {

struct WboitPassConfig {
  float depth_scale = 1.0f;
  float weight_bias = 1e-5f;
  bool enable_motion_vectors = true;
};

// McGuire & Bavoil 2013 derinlik agirlik fonksiyonu
inline float compute_wboit_weight(float depth, float alpha) {
  const float clamped_alpha = std::max(0.0f, std::min(1.0f, alpha));
  const float z = std::max(0.001f, depth);
  const float z_over_5 = z * 0.2f;
  const float z_over_200 = z * 0.005f;

  const float z_term2 = z_over_5 * z_over_5;
  const float z_term6 = z_over_200 * z_over_200 * z_over_200 * z_over_200 * z_over_200 * z_over_200;

  const float w = 10.0f / (1e-5f + z_term2 + z_term6);
  const float weight = clamped_alpha * std::max(0.01f, std::min(3000.0f, w));
  return weight;
}

// Fragment Shading sirasinda Accumulation ve Revealage tamponuna yazilacak degerler
struct WboitFragmentOutput {
  Vec4 accum;     // RGB * alpha * weight, alpha * weight
  float revealage; // 1 - alpha (Blending: ZERO, ONE_MINUS_SRC_COLOR)
};

inline WboitFragmentOutput evaluate_wboit_fragment(Vec4 color, float depth) {
  const float w = compute_wboit_weight(depth, color.w);
  return {
    {color.x * color.w * w, color.y * color.w * w, color.z * color.w * w, color.w * w},
    1.0f - color.w
  };
}

// 2026 Gelismis Emissive Extension: Voksel duman, alev ve parildayan enerji efektleri
// icin tek geciste additive ve seffaf harmanlama (McGuire & Bavoil 2013 uzantisi)
inline WboitFragmentOutput evaluate_wboit_fragment_emissive(Vec4 color, float depth, float emissive_additivity = 0.0f) {
  const float clamped_alpha = std::max(0.0f, std::min(1.0f, color.w));
  const float w = compute_wboit_weight(depth, clamped_alpha);
  const float total_weight = (clamped_alpha + emissive_additivity) * w;
  const float inv_revealage = std::max(0.0f, 1.0f - clamped_alpha * (1.0f - std::min(1.0f, emissive_additivity)));
  return {
    {color.x * (clamped_alpha + emissive_additivity) * w,
     color.y * (clamped_alpha + emissive_additivity) * w,
     color.z * (clamped_alpha + emissive_additivity) * w,
     total_weight},
    inv_revealage
  };
}

// Bilesik (Composite) Pass: Ekran uzerinde nihai harmanlama
inline Vec4 composite_wboit(Vec4 accum, float revealage, Vec4 background) {
  const float r = std::max(0.0f, std::min(1.0f, revealage));
  const float a = std::max(1e-4f, accum.w);
  const Vec3 transparent_color = {accum.x / a, accum.y / a, accum.z / a};

  const float one_minus_r = 1.0f - r;
  return {
    transparent_color.x * one_minus_r + background.x * r,
    transparent_color.y * one_minus_r + background.y * r,
    transparent_color.z * one_minus_r + background.z * r,
    background.w * r + one_minus_r
  };
}

// TAA / FSR per-particle hareket vektoru hesabi
inline Vec2 compute_particle_motion_vector(Vec4 cur_clip, Vec4 prev_clip) {
  if (std::fabs(cur_clip.w) < 1e-6f || std::fabs(prev_clip.w) < 1e-6f) {
    return {0.0f, 0.0f};
  }
  const Vec2 cur_ndc = {cur_clip.x / cur_clip.w, cur_clip.y / cur_clip.w};
  const Vec2 prev_ndc = {prev_clip.x / prev_clip.w, prev_clip.y / prev_clip.w};

  // NDC uzayindan UV uzayina (0.5 olcek)
  return (cur_ndc - prev_ndc) * 0.5f;
}

} // namespace tulpar::engine::renderer
