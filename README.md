# Tulpar Engine

Mobil öncelikli (ARM + Vulkan), tek stüdyoya özel C++17 oyun motoru. Genel amaçlı
değil: kendi oyunlarımız için yazılıyor. Bu depo **motorun kendisidir** — TulparLang
derleyicisinden [`git subtree split`](https://github.com/hamer1818/TulparLang) ile
ayrıldı, geçmişi korunarak.

Plan ve ölçümler `docs/` altında: [PLAN.md](docs/PLAN.md) (mimari),
[VIZYON.md](docs/VIZYON.md), [DURUM.md](docs/DURUM.md) (tek sayfa "ne var, ne ölçüldü,
ne yok"), faz raporları [FAZ0](docs/FAZ0.md)–[FAZ8](docs/FAZ8.md),
cihaz matrisi [CIHAZ-MATRISI.md](docs/CIHAZ-MATRISI.md),
Tulpar köprüsü [KOPRU.md](docs/KOPRU.md).

## Ağaç

```
platform/   L0  fatal, zaman, OS bellek, iş parçacığı, pencere (GLFW dlopen), çökme raporu
core/       L1  Arena ailesi + Pool<T>, fiber iş sistemi (x86_64/AArch64 asm), profiler,
                konteynerler, matematik, BVH/spatial hash, EBR
rhi/        L2  Vulkan (loader dlopen'lı — link zamanı bağımlılık yok), swapchain,
                pipeline cache, karo bütçesi, shader'lar (GLSL -> depoya giren *_spv.h)
renderer/   L3  forward Lambert, gölge atlası (3 kademe), bloom, cluster/LOD, derlenmiş
                render grafiği (geçişler veri, kod değil)
audio/      L3  kilitsiz SPSC mixer (32 ses), miniaudio cihazı
sim/        L4  ECS, çizelgeleyici, Jolt fiziği, Recast/Detour navmesh, animasyon
content/    L6  glTF 2.0 (cgltf+stb), KTX2/ASTC, sahne veri modeli (.sahne) ve blob (.sahneb)
bridge/     L6  Tulpar köprüsü: düz skaler `teng_*` C ABI + masaüstü/Android host
app/        L6  birleştirme kökü: engine_demo, engine_editor (ImGui+ImGuizmo)
tests/          engine_tests — tek ikili, bütün kapılar
tools/          layer_check.py (ihlal = derleme hatası), compile_shaders.py,
                gen_engine_bindings.py, texpack/sahnec/clodbake, android_run.sh, ...
third_party/    vendored: Vulkan başlıkları, Jolt, Recast, meshoptimizer, cgltf, stb,
                miniaudio, astcenc, ImGui, Tracy, GLFW başlıkları
```

## Derleme

Gerek duyulanlar: **CMake 3.14+**, C++17 derleyici (GCC ya da Clang), **Ninja** (ya da
make), **python3** (katman denetimi ve shader araçları için). Vulkan SDK **gerekmez** —
başlıklar vendored, loader çalışma zamanında `dlopen` ediliyor.

```bash
cmake -S . -B yapi -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build yapi -j
```

Windows'ta aynı komutlar **MSYS2 MINGW64** kabuğunda koşar. **MSVC desteklenmiyor**:
fiber geçişi GNU sözdizimli `.S` dosyası (Win64 dalı MinGW için yazılı), derleme
bayrakları da GCC/Clang yazımında.

```bash
pacman -S --needed mingw-w64-x86_64-{gcc,cmake,ninja,python}
```

Android çapraz derlemesi üst ağaç istemez:

```bash
cmake -S . -B build-android -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26
```

Seçenekler: `ENGINE_MEM_CANARY` (arena taşma kanaryaları, varsayılan ON — ship'te kapat),
`ENGINE_TRACY` (Tracy istemcisi, varsayılan OFF), `ENGINE_SWAPPY` (Android kare temposu).

### Ne çıkıyor

| hedef | ne |
|---|---|
| `engine_tests` | bütün kapılar tek ikilide |
| `engine_demo` | uygulama; `--headless N --out kare.ppm` ile penceresiz doğrulanır |
| `engine_editor` | sahne editörü (ImGui + ImGuizmo); `--headless N --out kare.ppm` |
| `engine_sahnec` | `.sahne` → `.sahneb` derleyicisi (`--check`, `--dump`, `--kanonik`) |
| `engine_texpack` | PNG → ASTC mip zinciri → `.ktx2` |
| `engine_clodbake` | çevrimdışı cluster/LOD bake |

## Test

```bash
./yapi/engine_tests            # hepsi
./yapi/engine_tests rhi        # ada göre süzülür (alt dize)
```

Özet satırı şöyle biter: `engine tests: N passed, 0 failed, M atlandi (N/N kosuldu)`.

**KOŞMAYAN KAPI YEŞİL DEĞİLDİR.** Donanım ya da araç yoksa test sessizce `return`
etmez; `ATLANDI: <sebep>` basar ve özet satırındaki sayaca girer. Bu yüzden:

* `M` (atlandı) sayısını **oku**. Vulkan loader yoksa bütün RHI/renderer/içerik
  kapıları atlanır ve geriye kalan "passed" sayısı GPU hakkında hiçbir şey söylemez.
* `TULPAR_ENGINE_NO_VULKAN=1` bu yolu zorlar — atlama mekanizmasının pozitif kontrolü.
* CI (`.github/workflows/ci.yml`) Ubuntu'da **lavapipe** kurar ve Vulkan yolunun
  gerçekten koştuğunu doğrular: "Vulkan loader yok" gerekçeli bir atlama görürse
  işi **kırmızıya** çevirir.
* Zamanlama satırları (`[profiler]`, `[bilgi]`) bilgi basar, karar vermez.

Penceresiz doğrulama kuralı: **doğrulamak için pencere açma.** Demo ve editör
`--headless N --out x.ppm` ile aynı boru hattını offscreen koşturur; pencereli yolu
yalnız kullanıcı çalıştırır.

## Katman kuralı — sözleşme değil, mekanizma

`tools/layer_check.py` `engine_core`'un ön koşuludur: ihlal = **derleme hatası**.

1. Bir katman yalnız **altındaki** katmanları `#include` eder. Yukarı çağrı yok.
   `platform`=L0, `core`=L1, `rhi`=L2, `renderer`/`audio`=L3, `sim`=L4, `gameplay`=L5,
   `content`/`app`/`bridge`=L6, `tools`=L7; `tests/` her şeyi görür.
2. **STL konteyneri yok** (`vector`, `string`, `map`, `memory`, `functional`, …) —
   **testler dahil**. Test kodunda `vector` kullanmak "kare içinde 0 ayırma"
   iddiasını gizlerdi.
3. `-fno-exceptions -fno-rtti`; kare içinde global `new` yok (`AllocGate` sayar,
   Faz 0 kapısı pozitif kontrolle 0 ayırma iddia eder).

## TulparLang ile ilişkisi

Motor C++ kalır, **oyun betikleri Tulpar'da yazılır**. Bağlantı `bridge/`:

* `bridge/engine_api.h` — `teng_*`: **düz skaler** C ABI (struct yok, callback yok;
  Tulpar'ın bugünkü FFI'ının taşıdığı tek şekil). Çok değerli sorgu "hesapla sonra oku"
  kalıbıyla, çarpışma ise **kuyrukla** verilir — callback olmadığı için.
* `bridge/desktop_host.cpp` / `android_host.cpp` — pencere/yüzey/girdi (`BridgeHost`).
* `tools/gen_engine_bindings.py` içindeki `SPEC` tablosu **tek kaynaktır**: tek komutla
  derleyici tarafındaki dört dosyayı birden üretir. Elle tutulan nokta olmadığı için
  bağlama noktaları birbirinden kayamaz.

Derleyicinin kendisi — `src/`, `lib/engine.tpr` sarmalayıcısı, `runtime/engine_bindings.cpp`,
`examples/engine_*.tpr` — **ayrı depodadır**: <https://github.com/hamer1818/TulparLang>.
`docs/` içindeki belgelerde bu yollar olduğu gibi bırakıldı; hangi depoda oldukları her
belgenin başındaki "Depo notu" kutusunda yazıyor.

Sözleşmenin tamamı: [docs/KOPRU.md](docs/KOPRU.md).

## Bilinen boşluk

`tools/` altındaki Android/Tracy kabuk betikleri (`android_run.sh`,
`build_bridge_android.sh`, `fetch_vvl_android.sh`, `fetch_swappy.sh`, `tracy_check.sh`,
`clang_syntax_check.sh`) hâlâ eski tek-depo yerleşimini varsayıyor: kök iki dizin yukarıda
ve yollar `engine/` ön ekli. Bu depoda tek başına koşmazlar; ayrı bir düzeltme istiyorlar.
CMake derlemesi, `engine_tests` ve `tools/layer_check.py` bundan etkilenmez.

## Lisans

Motor kaynağı TulparLang projesinin lisansına tabidir. `third_party/` altındaki
vendored kütüphaneler kendi lisanslarıyla gelir (Jolt MIT, Recast zlib, meshoptimizer MIT,
cgltf MIT, stb public domain/MIT, miniaudio MIT/public domain, astc-encoder Apache-2.0,
ImGui MIT, Tracy BSD-3, Vulkan başlıkları Apache-2.0, `assets/fonts/DejaVuSans.ttf`
Bitstream Vera).
