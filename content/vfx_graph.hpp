// L6 CONTENT — Cift-Yonlu Dugum Grafi Mimarisi (Node-Based VFX Graph).
// Unity VFX Graph Context Mimarisi ve Unreal Niagara Yigin / Cizge Hibriti.
// Sifir dinamik tahsis (0 alloc): Sabit kapasiteli dugum dizisi ve statik baglanti tablosu.
#pragma once
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>

#include "content/particles.hpp"
#include "core/math/vec.hpp"

namespace tulpar::engine::content {

enum class VfxNodeType : uint8_t {
  Event = 0,
  Context = 1,
  Block = 2,
  Operator = 3,
};

enum class VfxContextStage : uint8_t {
  Spawn = 0,
  Initialize = 1,
  Update = 2,
  Output = 3,
};

enum class VfxBlockType : uint8_t {
  SetPosition = 0,
  SetVelocity = 1,
  AddVelocity = 2,
  SetLifetime = 3,
  CurlNoise = 4,
  StokesDrag = 5,
  ColorOverLife = 6,
  SizeOverLife = 7,
  CollisionPlane = 8,
  SubEmitterOnDeath = 9,
};

enum class VfxOperatorType : uint8_t {
  Add = 0,
  Multiply = 1,
  Sin = 2,
  Cos = 3,
  RandomRange = 4,
  VectorCompose = 5,
};

struct VfxPin {
  char name[16]{};
  bool is_output = false;
  float default_val = 0.0f;
};

struct VfxNode {
  uint32_t id = 0;
  VfxNodeType type = VfxNodeType::Context;
  char title[32]{};
  float pos_x = 0.0f;
  float pos_y = 0.0f;
  float width = 160.0f;
  float height = 90.0f;

  VfxContextStage stage = VfxContextStage::Spawn;
  VfxBlockType block_type = VfxBlockType::SetPosition;
  VfxOperatorType op_type = VfxOperatorType::Add;

  // Blok / Operator parametreleri
  Vec3 vec_param{0.0f, 0.0f, 0.0f};
  Vec3 vec_param2{1.0f, 1.0f, 1.0f};
  float float_param = 1.0f;
  float float_param2 = 0.0f;
  bool bool_param = false;
  uint32_t int_param = 0;
};

struct VfxConnection {
  uint32_t from_node = 0;
  uint32_t from_pin = 0;
  uint32_t to_node = 0;
  uint32_t to_pin = 0;
};

class VfxGraph {
 public:
  static constexpr uint32_t kMaxNodes = 32;
  static constexpr uint32_t kMaxConnections = 48;

  void clear() {
    node_count_ = 0;
    conn_count_ = 0;
  }

  uint32_t node_count() const { return node_count_; }
  uint32_t connection_count() const { return conn_count_; }

  const VfxNode &node(uint32_t i) const { return nodes_[i]; }
  VfxNode &node_mut(uint32_t i) { return nodes_[i]; }
  const VfxConnection &connection(uint32_t i) const { return connections_[i]; }

  uint32_t add_node(VfxNodeType type, const char *title, float x, float y) {
    if (node_count_ >= kMaxNodes) return 0xFFFFFFFFu;
    const uint32_t id = node_count_++;
    VfxNode &n = nodes_[id];
    n.id = id;
    n.type = type;
    n.pos_x = x;
    n.pos_y = y;
    std::strncpy(n.title, title, sizeof(n.title) - 1);
    n.title[sizeof(n.title) - 1] = '\0';
    return id;
  }

  bool add_connection(uint32_t from_node, uint32_t from_pin, uint32_t to_node, uint32_t to_pin) {
    if (conn_count_ >= kMaxConnections) return false;
    connections_[conn_count_++] = {from_node, from_pin, to_node, to_pin};
    return true;
  }

