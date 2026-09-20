// L6 CONTENT — TulparEngine Yerel Prefab (Ön Tanımlı Varlık) Gerçekleştirmesi (prefab.cpp)
//
// O3DE ve Prowl açık kaynak oyun motorlarındaki şablon/prefab mimarisinin
// TulparEngine deterministik veri modeline sıfır harici kütüphane ve sıfır-tahsisle uyarlamasıdır.

#include "content/prefab.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace tulpar::engine::content {

namespace {

static void skip_ws(const char *&p) {
  while (*p && (*p == ' ' || *p == '\t' || *p == '\r')) p++;
}

static bool match_tok(const char *&p, const char *tok) {
  skip_ws(p);
  size_t len = std::strlen(tok);
  if (std::strncmp(p, tok, len) == 0 && (p[len] == ' ' || p[len] == '\t' || p[len] == '\r' || p[len] == '\n' || p[len] == 0)) {
    p += len;
    return true;
  }
  return false;
}

static bool parse_float(const char *&p, float &out) {
  skip_ws(p);
  char *end = nullptr;
  out = std::strtof(p, &end);
  if (end == p) return false;
  p = end;
  return true;
}

static bool parse_uint(const char *&p, uint32_t &out) {
  skip_ws(p);
  char *end = nullptr;
  unsigned long v = std::strtoul(p, &end, 10);
  if (end == p) return false;
  out = static_cast<uint32_t>(v);
  p = end;
  return true;
}

static bool parse_int(const char *&p, int32_t &out) {
  skip_ws(p);
  char *end = nullptr;
  long v = std::strtol(p, &end, 10);
  if (end == p) return false;
  out = static_cast<int32_t>(v);
  p = end;
  return true;
}

static bool parse_vec3(const char *&p, Vec3 &out) {
  return parse_float(p, out.x) && parse_float(p, out.y) && parse_float(p, out.z);
}

static bool parse_vec2(const char *&p, Vec2 &out) {
  return parse_float(p, out.x) && parse_float(p, out.y);
}

static bool parse_quoted_str(const char *&p, char *out, size_t max_len) {
  skip_ws(p);
  if (*p != '"') return false;
  p++;
  size_t i = 0;
  while (*p && *p != '"' && i < max_len - 1) {
    out[i++] = *p++;
  }
  out[i] = 0;
  if (*p == '"') { p++; return true; }
  return false;
}

} // namespace

