# Usta Plan — boşluk taraması

> **Depo notu.** Bu belge motor deposuna taşındı; ağaç yolları artık `engine/` öneksiz
> (`core/…`, `rhi/…`, `tools/…`). Metinde geçen `src/`, `lib/*.tpr`, `runtime/`, `examples/`,
> `android/host/`, `build.sh`, `./tulpar` ve `docs/mindmap/` **derleyici deposundadır**
> ([hamer1818/TulparLang](https://github.com/hamer1818/TulparLang)) — olduğu gibi bırakıldı.

**Tarih:** 2026-09-19 · **Yöntem:** 6 envanter ajanı + 30 çürütme ajanı, 117 bileşen,
her iddia dosya yolu kanıtıyla; "VAR" diyen ve çelişki işaretleyen iddialar ayrıca
çürütülmeye çalışıldı. **Kod yazılmadı, yalnızca ölçüldü.**

Bu belge dışarıdan gelen "TulparLang Engine — Usta Plan"ı (9 faz, ~60 bileşen)
ağacın **bugünkü** haline karşı ölçer. Amaç plan yazmak değil; **nereden
başlanacağını** dürüstçe söylemek.

## 0. Tek paragraflık cevap

Planın Faz 0/1/2/5/6'sının büyük bölümü **zaten yazılmış ve kapıları var** —
"Faz 0.1 Job System"den başlamak var olanı yeniden yazmak olurdu. Planın
gerçekten yeni olan çekirdeği (QVG) ise deponun **kendi kilitli kararlarıyla**
çarpışıyor. Buna karşılık taramanın en değerli bulgusu şu: **sürekli küme LOD'u
baştan sona yazılmış, test edilmiş ve çizim yoluna hiç bağlanmamış.** Planın
manşeti olan "3M poligonu 10K gibi çiz" yeteneğinin çekirdeği ağaçta duruyor,
yalnızca kablosu takılmamış.

Sayılar: **24 VAR, 49 KISMİ, 44 YOK.** Çürütme turu 6 iddiayı düşürdü ve
**altısı da "fazla iyimser" yönündeydi** — yani envanterin kendisi bile gerçeğin
üstünde çıktı.

## 1. Kilitli kararlarla çarpışan beş madde

`docs/PLAN.md` §8 başlığı **"Kilitli Kararlar (tartışmaya açılmayacaklar)"**.
Planın şu maddeleri doğrudan oraya çarpıyor:

| Plan maddesi | Çarptığı karar | Kararın gerekçesi |
|---|---|---|
| **4.3 `sw_raster`** | §8/2 "Software rasterizer yok" | "TBDR mimarisiyle uyumsuz." Ayrıca `PLAN.md:874` int64 image atomic şartını "attığımız kararı arka kapıdan geri sokmak" diye reddediyor |
| **5.1 Kuantum Alan GI** | §8/3 "Realtime GI yok" | "Watt bütçesi yok. Bake + dinamik katman." |
| **5.6 Gaussian Splatting** | `PLAN.md:78`, `:772` "ATLA" | "capture tabanlı iş yapmıyoruz" |
| **9.5 Neural compression** | `PLAN.md:77`, `:771` "ATLA" | "Mobil NPU/GPU entegrasyonu olgun değil" |
| **RHI WebGPU arka ucu** | `PLAN.md:733` | "web hedef DEĞİL (karar 2026-09-13)" |

`sw_raster`'ın düşmesi önemli, çünkü planda QVG'nin **mobil** hikâyesi ona
dayanıyor. Karar teknik olarak da sağlam: tile-based deferred mobil GPU'da
compute tabanlı yazılım rasterizasyonu mimariyle kavga eder.

**Bunlar açılabilir — ama açılması senin kararın.** Kilitli kararı sessizce
delmek, projenin kendi disiplinini bozar.

## 2. Grover ve QUBO — ölçülen sonuç: kazanç yok

Plan bunları "rakiplerde olmayan" özellik diye sayıyor. Ölçüm şunu söylüyor:

* **Grover:** klasik donanımda benzetimi, elediği doğrusal taramadan **kat kat
  pahalı**. Üstelik planın kendi tasarımındaki "klasik doğrulama" adımı doğrusal
  maliyeti zaten geri yüklüyor. Net kazanç negatif.
* **QUBO sürekli LOD:** çözdüğü problemi monoton hata + iki taraflı eşitsizlik
  **zaten TAM ve O(N)** çözüyor — `renderer/cluster_lod.hpp:93-99`. Yani yerine
  geçeceği şey bir yaklaşıklama değil, kesin çözüm.

Proje bu dersi bir kez ödemiş: `DEVRIMSEL-YOL-HARITASI.md` §3, daha önce eklenen
`qpp` bağımlılığının "bir kuantum kapısı uygulayıp **sonucu attığını**" yazıyor;
`sim/quantum_gen.hpp:10-14` bu kararı kayıt altına almış. Planın kendi 1.
altın kuralı da ("kuantum esinli = dürüst algoritma, pazarlama değil") aynı yere
çıkıyor.

## 3. En değerli bulgu: sürekli LOD yazılmış ama bağlanmamış

**4.1 Cluster DAG / sürekli LOD — kod VAR, çizim yoluna BAĞLI DEĞİL.**

Ağaçta duran ve test edilen: iki bağımsız DAG kurucusu, `.clod` ve `.sahneb`
formatları, ekran-hatası cut kuralı, CPU referans uygulaması ve
`cluster_cull.comp` shader'ı.

Bağlı olmadığının kanıtı (bağımsız doğrulandı, envanter ajanının ifadesi bir
noktada yanlıştı ve burada düzeltildi):

* Modüller **derleniyor**: `renderer/cluster_lod.cpp` + `renderer/cluster_cull.cpp`
  → `engine_renderer` (`CMakeLists.txt:214`), `content/clod_format.cpp`
  → `engine_content` (`:224`). Yani ölü kod değil.
* Testleri **var ve geçiyor**: `test_cluster_lod.cpp`, `test_cluster_cull.cpp`,
  `test_clod_format.cpp`.
* Kendi dosyaları ve testleri dışında bunları çağıran **tek yer**
  `tools/clodbake.cpp` — yani **çevrimdışı bake aracı**.
* Çalışma zamanı çizim yolu haberdar değil: `scene_runtime.cpp`,
  `renderer.cpp`, `editor_app.cpp`, `demo_scene.cpp` → **dördünde de 0 referans**.

> Envanter ajanı "`cluster_cull_comp_spv.h` hiçbir `.cpp`'ye include edilmiyor"
> demişti; bu **yanlış** — `renderer/cluster_cull.hpp` onu include ediyor.
> Doğru ifade yukarıdaki: shader ve modül yerinde, eksik olan **çizim yolunda
> tüketici**.

Yani planın manşeti için gereken şeyin çoğu yazılmış durumda ve kilitli hiçbir
kararı çiğnemiyor — sürekli LOD, yazılım rasterizasyonu **değildir**.

### Uyarı: depoda "LOD" adını taşıyan dört ayrı şey var

Karıştırılmaları kolay ve tarama sırasında bir ajan bunu gerçekten karıştırdı:

| Dosya | Ne yapar | Çizime bağlı mı |
|---|---|---|
| `renderer/cluster.cpp` | **Kümelenmiş ışık** (DAG değil) | evet |
| `indirect_cull` + `cull.comp` | Örnek bazlı frustum + mesafe LOD'u (Sascha Willems portu) | evet |
| `content/lod_select.hpp` | Tüm-mesh geleneksel / dithered LOD | evet |
| `cluster_lod` / `cluster_dag` | **Gerçek sürekli LOD** | **hayır** |

## 4. Planın atladığı ve deponun P0 saydığı madde

`DEVRIMSEL-YOL-HARITASI.md` birinci önceliği **sabit noktalı (fixed-point)
matematik** olarak koymuş. Planın hiçbir fazında geçmiyor.

Ve işin tuhafı: **tip ağaçta zaten yazılı** — `core/math/fixed.hpp`
(Q16.16, 143 satır) + `test_fixed.cpp` (131 satır) — ama grep'e göre **test
dışında hiçbir yerde kullanılmıyor.**

Sonuç: determinizm hâlâ tamamen **disipline** dayanıyor (`PLAN.md:585`'teki beş
şartlı sözleşme ve tek nöbetçi `physics_cross_platform_golden_hash`). İkinci
maliyeti de şu: güvenli NEON vektörleştirme kilitli kalıyor —
Motor ağacında `arm_neon.h` / `immintrin.h` → **0 isabet**.

## 5. Determinizmi tehdit eden plan maddeleri

Motorun ana iddiası bit-eş determinizm. Şu maddeler onu riske atıyor:

* **`parallel_for`** (1.1): bölüm sayısı worker sayısına bağlanırsa float toplama
  sırası değişir; float toplama birleşmeli değildir → altın özet hash'leri kırılır.
  Eklenecekse bölümleme worker sayısından **bağımsız** ve indirgeme **sabit
  sırada** olmalı.
* **Varlık akıtıcısı** (1.5): yükleme tamamlanma anı sim'e sızarsa 600-tick altın
  hash kırılır.
* **FSR/XeSS** (9.1): çalışma zamanı SIMD seçimi — `DEVRIMSEL-YOL-HARITASI.md` §3
  `ncnn`/`FastNoiseSIMD`'i tam bu sebeple reddetmiş.

## 6. "Her şey lokal, API yok" kısıtının sonucu

**Elenir:** D-Wave, bulut LLM, DLSS, XeSS, dört mağaza API'si, telemetry
dashboard, macOS notarization, iOS/App Store zinciri.

**Yerel muadili var:**

| Plan maddesi | Yerel karşılığı |
|---|---|
| FSR2 / Arm ASR | MIT, vendor edilebilir — **ama varsayılan kapalı ve kapalıyken bit-aynı olmalı** |
| Bulut kayıt | `teng_save_*` yerel deposu |
| Achievement | aynı yerel depo |
| Crash + perf telemetrisi | `crash.cpp` + `symbolize.py` + profiler Chrome trace + `paket_boyut_audit.py` |
| Uzak liderlik tablosu | `lib/arcade.tpr:344` deseni: varsayılan boş URL, boşsa hiç ağ yok |

Tek canlı dış bağımlılık `tulpar update` ve o bir geliştirici aracı; yerel
arşivden kurulumla kapatılabilir.

## 7. Diğer gerçek boşluklar (kilitli kararla ilgisi olmayan)

* **Motor çapında log modülü YOK.** `find engine -name "*log*"` → sıfır dosya.
  Loglama 12+ modülde ham `printf`; seviye sistemi yalnızca köprüye özel.
* **RHI yalnız Vulkan.** Metal / DX12 / WebGPU sıfır. Dahası ortak arka uç
  arayüzü de yok: `Vk*` tipleri `rhi/device.hpp`'nin public yüzeyinde ve L3
  renderer, L6 content, app, bridge katmanlarına sızıyor. İkinci bir arka uç
  "backend ekle" değil, **"önce soyutlamayı çıkar"** işi.
* **Job sisteminde** lock-free MPMC kuyruk ve `parallel_for` yok (kuyruk bilinçli
  olarak spinlock'lu halka; `FAZ0.md` "Faz 1'e devreden, ÖLÇÜLMEDEN DEĞİŞTİRİLMEZ"
  diyor).
* **Sanitizer otomasyonu sıfır**; CI matrisi beş platformdan ikisini kapsıyor.
* **Font 213 glifle sabit** (ASCII + Latin-1 + Türkçe); shaping / RTL / çoğul yok.

## 8. Önerilen başlangıç sırası

Kilitli hiçbir kararı çiğnemeyen, ölçülebilir ve planın manşetine hizmet eden sıra:

1. **Sürekli LOD'u çizim yoluna bağla** (§3). Kod ve testler hazır; iş kablolama.
   Kapı: aynı sahnede üçgen sayısı ve kare süresi, LOD açık/kapalı ölçümü.
2. **Sabit noktalı matematiği benimset** (§4). Tip hazır, kullanılmıyor. Önce
   sim'in bir alt sistemi, sonra genişlet. Kapı: mevcut altın hash korunmalı.
3. **NEON'u aç** — (2) bittiğinde güvenli hale gelir. Kapı: bit-eşlik + hız ölçümü.
4. **Log modülü** — ucuz, her şeyi kolaylaştırır.
5. Bundan sonrası kilitli kararların açılıp açılmayacağına bağlı.

## 9. Bu belgenin sınırı

Tarama **statik**: kod okudu, grep'ledi, derleme hedeflerini kontrol etti.
Performans iddialarını (örn. "1M entity'de 60 FPS") **ölçmedi**. Bir bileşen
"VAR" diyorsa "kodu ve kapısı var" demektir; "planın vaat ettiği hızda çalışıyor"
demek değildir.
