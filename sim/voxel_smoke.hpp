// L4 SIMULATION — CS2 Tarzi Duyarli Voksel Dumani (Responsive Eulerian-Lagrangian Voxel Smoke).
// 2026 Counter-Strike 2 tarzı etkileşimli duman mimarisi:
// 1. Mermi gectiginde hat boyunca dumanı silen silindirik tünel (bullet void).
// 2. Bomba patlamasında dumanı dışarı iten radyal şok dalgası (explosion displacement).
// 3. Navier-Stokes kütle korunumlu Laplacian difüzyonu ile tünelin zamanla geri kapanması (gradual refill).
// 4. Sifir-tahsis (zero heap allocation) garantisi: Arena uzerinden ping-pong tamponlar.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/math/vec.hpp"
#include "core/memory/arena.hpp"

namespace tulpar::engine::sim {

struct VoxelSmokeParams {
  float diffusion_rate = 0.15f;    // Bosluklarin geri dolma hizi
  float dissipation_rate = 0.01f;  // Zamanla kaybolma / sonme orani
};

class VoxelSmokeGrid {
 public:
  bool init(Arena &arena, uint32_t dim_x, uint32_t dim_y, uint32_t dim_z,
            Vec3 origin, float voxel_size = 0.25f) {
    dim_x_ = dim_x;
    dim_y_ = dim_y;
    dim_z_ = dim_z;
    origin_ = origin;
    voxel_size_ = voxel_size > 0.01f ? voxel_size : 0.01f;
    inv_voxel_size_ = 1.0f / voxel_size_;

    const uint32_t total = dim_x * dim_y * dim_z;
    density_ = arena.alloc_array<float>(total);
    density_next_ = arena.alloc_array<float>(total);

    if (!density_ || !density_next_) return false;

    for (uint32_t i = 0; i < total; i++) {
      density_[i] = 0.0f;
      density_next_[i] = 0.0f;
    }
    return true;
  }

  uint32_t dim_x() const { return dim_x_; }
  uint32_t dim_y() const { return dim_y_; }
  uint32_t dim_z() const { return dim_z_; }
  Vec3 origin() const { return origin_; }
  float voxel_size() const { return voxel_size_; }

  inline uint32_t index(int32_t x, int32_t y, int32_t z) const {
    return (uint32_t)z * (dim_x_ * dim_y_) + (uint32_t)y * dim_x_ + (uint32_t)x;
  }

  inline bool in_bounds(int32_t x, int32_t y, int32_t z) const {
    return x >= 0 && x < (int32_t)dim_x_ &&
           y >= 0 && y < (int32_t)dim_y_ &&
           z >= 0 && z < (int32_t)dim_z_;
  }

  inline float get_density(int32_t x, int32_t y, int32_t z) const {
    return in_bounds(x, y, z) ? density_[index(x, y, z)] : 0.0f;
  }

  void clear() {
    const uint32_t total = dim_x_ * dim_y_ * dim_z_;
    for (uint32_t i = 0; i < total; i++) {
      density_[i] = 0.0f;
      density_next_[i] = 0.0f;
    }
  }

  // Dunya koordinatini voksel koordinatina donusturur
  Vec3 world_to_voxel(Vec3 world_p) const {
    return (world_p - origin_) * inv_voxel_size_;
  }

  // Voksel indeksini dunya koordinatina donusturur
  Vec3 voxel_to_world(int32_t x, int32_t y, int32_t z) const {
    return origin_ + Vec3{((float)x + 0.5f) * voxel_size_,
                          ((float)y + 0.5f) * voxel_size_,
                          ((float)z + 0.5f) * voxel_size_};
  }

  // Belirli bir alana duman pufu enjekte eder
  void inject_smoke(Vec3 center, float radius, float density) {
    const Vec3 vox_c = world_to_voxel(center);
    const float vox_r = radius * inv_voxel_size_;
    const int32_t min_x = std::max(0, (int32_t)std::floor(vox_c.x - vox_r));
    const int32_t max_x = std::min((int32_t)dim_x_ - 1, (int32_t)std::ceil(vox_c.x + vox_r));
    const int32_t min_y = std::max(0, (int32_t)std::floor(vox_c.y - vox_r));
    const int32_t max_y = std::min((int32_t)dim_y_ - 1, (int32_t)std::ceil(vox_c.y + vox_r));
    const int32_t min_z = std::max(0, (int32_t)std::floor(vox_c.z - vox_r));
    const int32_t max_z = std::min((int32_t)dim_z_ - 1, (int32_t)std::ceil(vox_c.z + vox_r));

    const float r_sq = vox_r * vox_r;
    for (int32_t z = min_z; z <= max_z; z++) {
      for (int32_t y = min_y; y <= max_y; y++) {
        for (int32_t x = min_x; x <= max_x; x++) {
          const float dx = (float)x - vox_c.x;
          const float dy = (float)y - vox_c.y;
          const float dz = (float)z - vox_c.z;
          const float d_sq = dx * dx + dy * dy + dz * dz;
          if (d_sq <= r_sq) {
            const float falloff = 1.0f - std::sqrt(d_sq) / vox_r;
            const uint32_t idx = index(x, y, z);
            density_[idx] = std::min(1.0f, density_[idx] + density * falloff);
          }
        }
      }
    }
  }