bool prefab_save(const SceneEntity &e, const char *filepath) {
  FILE *f = std::fopen(filepath, "w");
  if (!f) return false;

  std::fprintf(f, "tulpar-prefab %u\n", kPrefabVersion);
  std::fprintf(f, "nesne \"%s\"\n", e.name[0] ? e.name : "PrefabVarlik");
  std::fprintf(f, "  konum %.4f %.4f %.4f\n", e.pos.x, e.pos.y, e.pos.z);
  std::fprintf(f, "  donus %.4f %.4f %.4f\n", e.rot_deg.x, e.rot_deg.y, e.rot_deg.z);
  std::fprintf(f, "  olcek %.4f %.4f %.4f\n", e.scale.x, e.scale.y, e.scale.z);
  if (e.flags) std::fprintf(f, "  bayrak %u\n", e.flags);

  if (e.components & kSceneModel) {
    std::fprintf(f, "  model %d %.4f %.4f %.4f\n", e.asset, e.tint.x, e.tint.y, e.tint.z);
    if (e.primitive >= 0) std::fprintf(f, "  ilkel %d\n", e.primitive);
    std::fprintf(f, "  malzeme %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n",
                 e.metallic, e.roughness, e.reflectance,
                 e.emissive.x, e.emissive.y, e.emissive.z, e.emissive_strength);
  }
  if (e.components & kSceneAnim) {
    std::fprintf(f, "  animasyon %u %.4f %.4f\n", e.clip, e.phase, e.speed);
  }
  if (e.components & kSceneLight) {
    const char *lt = "nokta";
    if (e.light_type == SceneLightType::Directional) lt = "yonlu";
    else if (e.light_type == SceneLightType::Spot) lt = "spot";
    else if (e.light_type == SceneLightType::Rect) lt = "alan";
    else if (e.light_type == SceneLightType::Capsule) lt = "tup";
    else if (e.light_type == SceneLightType::Disk) lt = "disk";

    std::fprintf(f, "  isik %.4f %.4f %.4f %.4f %.4f %s\n",
                 e.light_color.x, e.light_color.y, e.light_color.z,
                 e.light_intensity, e.light_radius, lt);
    if (e.light_type == SceneLightType::Spot) {
      std::fprintf(f, "  spot_koni %.4f %.4f\n", e.light_spot_inner, e.light_spot_outer);
    } else if (e.light_type == SceneLightType::Rect || e.light_type == SceneLightType::Capsule || e.light_type == SceneLightType::Disk) {
      std::fprintf(f, "  isik_boyut %.4f %.4f\n", e.light_width, e.light_height);
    }
    if (e.light_godray) {
      std::fprintf(f, "  isik_huzmesi acik %.4f\n", e.light_godray_intensity);
    }
  }
  if (e.components & kSceneBody) {
    if (e.shape == SceneShape::Box) {
      std::fprintf(f, "  govde kutu %.4f %.4f %.4f %s\n", e.half.x, e.half.y, e.half.z, e.dynamic ? "dinamik" : "sabit");
    } else {
      std::fprintf(f, "  govde kure %.4f %s\n", e.radius, e.dynamic ? "dinamik" : "sabit");
    }
  }
  if (e.components & kSceneCamera) {
    std::fprintf(f, "  kamera %.4f %.4f %.4f\n", e.cam_fov, e.cam_near, e.cam_far);
  }
  if (e.components & kSceneAudio) {
    std::fprintf(f, "  ses \"%s\" %.4f %.4f %s %s\n",
                 e.audio_clip, e.audio_volume, e.audio_pitch,
                 e.audio_loop ? "dongu" : "tek",
                 e.audio_spatial ? "uzamsal" : "2b");
  }
  if (e.components & kSceneScript) {
    std::fprintf(f, "  betik \"%s\" %s\n", e.script_file, e.script_enabled ? "etkin" : "kapali");
  }
  if (e.components & kSceneCharacter) {
    std::fprintf(f, "  karakter %.4f %.4f %.4f %.4f\n", e.char_radius, e.char_height, e.char_mass, e.char_max_slope);
  }
  if (e.components & kSceneParticle) {
    std::fprintf(f, "  partikul %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n",
                 e.particle_spawn_rate, e.particle_lifetime_min, e.particle_lifetime_max,
                 e.particle_size_start, e.particle_size_end,
                 e.particle_velocity.x, e.particle_velocity.y, e.particle_velocity.z,
                 e.particle_jitter.x, e.particle_jitter.y, e.particle_jitter.z);
    std::fprintf(f, "  partikul_renk %.4f %.4f %.4f %.4f %.4f %.4f\n",
                 e.particle_color_start.x, e.particle_color_start.y, e.particle_color_start.z,
                 e.particle_color_end.x, e.particle_color_end.y, e.particle_color_end.z);
    std::fprintf(f, "  partikul_fizik %.4f %u\n", e.particle_gravity, e.particle_billboard_type);
  }
  if (e.components & kSceneTerrain) {
    std::fprintf(f, "  arazi %.4f %.4f %.4f %.4f %.4f %d %u\n",
                 e.terrain_width, e.terrain_height, e.terrain_cell,
                 e.terrain_amp, e.terrain_freq, e.terrain_octaves, e.terrain_seed);
  }
  if (e.components & kSceneWater) {
    std::fprintf(f, "  su %.4f %.4f %.4f %.4f %.4f %.4f\n",
                 e.wave_length, e.wave_amplitude, e.wave_steepness, e.wave_speed,
                 e.wave_direction.x, e.wave_direction.y);
  }
  if (e.components & kSceneWind) {
    std::fprintf(f, "  ruzgar %.4f %.4f %.4f %.4f %.4f %u\n",
                 e.wind_direction.x, e.wind_direction.y, e.wind_strength,
                 e.wind_gustiness, e.wind_gust_freq, e.wind_seed);
  }
  if (e.components & kSceneVoxel) {
    std::fprintf(f, "  voksel %u %u %u %.4f\n", e.voxel_size_x, e.voxel_size_y, e.voxel_size_z, e.voxel_cell);
  }
  if (e.components & kSceneJoint) {
    std::fprintf(f, "  eklem %d %.4f %.4f %.4f %.4f %.4f %.4f\n",
                 e.joint_target, e.joint_axis.x, e.joint_axis.y, e.joint_axis.z,
                 e.joint_limit_min, e.joint_limit_max, e.joint_motor_speed);
  }
  if (e.components & kSceneRefProbe) {
    std::fprintf(f, "  yansima_sondasi %.4f %.4f\n", e.ref_probe_radius, e.ref_probe_intensity);
  }
  if (e.components & kSceneReverb) {
    std::fprintf(f, "  yanki %.4f %.4f\n", e.reverb_decay, e.reverb_room_size);
  }
  if (e.components & kSceneHealth) {
    std::fprintf(f, "  saglik %.4f %.4f\n", e.health_max, e.health_current);
  }
  if (e.components & kSceneAbility) {
    std::fprintf(f, "  yetenek %u %.4f %.4f %.4f\n", e.ability_id, e.ability_damage, e.ability_range, e.ability_cooldown);
  }
  if (e.components & kSceneNavAgent) {
    std::fprintf(f, "  navagent %.4f %.4f %.4f %.4f %.4f\n",
                 e.ai_target.x, e.ai_target.y, e.ai_target.z, e.ai_speed, e.ai_turn_speed);
  }

  std::fprintf(f, "son\n");
  std::fclose(f);
  return true;
}

