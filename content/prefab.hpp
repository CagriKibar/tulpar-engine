// L6 CONTENT — TulparEngine Yerel Prefab (Ön Tanımlı Varlık) Sistemi (prefab.hpp)
//
// ESİNLENME VE MİMARİ:
// 1. O3DE Prefab System (Code/Framework/AzToolsFramework/AzToolsFramework/Prefab/)
//    Entity ve bileşen ağaçlarının yeniden kullanılabilir şablonlar (template/prefab)
//    halinde diskte saklanması ve sahneye çoklu örnek (instantiate) olarak bırakılması.
// 2. Prowl Game Engine (Prowl.Runtime/Prefab.cs & GameObject.Instantiate)
//    Bileşen metaverilerini ve yerel öteleme (spawn offset) mantığını koruyan
//    hafif ve doğrudan varlık kopyalama disiplini.
//
// SIFIR TAHSİS VE SIFIR HARİCİ APİ SÖZLEŞMESİ:
// - Hiçbir 3. parti JSON/YAML kütüphanesi veya harici API içermez.
// - Tulpar'ın deterministik satir-tabanli ASCII formatını (.prefab) kullanır.
// - Çalışma zamanında sıfır dinamik heap ayırması (zero-alloc).

#pragma once

#include <cstddef>
#include <cstdint>
#include "content/scene.hpp"
#include "core/math/vec.hpp"

namespace tulpar::engine::content {

constexpr uint32_t kPrefabVersion = 1;

struct PrefabData {
  char name[kSceneNameLen] = {0};
  SceneEntity entity{};
};

// Varlığı ve tüm aktif bileşen alanlarını .prefab dosyasına deterministik metin olarak kaydeder.
bool prefab_save(const SceneEntity &entity, const char *filepath);

// Diskteki .prefab dosyasını okur ve PrefabData yapısına ayrıştırır.
bool prefab_load(const char *filepath, PrefabData &out_prefab);

// Prefab verisini sahneye yeni bir varlık olarak ekler, konumunu `spawn_pos` ile öteler.
// Eklenen yeni varlığın indeksini döndürür; sahne doluysa veya hata varsa -1 döner.
int32_t prefab_instantiate(SceneDesc &scene, const PrefabData &prefab, Vec3 spawn_pos);

} // namespace tulpar::engine::content