  // Varsayilan Ates (Fire) Efekti Grafigi Olustur
  void build_fire_graph() {
    clear();
    // 1. Event Node
    uint32_t ev = add_node(VfxNodeType::Event, "OnPlay", 40.0f, 50.0f);
    if (ev < kMaxNodes) nodes_[ev].width = 120.0f;

    // 2. Spawn Context
    uint32_t ctx_spawn = add_node(VfxNodeType::Context, "Spawn Context", 200.0f, 50.0f);
    if (ctx_spawn < kMaxNodes) {
      nodes_[ctx_spawn].stage = VfxContextStage::Spawn;
      nodes_[ctx_spawn].float_param = 65.0f; // spawn rate
    }

    // 3. Initialize Context
    uint32_t ctx_init = add_node(VfxNodeType::Context, "Initialize", 390.0f, 50.0f);
    if (ctx_init < kMaxNodes) {
      nodes_[ctx_init].stage = VfxContextStage::Initialize;
      nodes_[ctx_init].vec_param = Vec3{0.0f, 2.8f, 0.0f}; // base velocity
      nodes_[ctx_init].vec_param2 = Vec3{0.4f, 0.6f, 0.4f}; // jitter
      nodes_[ctx_init].float_param = 0.8f; // min lifetime
      nodes_[ctx_init].float_param2 = 1.6f; // max lifetime
      nodes_[ctx_init].int_param = static_cast<uint32_t>(ParticleEmissionShape::PlanarRing);
    }

    // 4. Update Context
    uint32_t ctx_update = add_node(VfxNodeType::Context, "Update Context", 580.0f, 50.0f);
    if (ctx_update < kMaxNodes) {
      nodes_[ctx_update].stage = VfxContextStage::Update;
      nodes_[ctx_update].vec_param = Vec3{0.0f, -9.8f, 0.0f}; // gravity
      nodes_[ctx_update].float_param = 2.5f; // curl noise strength
      nodes_[ctx_update].float_param2 = 0.2f; // drag
    }

    // 5. Output Context (WBOIT / Mesh)
    uint32_t ctx_output = add_node(VfxNodeType::Context, "Output (WBOIT)", 770.0f, 50.0f);
    if (ctx_output < kMaxNodes) {
      nodes_[ctx_output].stage = VfxContextStage::Output;
      nodes_[ctx_output].vec_param = Vec3{1.0f, 0.6f, 0.1f}; // color start
      nodes_[ctx_output].vec_param2 = Vec3{0.2f, 0.1f, 0.05f}; // color end
      nodes_[ctx_output].float_param = 0.22f; // size start
      nodes_[ctx_output].float_param2 = 0.03f; // size end
    }

    // Baglantilari kur (Flow)
    add_connection(ev, 0, ctx_spawn, 0);
    add_connection(ctx_spawn, 0, ctx_init, 0);
    add_connection(ctx_init, 0, ctx_update, 0);
    add_connection(ctx_update, 0, ctx_output, 0);
  }

