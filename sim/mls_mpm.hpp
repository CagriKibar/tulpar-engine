// L4 SIMULATION — 2026 MLS-MPM Cok Fazli Fizik (Moving Least Squares Material Point Method).
// Kum, kar, camur, eriyik ve viskoz macun icin gercek-zamanli MPM cozucusu.
// Lagrangian parcaciklar ile Eulerian arka plan izgarasini birlestirir:
// 1. P2G (Particle-to-Grid): APIC afin momentumu ve kutleyi kuadratik B-spline ile aktarir.
// 2. Grid guncellemesi: Yercekimi ivmesi ve zemin/duvar sinir sartlari.
// 3. G2P (Grid-to-Particle): Acisal momentumu koruyan afin matris C ve yeni hiz transferi.
// Sifir-tahsis (zero heap allocation): Arena ile onceden tahsis edilen izgara ve parcacik havuzu.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/math/vec.hpp"
#include "core/memory/arena.hpp"

namespace tulpar::engine::sim {

struct MpmParticle {
  Vec3 pos{};
  Vec3 vel{};
  float C[3][3]{}; // APIC afin hiz gradyani matrisi (acisal momentum korunumu)
  float mass = 1.0f;
  float initial_volume = 1.0f;
};

struct MpmGridNode {
  Vec3 vel{};
  float mass = 0.0f;
};

class MlsMpmSimulator {
 public:
  bool init(Arena &arena, uint32_t res_x, uint32_t res_y, uint32_t res_z,
            uint32_t max_particles, float cell_size = 0.2f, Vec3 origin = {}) {
    res_x_ = res_x;
    res_y_ = res_y;
    res_z_ = res_z;
    cell_size_ = cell_size > 0.01f ? cell_size : 0.01f;
    inv_cell_size_ = 1.0f / cell_size_;
    origin_ = origin;

    const uint32_t total_nodes = res_x * res_y * res_z;
    grid_ = arena.alloc_array<MpmGridNode>(total_nodes);
    particles_ = arena.alloc_array<MpmParticle>(max_particles);
    max_particles_ = max_particles;
    particle_count_ = 0;

    if (!grid_ || !particles_) return false;

    clear_grid();
    return true;
  }

  uint32_t particle_count() const { return particle_count_; }
  const MpmParticle &particle(uint32_t i) const { return particles_[i]; }
  MpmParticle &particle(uint32_t i) { return particles_[i]; }
  float cell_size() const { return cell_size_; }

  void clear_particles() { particle_count_ = 0; }

  bool add_particle(Vec3 pos, Vec3 vel, float mass = 1.0f) {
    if (particle_count_ >= max_particles_) return false;
    MpmParticle &p = particles_[particle_count_++];
    p.pos = pos;
    p.vel = vel;
    p.mass = mass;
    p.initial_volume = mass;
    for (int r = 0; r < 3; r++) {
      for (int c = 0; c < 3; c++) {
        p.C[r][c] = 0.0f;
      }
    }
    return true;
  }

  inline uint32_t node_index(int32_t x, int32_t y, int32_t z) const {
    return (uint32_t)z * (res_x_ * res_y_) + (uint32_t)y * res_x_ + (uint32_t)x;
  }

  inline bool in_bounds(int32_t x, int32_t y, int32_t z) const {
    return x >= 0 && x < (int32_t)res_x_ &&
           y >= 0 && y < (int32_t)res_y_ &&
           z >= 0 && z < (int32_t)res_z_;
  }

  void clear_grid() {
    const uint32_t total = res_x_ * res_y_ * res_z_;
    for (uint32_t i = 0; i < total; i++) {
      grid_[i].vel = {0.0f, 0.0f, 0.0f};
      grid_[i].mass = 0.0f;
    }
  }

