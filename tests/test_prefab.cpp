// L6 CONTENT — Prefab Sistemi Birim Testleri (test_prefab.cpp)
// O3DE ve Prowl tarzı prefab kaydetme, yükleme ve sahneye yerleştirme doğrulaması.

#include "content/prefab.hpp"
#include "content/scene.hpp"
#include "tests/test.hpp"
#include <cstdio>
#include <cstring>

using namespace tulpar::engine;
using namespace tulpar::engine::content;

ENGINE_TEST(prefab_save_and_load_roundtrip) {
  const char *test_path = "tests_scratch_lamp.prefab";

  SceneEntity src{};
  std::snprintf(src.name, sizeof(src.name), "SokakLambasi");
  src.pos = Vec3{10.0f, 2.0f, -5.0f};
  src.rot_deg = Vec3{0.0f, 45.0f, 0.0f};
  src.scale = Vec3{1.2f, 1.2f, 1.2f};
  src.components = kSceneLight | kSceneBody;

  // Işık alanları
  src.light_type = SceneLightType::Spot;
  src.light_color = Vec3{1.0f, 0.85f, 0.6f};
  src.light_intensity = 35.0f;
  src.light_radius = 12.5f;
  src.light_spot_inner = 20.0f;
  src.light_spot_outer = 45.0f;
  src.light_godray = true;
  src.light_godray_intensity = 1.8f;

  // Gövde alanları
  src.shape = SceneShape::Box;
  src.half = Vec3{0.3f, 2.5f, 0.3f};
  src.dynamic = false;

  // 1. Kaydet
  bool save_ok = prefab_save(src, test_path);
  CHECK(save_ok == true);

  // 2. Yükle
  PrefabData loaded{};
  bool load_ok = prefab_load(test_path, loaded);
  CHECK(load_ok == true);
  CHECK(std::strcmp(loaded.name, "SokakLambasi") == 0);

  // 3. Veri doğrulaması
  CHECK((loaded.entity.components & kSceneLight) != 0);
  CHECK((loaded.entity.components & kSceneBody) != 0);
  CHECK(loaded.entity.light_type == SceneLightType::Spot);
  CHECK(loaded.entity.light_intensity == 35.0f);
  CHECK(loaded.entity.light_godray == true);
  CHECK(loaded.entity.light_godray_intensity == 1.8f);
  CHECK(loaded.entity.shape == SceneShape::Box);
  CHECK(loaded.entity.half.y == 2.5f);

  // Temizlik
  std::remove(test_path);
}

ENGINE_TEST(prefab_instantiate_into_scene) {
  PrefabData prefab{};
  std::snprintf(prefab.name, sizeof(prefab.name), "Robot");
  prefab.entity.pos = Vec3{0, 0, 0};
  prefab.entity.components = kSceneCharacter | kSceneHealth;
  prefab.entity.char_radius = 0.6f;
  prefab.entity.char_height = 1.8f;
  prefab.entity.health_max = 250.0f;
  prefab.entity.health_current = 250.0f;

  SceneDesc scene{};
  Vec3 spawn_at{15.0f, 0.0f, 30.0f};

  int32_t idx = prefab_instantiate(scene, prefab, spawn_at);
  CHECK(idx == 0);
  CHECK(scene.entity_count == 1);
  CHECK(scene.entities[0].pos.x == 15.0f);
  CHECK(scene.entities[0].pos.y == 0.0f);
  CHECK(scene.entities[0].pos.z == 30.0f);
  CHECK(scene.entities[0].char_height == 1.8f);
  CHECK(scene.entities[0].health_max == 250.0f);
}