  // CS2 Mermi Deligi (Bullet Void):
  // Mermi hatti boyunca silindirik duman boslugu oyar (yogunlugu 0 yapar)
  void carve_bullet_tunnel(Vec3 ray_origin, Vec3 ray_dir, float length, float radius) {
    const Vec3 dir_norm = normalize(ray_dir);
    const float step_size = voxel_size_ * 0.5f;
    const uint32_t num_steps = (uint32_t)std::ceil(length / step_size);
    const float rad_sq = radius * radius;

    for (uint32_t s = 0; s <= num_steps; s++) {
      const Vec3 p = ray_origin + dir_norm * (s * step_size);
      const Vec3 vox = world_to_voxel(p);
      const int32_t cx = (int32_t)std::round(vox.x);
      const int32_t cy = (int32_t)std::round(vox.y);
      const int32_t cz = (int32_t)std::round(vox.z);
      const int32_t r_vox = (int32_t)std::ceil(radius * inv_voxel_size_);

      for (int32_t dz = -r_vox; dz <= r_vox; dz++) {
        for (int32_t dy = -r_vox; dy <= r_vox; dy++) {
          for (int32_t dx = -r_vox; dx <= r_vox; dx++) {
            const int32_t vx = cx + dx;
            const int32_t vy = cy + dy;
            const int32_t vz = cz + dz;
            if (!in_bounds(vx, vy, vz)) continue;

            const Vec3 world_vox = voxel_to_world(vx, vy, vz);
            // Hattan noktaya dik mesafe
            const Vec3 v = world_vox - ray_origin;
            const float t = dot(v, dir_norm);
            if (t < 0.0f || t > length) continue;
            const Vec3 proj = ray_origin + dir_norm * t;
            const float dist_sq = length_sq(world_vox - proj);
            if (dist_sq <= rad_sq) {
              density_[index(vx, vy, vz)] = 0.0f;
            }
          }
        }
      }
    }
  }

  // CS2 Bomba / Patlama Sok Dalgasi (Explosion Shockwave):
  // Icerideki dumanı disari dogru iter, merkezde kuresel bosluk acar
  void apply_explosion_shockwave(Vec3 center, float inner_radius, float outer_radius, float push_force) {
    const Vec3 vox_c = world_to_voxel(center);
    const float r_in_vox = inner_radius * inv_voxel_size_;
    const float r_out_vox = outer_radius * inv_voxel_size_;
    const int32_t r_max = (int32_t)std::ceil(r_out_vox);

    const int32_t cx = (int32_t)std::round(vox_c.x);
    const int32_t cy = (int32_t)std::round(vox_c.y);
    const int32_t cz = (int32_t)std::round(vox_c.z);

    for (int32_t dz = -r_max; dz <= r_max; dz++) {
      for (int32_t dy = -r_max; dy <= r_max; dy++) {
        for (int32_t dx = -r_max; dx <= r_max; dx++) {
          const int32_t vx = cx + dx;
          const int32_t vy = cy + dy;
          const int32_t vz = cz + dz;
          if (!in_bounds(vx, vy, vz)) continue;

          const float dist = std::sqrt((float)(dx * dx + dy * dy + dz * dz));
          const uint32_t idx = index(vx, vy, vz);
          if (dist <= r_in_vox) {
            // Tam ic bolge tamamen temizlenir
            density_[idx] = 0.0f;
          } else if (dist <= r_out_vox) {
            // Dis cidar disari dogru itilir (seyrelir)
            const float factor = (dist - r_in_vox) / (r_out_vox - r_in_vox);
            density_[idx] *= factor * (1.0f - std::min(1.0f, push_force));
          }
        }
      }
    }
  }

