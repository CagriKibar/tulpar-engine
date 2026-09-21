// L6 CONTENT — Dynamic Ribbon Trail geometry generation.
// Sifir-tahsis (zero heap allocation) garantisi: Arena uzerinden onden
// ayrilmis sabit kapasite. Parcaciklarin arkasinda yumusak kuyruk izi
// (ribbon quad strip) olusturur. Kamera acisina gore billboard normali
// hesaplar ve uclara dogru seffaflasip incelen serit uretir.
#pragma once
#include <cmath>
#include <cstdint>

#include "core/math/vec.hpp"
#include "core/memory/arena.hpp"

namespace tulpar::engine::content {

struct RibbonPoint {
  Vec3 pos{};
  Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
  float width = 0.2f;
  float age = 0.0f;
  float lifetime = 1.0f;
};

struct RibbonVertex {
  Vec3 pos{};
  Vec2 uv{};
  Vec4 color{};
};

class RibbonTrail {
 public:
  bool init(Arena &arena, uint32_t max_points) {
    points_ = arena.alloc_array<RibbonPoint>(max_points);
    capacity_ = max_points;
    count_ = 0;
    return points_ != nullptr;
  }

  void clear() { count_ = 0; }
  uint32_t count() const { return count_; }
  uint32_t capacity() const { return capacity_; }
  const RibbonPoint &point(uint32_t i) const { return points_[i]; }
  RibbonPoint &point(uint32_t i) { return points_[i]; }

  void add_point(Vec3 pos, Vec4 color, float width, float lifetime = 1.0f) {
    if (count_ >= capacity_) {
      // En eski noktayi kaydir (ring buffer veya linear shift)
      for (uint32_t i = 1; i < count_; i++) {
        points_[i - 1] = points_[i];
      }
      count_ = capacity_ - 1;
    }
    RibbonPoint &p = points_[count_++];
    p.pos = pos;
    p.color = color;
    p.width = width;
    p.age = 0.0f;
    p.lifetime = lifetime > 0.001f ? lifetime : 0.001f;
  }

  void update(float dt) {
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < count_; i++) {
      points_[i].age += dt;
      if (points_[i].age < points_[i].lifetime) {
        if (write_idx != i) {
          points_[write_idx] = points_[i];
        }
        write_idx++;
      }
    }
    count_ = write_idx;
  }

  // Kamera yonune dik billboard quad strip geometrisi olusturur.
  // out_vertices: en az count * 2 kapasiteli olmalidir.
  // out_indices: en az (count - 1) * 6 kapasiteli olmalidir.
  void build_geometry(Vec3 camera_pos,
                      RibbonVertex *out_vertices, uint32_t &out_vcount,
                      uint32_t *out_indices, uint32_t &out_icount) const {
    out_vcount = 0;
    out_icount = 0;
    if (count_ < 2) return;

    for (uint32_t i = 0; i < count_; i++) {
      const RibbonPoint &pt = points_[i];
      const float life_ratio = pt.age / pt.lifetime;
      const float fade = 1.0f - (life_ratio < 0.0f ? 0.0f : (life_ratio > 1.0f ? 1.0f : life_ratio));
      const float current_width = pt.width * fade;

      // Teget vektoru
      Vec3 tangent{};
      if (i == 0) {
        tangent = normalize(points_[1].pos - pt.pos);
      } else if (i == count_ - 1) {
        tangent = normalize(pt.pos - points_[i - 1].pos);
      } else {
        tangent = normalize(points_[i + 1].pos - points_[i - 1].pos);
      }

      Vec3 to_cam = normalize(camera_pos - pt.pos);
      Vec3 side = cross(tangent, to_cam);
      if (length_sq(side) < 0.0001f) {
        side = {0.0f, 1.0f, 0.0f};
      } else {
        side = normalize(side);
      }

      const Vec3 offset = side * (current_width * 0.5f);
      const float u = (float)i / (float)(count_ - 1);
      Vec4 col = pt.color;
      col.w *= fade;

      // Sol vertex
      out_vertices[out_vcount++] = {
        pt.pos - offset,
        {u, 0.0f},
        col
      };

      // Sag vertex
      out_vertices[out_vcount++] = {
        pt.pos + offset,
        {u, 1.0f},
        col
      };

      // Indeksleri olustur (iki komsu cift arasinda 2 ucgen = 6 indeks)
      if (i > 0) {
        const uint32_t cur_left = (i * 2);
        const uint32_t cur_right = cur_left + 1;
        const uint32_t prev_left = cur_left - 2;
        const uint32_t prev_right = cur_left - 1;

        out_indices[out_icount++] = prev_left;
        out_indices[out_icount++] = prev_right;
        out_indices[out_icount++] = cur_left;

        out_indices[out_icount++] = cur_left;
        out_indices[out_icount++] = prev_right;
        out_indices[out_icount++] = cur_right;
      }
    }
  }

 private:
  RibbonPoint *points_ = nullptr;
  uint32_t capacity_ = 0;
  uint32_t count_ = 0;
};

} // namespace tulpar::engine::content