  void build_smoke_graph() {
    clear();
    uint32_t ev = add_node(VfxNodeType::Event, "OnPlay", 40.0f, 50.0f);
    uint32_t ctx_spawn = add_node(VfxNodeType::Context, "Spawn Context", 200.0f, 50.0f);
    if (ctx_spawn < kMaxNodes) {
      nodes_[ctx_spawn].stage = VfxContextStage::Spawn;
      nodes_[ctx_spawn].float_param = 20.0f;
    }
    uint32_t ctx_init = add_node(VfxNodeType::Context, "Initialize", 390.0f, 50.0f);
    if (ctx_init < kMaxNodes) {
      nodes_[ctx_init].stage = VfxContextStage::Initialize;
      nodes_[ctx_init].vec_param = Vec3{0.1f, 1.0f, 0.05f};
      nodes_[ctx_init].vec_param2 = Vec3{0.3f, 0.2f, 0.3f};
      nodes_[ctx_init].float_param = 2.5f;
      nodes_[ctx_init].float_param2 = 4.5f;
      nodes_[ctx_init].int_param = static_cast<uint32_t>(ParticleEmissionShape::SphericalVolume);
    }
    uint32_t ctx_update = add_node(VfxNodeType::Context, "Update Context", 580.0f, 50.0f);
    if (ctx_update < kMaxNodes) {
      nodes_[ctx_update].stage = VfxContextStage::Update;
      nodes_[ctx_update].vec_param = Vec3{0.0f, 0.15f, 0.0f};
      nodes_[ctx_update].float_param = 1.2f;
      nodes_[ctx_update].float_param2 = 0.4f;
    }
    uint32_t ctx_output = add_node(VfxNodeType::Context, "Output (WBOIT)", 770.0f, 50.0f);
    if (ctx_output < kMaxNodes) {
      nodes_[ctx_output].stage = VfxContextStage::Output;
      nodes_[ctx_output].vec_param = Vec3{0.45f, 0.45f, 0.45f};
      nodes_[ctx_output].vec_param2 = Vec3{0.12f, 0.12f, 0.12f};
      nodes_[ctx_output].float_param = 0.12f;
      nodes_[ctx_output].float_param2 = 0.95f;
    }
    add_connection(ev, 0, ctx_spawn, 0);
    add_connection(ctx_spawn, 0, ctx_init, 0);
    add_connection(ctx_init, 0, ctx_update, 0);
    add_connection(ctx_update, 0, ctx_output, 0);
  }

  void build_sparks_graph() {
    clear();
    uint32_t ev = add_node(VfxNodeType::Event, "OnPlay", 40.0f, 50.0f);
    uint32_t ctx_spawn = add_node(VfxNodeType::Context, "Spawn Context", 200.0f, 50.0f);
    if (ctx_spawn < kMaxNodes) {
      nodes_[ctx_spawn].stage = VfxContextStage::Spawn;
      nodes_[ctx_spawn].float_param = 90.0f;
    }
    uint32_t ctx_init = add_node(VfxNodeType::Context, "Initialize", 390.0f, 50.0f);
    if (ctx_init < kMaxNodes) {
      nodes_[ctx_init].stage = VfxContextStage::Initialize;
      nodes_[ctx_init].vec_param = Vec3{0.0f, 5.0f, 0.0f};
      nodes_[ctx_init].vec_param2 = Vec3{3.5f, 2.0f, 3.5f};
      nodes_[ctx_init].float_param = 0.6f;
      nodes_[ctx_init].float_param2 = 1.4f;
      nodes_[ctx_init].int_param = static_cast<uint32_t>(ParticleEmissionShape::ConicalFountain);
    }
    uint32_t ctx_update = add_node(VfxNodeType::Context, "Update Context", 580.0f, 50.0f);
    if (ctx_update < kMaxNodes) {
      nodes_[ctx_update].stage = VfxContextStage::Update;
      nodes_[ctx_update].vec_param = Vec3{0.0f, -9.8f, 0.0f};
      nodes_[ctx_update].float_param = 0.5f;
      nodes_[ctx_update].float_param2 = 0.08f;
    }
    uint32_t ctx_output = add_node(VfxNodeType::Context, "Output (WBOIT)", 770.0f, 50.0f);
    if (ctx_output < kMaxNodes) {
      nodes_[ctx_output].stage = VfxContextStage::Output;
      nodes_[ctx_output].vec_param = Vec3{1.0f, 0.95f, 0.4f};
      nodes_[ctx_output].vec_param2 = Vec3{0.9f, 0.15f, 0.0f};
      nodes_[ctx_output].float_param = 0.09f;
      nodes_[ctx_output].float_param2 = 0.01f;
    }
    add_connection(ev, 0, ctx_spawn, 0);
    add_connection(ctx_spawn, 0, ctx_init, 0);
    add_connection(ctx_init, 0, ctx_update, 0);
    add_connection(ctx_update, 0, ctx_output, 0);
  }

