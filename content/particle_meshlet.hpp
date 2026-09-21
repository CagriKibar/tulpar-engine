// L6 CONTENT — Particle Meshlet Clustering (Task/Mesh Shader hazirligi).
// 2026 VK_EXT_mesh_shader ve GPU Indirect Draw icin parcacik kumeleme mimarisi.
// Parcaciklari 64'luk (NVIDIA/AMD warp/wavefront uyumlu) kumelere boler.
// Her kume tek bir AABB ve Bounding Sphere ile temsil edilir.
// Frustum ve Okluzyon elemesi CPU/GPU'da 64 parcacik birden tek adimda yapilir.
#pragma once
#include <algorithm>
#include <cstdint>

#include "content/particles.hpp"
#include "core/math/vec.hpp"
#include "core/memory/arena.hpp"

namespace tulpar::engine::content {

constexpr uint32_t kParticleMeshletSize = 64;

struct ParticleMeshletCluster {
  uint32_t particle_offset = 0;
  uint32_t particle_count = 0;
  Aabb bounds{};
  Sphere sphere{};
  bool visible = true;
};

class ParticleMeshletBuilder {
 public:
  bool init(Arena &arena, uint32_t max_particles) {
    max_clusters_ = (max_particles + kParticleMeshletSize - 1) / kParticleMeshletSize;
    clusters_ = arena.alloc_array<ParticleMeshletCluster>(max_clusters_);
    cluster_count_ = 0;
    return clusters_ != nullptr;
  }

  uint32_t cluster_count() const { return cluster_count_; }
  const ParticleMeshletCluster &cluster(uint32_t i) const { return clusters_[i]; }
  ParticleMeshletCluster &cluster(uint32_t i) { return clusters_[i]; }

  // Canli parcacik havuzunu 64'luk meshlet kumelerine boler ve sinirlayici kutulari hesaplar.
  void build_clusters(const ParticleSystem &ps) {
    cluster_count_ = 0;
    const uint32_t total = ps.alive_count();
    if (total == 0) return;

    cluster_count_ = (total + kParticleMeshletSize - 1) / kParticleMeshletSize;
    if (cluster_count_ > max_clusters_) cluster_count_ = max_clusters_;

    for (uint32_t c = 0; c < cluster_count_; c++) {
      ParticleMeshletCluster &cl = clusters_[c];
      cl.particle_offset = c * kParticleMeshletSize;
      const uint32_t count = std::min(kParticleMeshletSize, total - cl.particle_offset);
      cl.particle_count = count;
      cl.visible = true;

      if (count == 0) continue;

      // Kume AABB ve Sphere hesapla
      const Particle &p0 = ps.particle(cl.particle_offset);
      Vec3 min_p = p0.pos - Vec3{p0.size, p0.size, p0.size};
      Vec3 max_p = p0.pos + Vec3{p0.size, p0.size, p0.size};

      for (uint32_t i = 1; i < count; i++) {
        const Particle &p = ps.particle(cl.particle_offset + i);
        min_p = vmin(min_p, p.pos - Vec3{p.size, p.size, p.size});
        max_p = vmax(max_p, p.pos + Vec3{p.size, p.size, p.size});
      }

      cl.bounds.min = min_p;
      cl.bounds.max = max_p;
      cl.sphere.center = (min_p + max_p) * 0.5f;
      cl.sphere.radius = length(max_p - min_p) * 0.5f;
    }
  }

  // Frustum kirpmasi: 64 parcacigi tek bir AABB/Sphere testiyle eler.
  uint32_t cull_frustum(const Frustum &frustum) {
    uint32_t visible_particles = 0;
    for (uint32_t i = 0; i < cluster_count_; i++) {
      ParticleMeshletCluster &cl = clusters_[i];
      cl.visible = intersects(frustum, cl.bounds);
      if (cl.visible) {
        visible_particles += cl.particle_count;
      }
    }
    return visible_particles;
  }
 private:
  ParticleMeshletCluster *clusters_ = nullptr;
  uint32_t max_clusters_ = 0;
  uint32_t cluster_count_ = 0;
};

} // namespace tulpar::engine::content