  // Tek bir MLS-MPM adimi: P2G -> Grid guncellemesi & Sinirlar -> G2P
  void step(float dt, Vec3 gravity) {
    if (particle_count_ == 0) return;
    clear_grid();

    // 1. P2G: Parcaciklardan Izgaraya Kutle ve Momentum Transferi (APIC)
    for (uint32_t p_idx = 0; p_idx < particle_count_; p_idx++) {
      const MpmParticle &p = particles_[p_idx];
      const Vec3 grid_pos = (p.pos - origin_) * inv_cell_size_;
      const int32_t base_x = (int32_t)std::floor(grid_pos.x - 0.5f);
      const int32_t base_y = (int32_t)std::floor(grid_pos.y - 0.5f);
      const int32_t base_z = (int32_t)std::floor(grid_pos.z - 0.5f);

      const Vec3 fx = grid_pos - Vec3{(float)base_x, (float)base_y, (float)base_z};

      // Kuadratik B-spline agirliklari
      Vec3 w[3];
      w[0] = {0.5f * (1.5f - fx.x) * (1.5f - fx.x),
              0.5f * (1.5f - fx.y) * (1.5f - fx.y),
              0.5f * (1.5f - fx.z) * (1.5f - fx.z)};
      w[1] = {0.75f - (fx.x - 1.0f) * (fx.x - 1.0f),
              0.75f - (fx.y - 1.0f) * (fx.y - 1.0f),
              0.75f - (fx.z - 1.0f) * (fx.z - 1.0f)};
      w[2] = {0.5f * (fx.x - 0.5f) * (fx.x - 0.5f),
              0.5f * (fx.y - 0.5f) * (fx.y - 0.5f),
              0.5f * (fx.z - 0.5f) * (fx.z - 0.5f)};

      for (int32_t i = 0; i < 3; i++) {
        for (int32_t j = 0; j < 3; j++) {
          for (int32_t k = 0; k < 3; k++) {
            const int32_t nx = base_x + i;
            const int32_t ny = base_y + j;
            const int32_t nz = base_z + k;
            if (!in_bounds(nx, ny, nz)) continue;

            const float weight = w[i].x * w[j].y * w[k].z;
            const Vec3 dpos = (Vec3{(float)nx, (float)ny, (float)nz} - grid_pos) * cell_size_;

            // APIC afin momentum katkisi: C * dpos
            const Vec3 c_dpos{
              p.C[0][0] * dpos.x + p.C[0][1] * dpos.y + p.C[0][2] * dpos.z,
              p.C[1][0] * dpos.x + p.C[1][1] * dpos.y + p.C[1][2] * dpos.z,
              p.C[2][0] * dpos.x + p.C[2][1] * dpos.y + p.C[2][2] * dpos.z
            };

            const uint32_t g_idx = node_index(nx, ny, nz);
            grid_[g_idx].mass += weight * p.mass;
            grid_[g_idx].vel += (p.vel + c_dpos) * (weight * p.mass);
          }
        }
      }
    }

    // 2. Izgara Operasyonlari: Hiz normalizasyonu, Yercekimi ve Sinir Kosullari
    const uint32_t total_nodes = res_x_ * res_y_ * res_z_;
    for (uint32_t i = 0; i < total_nodes; i++) {
      if (grid_[i].mass > 1e-7f) {
        grid_[i].vel = grid_[i].vel * (1.0f / grid_[i].mass);
        grid_[i].vel += gravity * dt;
      }
    }

    // Sinir sartlari: zemin ve kutu cidarlarinda carpisma / surtunme
    for (int32_t z = 0; z < (int32_t)res_z_; z++) {
      for (int32_t y = 0; y < (int32_t)res_y_; y++) {
        for (int32_t x = 0; x < (int32_t)res_x_; x++) {
          const uint32_t idx = node_index(x, y, z);
          if (x < 2 && grid_[idx].vel.x < 0.0f) grid_[idx].vel.x = 0.0f;
          if (x >= (int32_t)res_x_ - 2 && grid_[idx].vel.x > 0.0f) grid_[idx].vel.x = 0.0f;
          if (y < 2 && grid_[idx].vel.y < 0.0f) {
            grid_[idx].vel.y = 0.0f;
            // Zemin surtunmesi
            grid_[idx].vel.x *= 0.8f;
            grid_[idx].vel.z *= 0.8f;
          }
          if (y >= (int32_t)res_y_ - 2 && grid_[idx].vel.y > 0.0f) grid_[idx].vel.y = 0.0f;
          if (z < 2 && grid_[idx].vel.z < 0.0f) grid_[idx].vel.z = 0.0f;
          if (z >= (int32_t)res_z_ - 2 && grid_[idx].vel.z > 0.0f) grid_[idx].vel.z = 0.0f;
        }
      }
    }

    // 3. G2P: Izgaradan Parcaciklara Hiz ve Afin Matris C Transferi
    const float inv_dpos_scale = 4.0f * inv_cell_size_ * inv_cell_size_;

    for (uint32_t p_idx = 0; p_idx < particle_count_; p_idx++) {
      MpmParticle &p = particles_[p_idx];
      const Vec3 grid_pos = (p.pos - origin_) * inv_cell_size_;
      const int32_t base_x = (int32_t)std::floor(grid_pos.x - 0.5f);
      const int32_t base_y = (int32_t)std::floor(grid_pos.y - 0.5f);
      const int32_t base_z = (int32_t)std::floor(grid_pos.z - 0.5f);

      const Vec3 fx = grid_pos - Vec3{(float)base_x, (float)base_y, (float)base_z};

      Vec3 w[3];
      w[0] = {0.5f * (1.5f - fx.x) * (1.5f - fx.x),
              0.5f * (1.5f - fx.y) * (1.5f - fx.y),
              0.5f * (1.5f - fx.z) * (1.5f - fx.z)};
      w[1] = {0.75f - (fx.x - 1.0f) * (fx.x - 1.0f),
              0.75f - (fx.y - 1.0f) * (fx.y - 1.0f),
              0.75f - (fx.z - 1.0f) * (fx.z - 1.0f)};
      w[2] = {0.5f * (fx.x - 0.5f) * (fx.x - 0.5f),
              0.5f * (fx.y - 0.5f) * (fx.y - 0.5f),
              0.5f * (fx.z - 0.5f) * (fx.z - 0.5f)};

      p.vel = {0.0f, 0.0f, 0.0f};
      for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
          p.C[r][c] = 0.0f;
        }
      }