  void build_explosion_graph() {
    clear();
    uint32_t ev = add_node(VfxNodeType::Event, "OnImpact", 40.0f, 50.0f);
    uint32_t ctx_spawn = add_node(VfxNodeType::Context, "Burst Spawn", 200.0f, 50.0f);
    if (ctx_spawn < kMaxNodes) {
      nodes_[ctx_spawn].stage = VfxContextStage::Spawn;
      nodes_[ctx_spawn].float_param = 0.0f;
    }
    uint32_t ctx_init = add_node(VfxNodeType::Context, "Initialize", 390.0f, 50.0f);
    if (ctx_init < kMaxNodes) {
      nodes_[ctx_init].stage = VfxContextStage::Initialize;
      nodes_[ctx_init].vec_param = Vec3{0.0f, 2.0f, 0.0f};
      nodes_[ctx_init].vec_param2 = Vec3{5.0f, 5.0f, 5.0f};
      nodes_[ctx_init].float_param = 0.5f;
      nodes_[ctx_init].float_param2 = 1.2f;
      nodes_[ctx_init].int_param = static_cast<uint32_t>(ParticleEmissionShape::SphericalVolume);
    }
    uint32_t ctx_update = add_node(VfxNodeType::Context, "Update Context", 580.0f, 50.0f);
    if (ctx_update < kMaxNodes) {
      nodes_[ctx_update].stage = VfxContextStage::Update;
      nodes_[ctx_update].vec_param = Vec3{0.0f, -3.0f, 0.0f};
      nodes_[ctx_update].float_param = 2.0f;
      nodes_[ctx_update].float_param2 = 0.2f;
    }
    uint32_t ctx_output = add_node(VfxNodeType::Context, "Output (WBOIT)", 770.0f, 50.0f);
    if (ctx_output < kMaxNodes) {
      nodes_[ctx_output].stage = VfxContextStage::Output;
      nodes_[ctx_output].vec_param = Vec3{1.0f, 0.8f, 0.1f};
      nodes_[ctx_output].vec_param2 = Vec3{0.8f, 0.1f, 0.0f};
      nodes_[ctx_output].float_param = 0.35f;
      nodes_[ctx_output].float_param2 = 0.05f;
    }
    add_connection(ev, 0, ctx_spawn, 0);
    add_connection(ctx_spawn, 0, ctx_init, 0);
    add_connection(ctx_init, 0, ctx_update, 0);
    add_connection(ctx_update, 0, ctx_output, 0);
  }

  void build_magic_graph() {
    clear();
    uint32_t ev = add_node(VfxNodeType::Event, "OnCast", 40.0f, 50.0f);
    uint32_t ctx_spawn = add_node(VfxNodeType::Context, "Spawn Context", 200.0f, 50.0f);
    if (ctx_spawn < kMaxNodes) {
      nodes_[ctx_spawn].stage = VfxContextStage::Spawn;
      nodes_[ctx_spawn].float_param = 55.0f;
    }
    uint32_t ctx_init = add_node(VfxNodeType::Context, "Initialize", 390.0f, 50.0f);
    if (ctx_init < kMaxNodes) {
      nodes_[ctx_init].stage = VfxContextStage::Initialize;
      nodes_[ctx_init].vec_param = Vec3{0.0f, 2.2f, 0.0f};
      nodes_[ctx_init].vec_param2 = Vec3{1.0f, 1.0f, 1.0f};
      nodes_[ctx_init].float_param = 1.5f;
      nodes_[ctx_init].float_param2 = 2.8f;
      nodes_[ctx_init].int_param = static_cast<uint32_t>(ParticleEmissionShape::PlanarRing);
    }
    uint32_t ctx_update = add_node(VfxNodeType::Context, "Update Context", 580.0f, 50.0f);
    if (ctx_update < kMaxNodes) {
      nodes_[ctx_update].stage = VfxContextStage::Update;
      nodes_[ctx_update].vec_param = Vec3{0.0f, 0.1f, 0.0f};
      nodes_[ctx_update].float_param = 6.5f;
      nodes_[ctx_update].float_param2 = 0.1f;
    }
    uint32_t ctx_output = add_node(VfxNodeType::Context, "Output (WBOIT)", 770.0f, 50.0f);
    if (ctx_output < kMaxNodes) {
      nodes_[ctx_output].stage = VfxContextStage::Output;
      nodes_[ctx_output].vec_param = Vec3{0.2f, 0.9f, 1.0f};
      nodes_[ctx_output].vec_param2 = Vec3{0.9f, 0.15f, 0.95f};
      nodes_[ctx_output].float_param = 0.14f;
      nodes_[ctx_output].float_param2 = 0.02f;
    }
    add_connection(ev, 0, ctx_spawn, 0);
    add_connection(ctx_spawn, 0, ctx_init, 0);
    add_connection(ctx_init, 0, ctx_update, 0);
    add_connection(ctx_update, 0, ctx_output, 0);
  }