bool prefab_load(const char *filepath, PrefabData &out_prefab) {
  FILE *f = std::fopen(filepath, "r");
  if (!f) return false;

  out_prefab = PrefabData{};
  SceneEntity &e = out_prefab.entity;
  char line[512];
  bool in_header = false;
  bool in_entity = false;

  while (std::fgets(line, sizeof(line), f)) {
    const char *p = line;
    skip_ws(p);
    if (*p == 0 || *p == '#') continue;

    if (!in_header) {
      if (match_tok(p, "tulpar-prefab")) {
        uint32_t ver = 0;
        if (parse_uint(p, ver) && ver == kPrefabVersion) {
          in_header = true;
          continue;
        }
      }
      std::fclose(f);
      return false;
    }

    if (!in_entity) {
      if (match_tok(p, "nesne")) {
        if (parse_quoted_str(p, e.name, sizeof(e.name))) {
          std::memcpy(out_prefab.name, e.name, sizeof(out_prefab.name));
          in_entity = true;
          continue;
        }
      }
      continue;
    }

    // Varlık içi tanımlar
    if (match_tok(p, "son")) {
      break;
    } else if (match_tok(p, "konum")) {
      parse_vec3(p, e.pos);
    } else if (match_tok(p, "donus")) {
      parse_vec3(p, e.rot_deg);
    } else if (match_tok(p, "olcek")) {
      parse_vec3(p, e.scale);
    } else if (match_tok(p, "bayrak")) {
      parse_uint(p, e.flags);
    } else if (match_tok(p, "model")) {
      if (parse_int(p, e.asset) && parse_vec3(p, e.tint)) {
        e.components |= kSceneModel;
      }
    } else if (match_tok(p, "ilkel")) {
      parse_int(p, e.primitive);
    } else if (match_tok(p, "malzeme")) {
      parse_float(p, e.metallic);
      parse_float(p, e.roughness);
      parse_float(p, e.reflectance);
      parse_vec3(p, e.emissive);
      parse_float(p, e.emissive_strength);
    } else if (match_tok(p, "animasyon")) {
      if (parse_uint(p, e.clip) && parse_float(p, e.phase) && parse_float(p, e.speed)) {
        e.components |= kSceneAnim;
      }
    } else if (match_tok(p, "isik")) {
      if (parse_vec3(p, e.light_color) && parse_float(p, e.light_intensity) && parse_float(p, e.light_radius)) {
        e.components |= kSceneLight;
        if (match_tok(p, "yonlu")) e.light_type = SceneLightType::Directional;
        else if (match_tok(p, "spot")) e.light_type = SceneLightType::Spot;
        else if (match_tok(p, "alan")) e.light_type = SceneLightType::Rect;
        else if (match_tok(p, "tup")) e.light_type = SceneLightType::Capsule;
        else if (match_tok(p, "disk")) e.light_type = SceneLightType::Disk;
        else e.light_type = SceneLightType::Point;
      }
    } else if (match_tok(p, "spot_koni")) {
      parse_float(p, e.light_spot_inner);
      parse_float(p, e.light_spot_outer);
    } else if (match_tok(p, "isik_boyut")) {
      parse_float(p, e.light_width);
      parse_float(p, e.light_height);
    } else if (match_tok(p, "isik_huzmesi")) {
      if (match_tok(p, "acik") || match_tok(p, "etkin") || match_tok(p, "1")) {
        e.light_godray = true;
        parse_float(p, e.light_godray_intensity);
      } else {
        e.light_godray = false;
      }
    } else if (match_tok(p, "govde")) {
      e.components |= kSceneBody;
      if (match_tok(p, "kutu")) {
        e.shape = SceneShape::Box;
        parse_vec3(p, e.half);
      } else if (match_tok(p, "kure")) {
        e.shape = SceneShape::Sphere;
        parse_float(p, e.radius);
      }
      if (match_tok(p, "dinamik")) e.dynamic = true;
      else if (match_tok(p, "sabit")) e.dynamic = false;
    } else if (match_tok(p, "kamera")) {
      if (parse_float(p, e.cam_fov) && parse_float(p, e.cam_near) && parse_float(p, e.cam_far)) {
        e.components |= kSceneCamera;
      }
    } else if (match_tok(p, "ses")) {
      if (parse_quoted_str(p, e.audio_clip, sizeof(e.audio_clip)) &&
          parse_float(p, e.audio_volume) && parse_float(p, e.audio_pitch)) {
        e.components |= kSceneAudio;
        e.audio_loop = match_tok(p, "dongu");
        e.audio_spatial = match_tok(p, "uzamsal");
      }
    } else if (match_tok(p, "betik")) {
      if (parse_quoted_str(p, e.script_file, sizeof(e.script_file))) {
        e.components |= kSceneScript;
        e.script_enabled = match_tok(p, "etkin");
      }
    } else if (match_tok(p, "karakter")) {
      if (parse_float(p, e.char_radius) && parse_float(p, e.char_height) &&
          parse_float(p, e.char_mass) && parse_float(p, e.char_max_slope)) {
        e.components |= kSceneCharacter;
      }
    } else if (match_tok(p, "partikul")) {
      if (parse_float(p, e.particle_spawn_rate) && parse_float(p, e.particle_lifetime_min) &&
          parse_float(p, e.particle_lifetime_max) && parse_float(p, e.particle_size_start) &&
          parse_float(p, e.particle_size_end) && parse_vec3(p, e.particle_velocity) &&
          parse_vec3(p, e.particle_jitter)) {
        e.components |= kSceneParticle;
      }
    } else if (match_tok(p, "partikul_renk")) {
      parse_vec3(p, e.particle_color_start);
      parse_vec3(p, e.particle_color_end);
    } else if (match_tok(p, "partikul_fizik")) {
      parse_float(p, e.particle_gravity);
      parse_uint(p, e.particle_billboard_type);
    } else if (match_tok(p, "arazi")) {
      if (parse_float(p, e.terrain_width) && parse_float(p, e.terrain_height) &&
          parse_float(p, e.terrain_cell) && parse_float(p, e.terrain_amp) &&
          parse_float(p, e.terrain_freq) && parse_int(p, e.terrain_octaves) &&
          parse_uint(p, e.terrain_seed)) {
        e.components |= kSceneTerrain;
      }
    } else if (match_tok(p, "su")) {
      if (parse_float(p, e.wave_length) && parse_float(p, e.wave_amplitude) &&
          parse_float(p, e.wave_steepness) && parse_float(p, e.wave_speed) &&
          parse_vec2(p, e.wave_direction)) {
        e.components |= kSceneWater;
      }
    } else if (match_tok(p, "ruzgar")) {
      if (parse_vec2(p, e.wind_direction) && parse_float(p, e.wind_strength) &&
          parse_float(p, e.wind_gustiness) && parse_float(p, e.wind_gust_freq) &&
          parse_uint(p, e.wind_seed)) {
        e.components |= kSceneWind;
      }
    } else if (match_tok(p, "voksel")) {
      if (parse_uint(p, e.voxel_size_x) && parse_uint(p, e.voxel_size_y) &&
          parse_uint(p, e.voxel_size_z) && parse_float(p, e.voxel_cell)) {
        e.components |= kSceneVoxel;
      }
    } else if (match_tok(p, "eklem")) {
      if (parse_int(p, e.joint_target) && parse_vec3(p, e.joint_axis) &&
          parse_float(p, e.joint_limit_min) && parse_float(p, e.joint_limit_max) &&
          parse_float(p, e.joint_motor_speed)) {
        e.components |= kSceneJoint;
      }
    } else if (match_tok(p, "yansima_sondasi")) {
      if (parse_float(p, e.ref_probe_radius) && parse_float(p, e.ref_probe_intensity)) {
        e.components |= kSceneRefProbe;
      }
    } else if (match_tok(p, "yanki")) {
      if (parse_float(p, e.reverb_decay) && parse_float(p, e.reverb_room_size)) {
        e.components |= kSceneReverb;
      }
    } else if (match_tok(p, "saglik")) {
      if (parse_float(p, e.health_max) && parse_float(p, e.health_current)) {
        e.components |= kSceneHealth;
      }
    } else if (match_tok(p, "yetenek")) {
      if (parse_uint(p, e.ability_id) && parse_float(p, e.ability_damage) &&
          parse_float(p, e.ability_range) && parse_float(p, e.ability_cooldown)) {
        e.components |= kSceneAbility;
      }
    } else if (match_tok(p, "navagent")) {
      if (parse_vec3(p, e.ai_target) && parse_float(p, e.ai_speed) && parse_float(p, e.ai_turn_speed)) {
        e.components |= kSceneNavAgent;
      }
    }
  }

  std::fclose(f);
  return in_entity;
}

int32_t prefab_instantiate(SceneDesc &scene, const PrefabData &prefab, Vec3 spawn_pos) {
  if (scene.entity_count >= kSceneMaxEntities) return -1;
  SceneEntity new_e = prefab.entity;
  new_e.pos = new_e.pos + spawn_pos;
  new_e.parent = -1;

  char unique_name[kSceneNameLen];
  std::snprintf(unique_name, sizeof(unique_name), "%s_%u",
                prefab.name[0] ? prefab.name : "Varlik",
                scene.entity_count + 1);
  std::memcpy(new_e.name, unique_name, sizeof(new_e.name));

  const uint32_t at = scene.entity_count;
  if (!scene.insert_entity(at, new_e)) return -1;
  return static_cast<int32_t>(at);
}

} // namespace tulpar::engine::content