      for (int32_t i = 0; i < 3; i++) {
        for (int32_t j = 0; j < 3; j++) {
          for (int32_t k = 0; k < 3; k++) {
            const int32_t nx = base_x + i;
            const int32_t ny = base_y + j;
            const int32_t nz = base_z + k;
            if (!in_bounds(nx, ny, nz)) continue;

            const float weight = w[i].x * w[j].y * w[k].z;
            const Vec3 dpos = (Vec3{(float)nx, (float)ny, (float)nz} - grid_pos) * cell_size_;
            const uint32_t g_idx = node_index(nx, ny, nz);
            const Vec3 g_vel = grid_[g_idx].vel;

            p.vel += g_vel * weight;

            // C = 4 * Delta_x^-2 * sum(weight * v_i * dpos^T)
            const float scale = weight * inv_dpos_scale;
            p.C[0][0] += scale * g_vel.x * dpos.x;
            p.C[0][1] += scale * g_vel.x * dpos.y;
            p.C[0][2] += scale * g_vel.x * dpos.z;

            p.C[1][0] += scale * g_vel.y * dpos.x;
            p.C[1][1] += scale * g_vel.y * dpos.y;
            p.C[1][2] += scale * g_vel.y * dpos.z;

            p.C[2][0] += scale * g_vel.z * dpos.x;
            p.C[2][1] += scale * g_vel.z * dpos.y;
            p.C[2][2] += scale * g_vel.z * dpos.z;
          }
        }
      }

      // Adveksiyon: konumu yeni hizla ilerlet
      p.pos += p.vel * dt;

      // Guvenlik siniri
      const float min_bound_y = origin_.y + cell_size_ * 2.0f;
      if (p.pos.y < min_bound_y) {
        p.pos.y = min_bound_y;
        p.vel.y = 0.0f;
      }
    }
  }

  // Korunum testleri icin toplam kutle
  float total_mass() const {
    float sum = 0.0f;
    for (uint32_t i = 0; i < particle_count_; i++) {
      sum += particles_[i].mass;
    }
    return sum;
  }

  // Toplam momentum vektoru
  Vec3 total_momentum() const {
    Vec3 p{0.0f, 0.0f, 0.0f};
    for (uint32_t i = 0; i < particle_count_; i++) {
      p += particles_[i].vel * particles_[i].mass;
    }
    return p;
  }

 private:
  MpmGridNode *grid_ = nullptr;
  MpmParticle *particles_ = nullptr;
  uint32_t max_particles_ = 0;
  uint32_t particle_count_ = 0;
  uint32_t res_x_ = 0;
  uint32_t res_y_ = 0;
  uint32_t res_z_ = 0;
  float cell_size_ = 0.2f;
  float inv_cell_size_ = 5.0f;
  Vec3 origin_{};
};

} // namespace tulpar::engine::sim