  // Grafigi derleyip ParticleEmitterConfig'e yaz
  bool compile_to_config(ParticleEmitterConfig &out_cfg) const {
    bool has_init = false;
    for (uint32_t i = 0; i < node_count_; i++) {
      const VfxNode &n = nodes_[i];
      if (n.type == VfxNodeType::Context) {
        if (n.stage == VfxContextStage::Initialize) {
          out_cfg.base_velocity = n.vec_param;
          out_cfg.velocity_jitter = n.vec_param2;
          out_cfg.lifetime_min = n.float_param;
          out_cfg.lifetime_max = n.float_param2;
          out_cfg.emission_shape = static_cast<ParticleEmissionShape>(n.int_param);
          has_init = true;
        } else if (n.stage == VfxContextStage::Update) {
          out_cfg.gravity = n.vec_param;
          out_cfg.custom_gravity = true;
          out_cfg.curl_noise_strength = n.float_param;
          out_cfg.drag = n.float_param2;
        } else if (n.stage == VfxContextStage::Output) {
          out_cfg.color_start = n.vec_param;
          out_cfg.color_end = n.vec_param2;
          out_cfg.size_start = n.float_param;
          out_cfg.size_end = n.float_param2;
        }
      }
    }
    return has_init;
  }

  // Vulkan Compute Shader Kaynagi (GLSL SPIR-V) Derle
  size_t compile_to_glsl_compute(char *out_buf, size_t max_len) const {
    if (!out_buf || max_len < 256) return 0;
    ParticleEmitterConfig cfg;
    compile_to_config(cfg);

    int written = std::snprintf(
        out_buf, max_len,
        "#version 450\n"
        "layout(local_size_x = 256) in;\n"
        "struct ParticleData {\n"
        "  vec4 pos_age;\n"
        "  vec4 vel_size;\n"
        "  vec4 color;\n"
        "};\n"
        "layout(std430, binding = 0) buffer ParticleBuffer { ParticleData particles[]; };\n"
        "layout(push_constant) uniform Constants { float dt; uint count; };\n"
        "void main() {\n"
        "  uint id = gl_GlobalInvocationID.x;\n"
        "  if (id >= count) return;\n"
        "  vec3 gravity = vec3(%.2ff, %.2ff, %.2ff);\n"
        "  float drag = %.2ff;\n"
        "  particles[id].vel_size.xyz = (particles[id].vel_size.xyz + gravity * dt) * max(0.0, 1.0 - drag * dt);\n"
        "  particles[id].pos_age.xyz += particles[id].vel_size.xyz * dt;\n"
        "  particles[id].pos_age.w += dt;\n"
        "}\n",
        cfg.gravity.x, cfg.gravity.y, cfg.gravity.z, cfg.drag);

    return written > 0 ? static_cast<size_t>(written) : 0;
  }

 private:
  VfxNode nodes_[kMaxNodes]{};
  VfxConnection connections_[kMaxConnections]{};
  uint32_t node_count_ = 0;
  uint32_t conn_count_ = 0;
};

} // namespace tulpar::engine::content