  // Zaman adimi: 3B Laplacian difuzyonu (bosluklarin geri dolmasi) + hafif sonumleme
  void update(float dt, const VoxelSmokeParams &params) {
    const float diff = params.diffusion_rate * dt;
    const float diss = std::max(0.0f, 1.0f - params.dissipation_rate * dt);

    for (int32_t z = 0; z < (int32_t)dim_z_; z++) {
      for (int32_t y = 0; y < (int32_t)dim_y_; y++) {
        for (int32_t x = 0; x < (int32_t)dim_x_; x++) {
          const uint32_t idx = index(x, y, z);
          const float cur = density_[idx];

          // 6 komsu ortalamasi (3B Laplacian)
          float neighbor_sum = 0.0f;
          uint32_t neighbor_count = 0;

          if (x > 0) { neighbor_sum += density_[index(x - 1, y, z)]; neighbor_count++; }
          if (x + 1 < (int32_t)dim_x_) { neighbor_sum += density_[index(x + 1, y, z)]; neighbor_count++; }
          if (y > 0) { neighbor_sum += density_[index(x, y - 1, z)]; neighbor_count++; }
          if (y + 1 < (int32_t)dim_y_) { neighbor_sum += density_[index(x, y + 1, z)]; neighbor_count++; }
          if (z > 0) { neighbor_sum += density_[index(x, y, z - 1)]; neighbor_count++; }
          if (z + 1 < (int32_t)dim_z_) { neighbor_sum += density_[index(x, y, z + 1)]; neighbor_count++; }

          const float avg = neighbor_count > 0 ? (neighbor_sum / (float)neighbor_count) : cur;
          const float new_val = (cur + (avg - cur) * diff) * diss;
          density_next_[idx] = new_val < 0.001f ? 0.0f : (new_val > 1.0f ? 1.0f : new_val);
        }
      }
    }

    // Tampon takasi
    const uint32_t total = dim_x_ * dim_y_ * dim_z_;
    for (uint32_t i = 0; i < total; i++) {
      density_[i] = density_next_[i];
    }
  }

  // Surekli dunya noktasinda trilinear duman yogunlugu ornekleme
  float sample_density(Vec3 world_pos) const {
    const Vec3 vox = world_to_voxel(world_pos);
    const int32_t x0 = (int32_t)std::floor(vox.x);
    const int32_t y0 = (int32_t)std::floor(vox.y);
    const int32_t z0 = (int32_t)std::floor(vox.z);

    if (x0 < 0 || x0 + 1 >= (int32_t)dim_x_ ||
        y0 < 0 || y0 + 1 >= (int32_t)dim_y_ ||
        z0 < 0 || z0 + 1 >= (int32_t)dim_z_) {
      return 0.0f;
    }

    const float fx = vox.x - (float)x0;
    const float fy = vox.y - (float)y0;
    const float fz = vox.z - (float)z0;

    const float d000 = density_[index(x0, y0, z0)];
    const float d100 = density_[index(x0 + 1, y0, z0)];
    const float d010 = density_[index(x0, y0 + 1, z0)];
    const float d110 = density_[index(x0 + 1, y0 + 1, z0)];
    const float d001 = density_[index(x0, y0, z0 + 1)];
    const float d101 = density_[index(x0 + 1, y0, z0 + 1)];
    const float d011 = density_[index(x0, y0 + 1, z0 + 1)];
    const float d111 = density_[index(x0 + 1, y0 + 1, z0 + 1)];

    const float x00 = d000 + (d100 - d000) * fx;
    const float x10 = d010 + (d110 - d010) * fx;
    const float x01 = d001 + (d101 - d001) * fx;
    const float x11 = d011 + (d111 - d011) * fx;

    const float y0_val = x00 + (x10 - x00) * fy;
    const float y1_val = x01 + (x11 - x01) * fy;

    return y0_val + (y1_val - y0_val) * fz;
  }

  // Toplam duman kutlesi (korunum testleri icin)
  float total_mass() const {
    float sum = 0.0f;
    const uint32_t total = dim_x_ * dim_y_ * dim_z_;
    for (uint32_t i = 0; i < total; i++) {
      sum += density_[i];
    }
    return sum;
  }

 private:
  float *density_ = nullptr;
  float *density_next_ = nullptr;
  uint32_t dim_x_ = 0;
  uint32_t dim_y_ = 0;
  uint32_t dim_z_ = 0;
  Vec3 origin_{};
  float voxel_size_ = 0.25f;
  float inv_voxel_size_ = 4.0f;
};

} // namespace tulpar::engine::sim
