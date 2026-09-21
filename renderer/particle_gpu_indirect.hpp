// L3 RENDERER — Bevy Hanabi GPU Indirect, Vulkan DGC & TBDR Mobil Partikul Mimarisi.
// 2026 GPU-Driven Partikul Komut Boru Hatti:
// 1. GPU SSBO Ping-Pong Tamponlari (AliveList[2], DeadList).
// 2. vkCmdDispatchIndirect ve vkCmdDrawIndexedIndirect komut yapilari.
// 3. Vulkan DGC (Device-Generated Commands / VK_EXT_device_generated_commands) GPU token akisi.
// 4. Mobil Arm Mali / Qualcomm Adreno icin 16x16 TBDR Tile Binning bolucusu.
#pragma once
#include <algorithm>
#include <cstdint>

#include "core/math/vec.hpp"

namespace tulpar::engine::renderer {

// GPU Tarafli Partikul Sayaclari (SSBO binding uyumlu)
struct alignas(16) GpuParticleCounters {
  uint32_t alive_count = 0;
  uint32_t dead_count = 0;
  uint32_t real_emit_count = 0;
  uint32_t culled_count = 0;
};

// Vulkan VkDrawIndexedIndirectCommand ile birebir eslesen yapilandirma
struct ParticleDrawIndexedIndirectCommand {
  uint32_t index_count = 6;      // Standart 2B quad billboard icin 6 indeks
  uint32_t instance_count = 0;   // Canli ve gorunur partikul sayisi
  uint32_t first_index = 0;
  int32_t  vertex_offset = 0;
  uint32_t first_instance = 0;
};

// Vulkan VkDispatchIndirectCommand ile eslesen yapilandirma
struct ParticleDispatchIndirectCommand {
  uint32_t x = 1; // (alive_count + 63) / 64
  uint32_t y = 1;
  uint32_t z = 1;
};

// Bevy Hanabi Ping-Pong Yuva Yoneticisi
struct HanabiBufferSlots {
  uint32_t read_slot = 0;
  uint32_t write_slot = 1;

  void swap() {
    uint32_t tmp = read_slot;
    read_slot = write_slot;
    write_slot = tmp;
  }
};

// Vulkan DGC (Device-Generated Commands) Token Akisi
// GPU, CPU'ya sormadan kendi cizim ve pipeline komutlarini VRAM icinde olusturur
struct VulkanDgcTokenStream {
  uint32_t pipeline_bind_index = 0;   // Additive / Opaque / WBOIT boru hatti secimi
  uint32_t indirect_draw_offset = 0;  // Indirect buffer bayt ofseti
  uint32_t dynamic_ubo_offset = 0;    // Descriptor dinamik ofseti
};

// 16x16 Mobil TBDR (Tile-Based Deferred Rendering) Kumeleyici
constexpr uint32_t kTbdrTileSize = 16;

struct TbdrTile {
  uint32_t tile_x = 0;
  uint32_t tile_y = 0;
  uint32_t particle_count = 0;
};

class TbdrParticleBinner {
 public:
  static void compute_dispatch_args(uint32_t alive_count,
                                    ParticleDispatchIndirectCommand &out_dispatch,
                                    ParticleDrawIndexedIndirectCommand &out_draw) {
    out_draw.instance_count = alive_count;
    out_draw.index_count = 6;
    out_draw.first_index = 0;
    out_draw.vertex_offset = 0;
    out_draw.first_instance = 0;

    out_dispatch.x = (alive_count + 63) / 64; // 64 thread / warp
    out_dispatch.y = 1;
    out_dispatch.z = 1;
  }

  // 16x16 ekran tile'larina gore partikulleri siniflandirir (Mobil Mali tile bant genisligi tasarrufu)
  static bool project_to_tile(Vec2 screen_pos, uint32_t screen_w, uint32_t screen_h,
                              uint32_t &out_tile_x, uint32_t &out_tile_y) {
    if (screen_pos.x < 0.0f || screen_pos.x >= (float)screen_w ||
        screen_pos.y < 0.0f || screen_pos.y >= (float)screen_h) {
      return false;
    }
    out_tile_x = (uint32_t)screen_pos.x / kTbdrTileSize;
    out_tile_y = (uint32_t)screen_pos.y / kTbdrTileSize;
    return true;
  }
};

} // namespace tulpar::engine::renderer
