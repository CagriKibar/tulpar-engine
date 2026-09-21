# TULPARENGINE — PARTİKÜL VE VFX SİSTEMİ MİMARİSİ
## Endüstri İncelemesi, Tasarım Felsefeleri ve Yeni Nesil Hibrit VFX Spesifikasyonu

> **Doküman Türü:** Çekirdek VFX Mimarisi, Açık Kaynak Motor Analizi, UX Akışı ve Uygulama Spesifikasyonu  
> **Konum:** `docs/PARTIKUL_VE_VFX_MIMARISI.md`  
> **Hazırlayan:** TulparEngine Baş Mimarı & VFX / Render Çalışma Grubu  
> **Kapsanan Motorlar:** Unreal Engine Niagara, Unity VFX Graph, Godot Engine (GPUParticles3D / CPUParticles3D), Valve Source 2, CryEngine (SecondGen), Bevy (bevy_hanabi), O3DE (OpenParticleSystem), Flax Engine, Roblox, GameMaker, Stride, Panda3D, jMonkeyEngine (Jme-VFX)  
> **Çekirdek İlkeler:** Sıfır Dinamik Tahsis (0 alloc/frame), Deterministik Simülasyon (Replay & Rollback Uyumlu), Vulkan 1.1 / Mobil TBDR (Mali/Adreno) Dostu, Hibrit Yığın (Stack) + Çizge (Graph) UX.

---

## İÇİNDEKİLER
1. [Giriş ve Stratejik Hedef](#1-giriş-ve-stratejik-hedef)
2. [Temel Felsefe: Üç Büyük Tasarım Kampı](#2-temel-felsefe-üç-büyük-tasarım-kampı)
   * 2.1 Kamp A: "Her Şey Bir Parçacık" (Particle-Centric)
   * 2.2 Kamp B: "Veri Odaklı Simülasyon" (Data-Oriented)
   * 2.3 Kamp C: "Hibrit Sentez" (Stack + Graph)
   * 2.4 Felsefi Karşılaştırma Matrisi
3. [Mimari Desenler: Motor Motor Derin İnceleme ve Dersler](#3-mimari-desenler-motor-motor-derin-i̇nceleme-ve-dersler)
   * 3.1 Unreal Engine Niagara
     * 3.1.1 Niagara Simulation Stages ve Grid2D Data Interface
   * 3.2 Unity VFX Graph
   * 3.3 Godot Engine (GPUParticles3D & CPUParticles3D)
     * 3.3.1 Godot'nun Çift Pipeline Geçiş Mekanizması
   * 3.4 Valve Source 2
   * 3.5 CryEngine (SecondGen & FlowGraph)
   * 3.6 Bevy (`bevy_hanabi`)
   * 3.7 O3DE (`OpenParticleSystem`)
   * 3.8 Flax Engine
   * 3.9 Roblox Engine
   * 3.10 GameMaker
   * 3.11 Stride Engine
   * 3.12 Panda3D
   * 3.13 jMonkeyEngine (Jme-VFX)
   * 3.14 Endüstri Motorları Karşılaştırma Matrisi
4. [Sanatçı Deneyimi ve UX Akışı (Artist UX Workflow)](#4-sanatçı-deneyimi-ve-ux-akışı-artist-ux-workflow)
   * 4.1 Bilişsel 8-Adım İş Akışı
   * 4.2 UX Paradigma Karşılaştırmaları (Niagara vs. Unity vs. Godot)
   * 4.3 TulparEngine Entegre Editör Inspector Tasarımı
5. [Modül Tasarımı ve Veri Sözleşmeleri](#5-modül-tasarımı-ve-veri-sözleşmeleri)
   * 5.1 10 Standart Açık Kaynak VFX Modülü
   * 5.2 Modül Başına Matematiksel Sözleşmeler ve Parametreler
   * 5.3 Grafik Editörü Mimarisi (Node-Based VFX Graph)
6. [Determinizm, Varyasyon ve Ağ Senkronizasyonu (Netcode)](#6-determinizm-varyasyon-ve-ağ-senkronizasyonu-netcode)
   * 6.1 Xorshift32 Deterministik Rastgelelik
   * 6.2 Bit-Identical Doğrulama ve Rollback Netcode Uyumu
     * 6.2.1 Sunum Katmanı Efektleri ve Rollback Ayrımı
   * 6.3 Parametre Eşleme ve Deterministik Varyasyon
7. [Performans Mimarisi ve Mobil TBDR Optimizasyonu](#7-performans-mimarisi-ve-mobil-tbdr-optimizasyonu)
   * 7.1 GPU vs. CPU Simülasyon Ayrımı
     * 7.1.1 Vulkan Compute Pipeline ve SSBO Tasarımı
   * 7.2 Mobil TBDR Darboğazları ve Çeyrek Çözünürlüklü Tampon (Quarter-Res RT)
     * 7.2.1 Ağırlıklı Harmanlanmış Sıra-Bağımsız Şeffaflık (WBOIT) Matematiksel Formülasyonu
     * 7.2.2 TBDR Erken Derinlik Testi ve Transient Attachment Optimizasyonu
   * 7.3 Sıfır Bellek Tahsisi Disiplini (0 Alloc Arena)
   * 7.4 Clustered Forward+ Işık Enjeksiyonu
   * 7.5 Dinamik Bounding Box ve Mesafe Bazlı LOD / Culling
8. [TulparEngine Yeni Nesil Hibrit VFX Mimarisi ve C++ Spesifikasyonu](#8-tulparengine-yeni-nesil-hibrit-vfx-mimarisi-ve-c-spesifikasyonu)
   * 8.1 Çekirdek Veri Yapıları (`ParticleEmitterConfig`)
   * 8.2 İleri Düzey Simülasyon ve Render Bileşenleri
   * 8.3 Genişletilebilir C++ Modül API'si (`IParticleModule`)
9. [Sonuç ve Tasarım Manifestosu](#9-sonuç-ve-tasarım-manifestosu)

---

## 1. GİRİŞ VE STRATEJİK HEDEF

Oyun motorlarında görsel efektler (VFX) ve parçacık sistemleri, sadece ekranda uçuşan kıvılcımlar veya estetik süslemeler değildir. Modern bir oyun motorunda VFX sistemi; **render boru hattı (RHI/TBDR)**, **fizik motoru (Jolt/çarpışma)**, **oynanış mantığı (GAS/hasar)**, **ses alt sistemi (akustik darbe)** ve **ağ senkronizasyonu (rollback/replay)** ile doğrudan kesişen en kritik mimari köprülerden biridir.

Mevcut açık kaynak ekosisteminde parçacık sistemleri çoğunlukla iki aşırı uçta sıkışmıştır:
1. **Aşırı İlkel Sistemler:** Yalnızca sabit 2B kamera yüzlü (billboard) quad spritelar basabilen, fizik veya olay desteği olmayan, CPU tarafında her kare binlerce heap tahsisi yapan yapılar.
2. **Aşırı Ağır / Donanım Seçen Sistemler:** Yalnızca üst düzey masaüstü GPU'larda yüz binlerce parçacığı compute shader ile döndüren; ancak mobil cihazlara (ARM Mali, Qualcomm Adreno) girildiğinde aşırı bellek bant genişliği ve kontrolsüz overdraw nedeniyle cihazı saniyeler içinde aşırı ısıtıp termal darboğaza (thermal throttling) sokan monolitik yapılar.

TulparEngine'in stratejik hedefi; **Unreal Niagara'nın modüler esnekliğini**, **Godot'nun 5 dakikalık erişilebilir basitliğini**, **Bevy ve CryEngine'in veri odaklı (DoD/SoA) saf performansını** tek bir potada sentezleyerek; **mobil cihazlarda 0 FPS düşüşü ve kare başına 0 bayt dinamik bellek tahsisiyle** çalışan endüstri standartlarında bir hibrit VFX ekosistemi inşa etmektir.

---

## 2. TEMEL FELSEFE: ÜÇ BÜYÜK TASARIM KAMPI

Dünya genelinde geliştirilmiş tüm ticari ve açık kaynak parçacık sistemleri üç ana felsefi kamp etrafında şekillenir. TulparEngine'in mimari kaderi, bu kampların zayıflıklarından arındırılmış bir **sentez** kurmasına bağlıdır.

```
                  ┌─────────────────────────────────────────┐
                  │          VFX TASARIM KAMPLARI           │
                  └────────────────────┬────────────────────┘
                                       │
         ┌─────────────────────────────┼─────────────────────────────┐
         ▼                             ▼                             ▼
┌──────────────────┐         ┌──────────────────┐         ┌──────────────────┐
│     KAMP A:      │         │     KAMP B:      │         │     KAMP C:      │
│ Parçacık Merkezli│         │    Veri Odaklı   │         │      HİBRİT      │
│(Particle-Centric)│         │  (Data-Oriented) │         │ (Stack + Graph)  │
├──────────────────┤         ├──────────────────┤         ├──────────────────┤
│• Niagara         │         │• CryEngine       │         │• Niagara         │
│• Unity VFX Graph │         │• Bevy Hanabi     │         │• Godot Material  │
│• Source 2        │         │• O3DE            │         │• TulparEngine    │
│                  │         │                  │         │                  │
│Her parçacık bir  │         │Parçacıklar SoA   │         │Sanatçıya Stack,  │
│Entity'dir; sonsuz│         │dizi satırıdır;   │         │Teknik uzmana     │
│esnek, ızgarada   │         │SIMD/Compute ile  │         │Graph; arkada     │
│yüksek maliyetli. │         │milyonlar akar.   │         │deterministik SoA.│
└──────────────────┘         └──────────────────┘         └──────────────────┘
```

### 2.1. Kamp A: "Her Şey Bir Parçacık" (Particle-Centric)
* **Temsilciler:** Unreal Engine Niagara, Unity VFX Graph, Valve Source 2.
* **Felsefe:** Simülasyonun atomik birimi parçacığın kendisidir. Bir akışkan alanı, ızgara (grid) veya hacimsel sis simüle edilmek istendiğinde, ızgara hücrelerinin her biri bağımsız birer parçacık olarak yaratılır ($256 \times 256 = 65.536$ parçacık). Parçacıklar birer mikro-entity gibi davranır; kendi yaşam döngüleri, öznitelikleri, yerel bellekleri ve komşu etkileşimleri vardır.
* **Güçlü Yanı:** Sınırsız esneklik. Her parçacık bağımsız bir simülasyon hücresi olduğu için akla gelebilecek her fiziksel olay (SPH akışkanlar, sürü zekası, deformasyon, cloth benzeri parçacık ağları) parçacık olarak modellenebilir.
* **Zayıf Yanı:** Bellek ve işlemci verimsizliği. Standart ızgara tabanlı simülasyonlar için her hücreye ayrı parçacık açmak, bellek hizalamasını bozar, aşırı overhead yaratır ve CPU/GPU bellek bant genişliğini tüketir.

### 2.2. Kamp B: "Veri Odaklı Simülasyon" (Data-Oriented Simulation)
* **Temsilciler:** CryEngine, Bevy (`bevy_hanabi`), O3DE (`OpenParticleSystem`).
* **Felsefe:** Parçacıklar birer nesne veya entity değil, bitişik bellek dizilerindeki salt veri satırlarıdır (SoA - Structure of Arrays). CryEngine ve Bevy'de bellek düzeni, CPU L1/L2 önbellek satırlarına ($64\text{ Bayt}$) ve AVX2/NEON vektörel SIMD kanallarına tam oturacak şekilde paketlenir.
* **Güçlü Yanı:** Devasa işleme hızı ve bellek bant genişliği verimliliği. CPU üzerinde tek çekirdekte saniyede yüz binlerce, GPU compute üzerinde milyonlarca parçacık sıfır önbellek kaçırmasıyla (cache miss) simüle edilir.
* **Zayıf Yanı:** Bireysel esneklik kaybı. Bir parçacığın kendi içinde bağımsız bir karar vermesi (karmaşık `if/else` dallanması), GPU SIMD/warp veya CPU vektör akışını parçalar (divergence) ve hız avantajını yok eder.

### 2.3. Kamp C: "Hibrit Sentez" (Stack + Graph)
* **Temsilciler:** Unreal Niagara (Yığın + Çizge), Unity VFX Graph (Context Graph), Godot (ProcessMaterial + VisualShader), TulparEngine.
* **Felsefe:** Kullanıcıya dönük katmanda modüler yığın (stack) ve görsel çizge (node graph) sunulurken; yürütme aşamasında bu bloklar derlenerek veri odaklı (SoA) SIMD veya GPU compute shader koduna dönüştürülür.
* **Güçlü Yanı:** Hem görsel sanatçı dostu (kod yazmadan sürükle-bırak bloklar) hem teknik sanatçı dostu (derin matematik düğümleri) hem de donanım dostudur. Sanatçı karmaşıklıktan korunurken donanım tam hızda çalışır.
* **Zayıf Yanı:** Editör ve ara katman karmaşıklığı. Stack ve Graph'ı derleyip çalışan yürütülebilir koda dönüştüren bir derleyici, AST (Abstract Syntax Tree) veya ara temsil (IR) gerektirir.

### 2.4. Felsefi Karşılaştırma Matrisi

| Kriter | Kamp A (Particle-Centric) | Kamp B (Data-Oriented) | Kamp C (Hibrit - Tulpar) |
| :--- | :--- | :--- | :--- |
| **Atomik Birim** | Bağımsız Entity / Parçacık | Bitişik SoA Veri Satırı | Modüler Blok $\to$ SoA Dizisi |
| **Sanatçı Ergonomisi** | Orta / İleri | Düşük (Teknik/Kod Odaklı) | **En Yüksek (Stack + Graph)** |
| **SIMD / GPU Uyumu** | Düşük (Divergence riski) | Mükemmel (Doğal Vektörel) | **Mükemmel (Derlenmiş SoA)** |
| **Izgara Simülasyonu** | Pahalı ($N$ parçacık tahsisi) | Optimize Voksel Buffer | **Voksel Grid + Parçacık Hibriti** |
| **Determinizm & Replay** | Zor (Karmaşık nesne durumları) | Kolay (Doğrusal array) | **Tam Bit-Identical (Xorshift32)** |
| **Mobil TBDR Uyumu** | Riskli (Overdraw fırtınası) | Yüksek (Az bellek transferi) | **En Yüksek (Quarter-Res RT + 0 Alloc)** |

---

## 3. MİMARİ DESENLER: MOTOR MOTOR DERİN İNCELEME VE DERSLER

Dünyadaki 13 majör oyun motorunun kaynak kodları, mimari dokümanları ve teknik yayınları incelenerek TulparEngine için çıkarılan dersler:

### 3.1. Unreal Engine Niagara: Emitter/System Ayrımı, Simulation Stages & Data Interfaces
* **Mimari Özellikleri:**
  1. **Emitter vs. System:** `Niagara System`, sahneye bırakılan aktördür; bünyesinde birden fazla bağımsız `Niagara Emitter` barındırır. Örneğin bir roket efekti; duman izi emitter'ı, alev küresi emitter'ı ve kıvılcım saçılma emitter'ından oluşur.
  2. **Yürütme Aşamaları (Simulation Stages):** GPU üzerinde parçacık verisini sıralı çok adımlı geçişlerle (multi-pass compute) günceller. İlk aşamada parçacık konumları hesaplanır, ikinci aşamada komşuluk ızgarasına hashlenir, üçüncü aşamada akışkan basınç gradyanı çözülür.
  3. **Data Interfaces & Parameter Map:** Parçacıkların dış dünyayla (iskelet kemikleri, sahne derinliği, ses frekans spektrumu, mesafe alanları) iletişim kurmasını sağlayan soyut C++ köprüleridir.
* **Tulpar İçin Mimari Çıkarım:**
  * Emitter ve System ayrımı, karmaşık görsel efektlerin tek bir bileşen altında yönetilmesi için zorunludur.
  * Simulation Stages mantığı, mobil TBDR mimarilerinde compute-to-raster geçişini tek komut tamponunda paketleyerek senkronizasyon kilitlenmelerini (pipeline bubble) engeller.

#### 3.1.1. Niagara Simulation Stages ve Grid2D Data Interface
Niagara'nın Simulation Stages mimarisi, standart parçacık yürütme akışının (`Emitter Spawn` $\to$ `Emitter Update` $\to$ `Particle Spawn` $\to$ `Particle Update` $\to$ `Render`) arasına **ek yürütme geçişleri (multi-pass stages)** ekler. Her Simulation Stage, ya parçacıklar üzerinde ya da bir **Data Interface** (Grid2D, Grid3D, Neighbor Grid) üzerinde iterasyon yapabilir.

* **Grid2D Akışkan Simülasyonu:** Bir $256 \times 256$ akışkan ızgarası simüle etmek için geleneksel yöntem 65.536 bağımsız parçacık oluşturmayı gerektirirken, Simulation Stage doğrudan ızgara üzerinde çalışır: her hücre bir "element" olarak işlenir ve parçacık başına spawn/ölüm/render overhead'i tamamen ortadan kalkar. Simülasyon sonucu daha sonra çok daha az sayıda görsel parçacıkla örneklenebilir veya doğrudan bir yüzey materyaline beslenebilir.
* **TulparEngine Uyarlaması:** `sim/voxel_smoke.hpp` içindeki `VoxelSmokeGrid`, Niagara'nın Grid2D/Grid3D Data Interface'ine karşılık gelir. Ancak Tulpar, bu ızgarayı **parçacıklardan tamamen bağımsız** bir `SimulationStage` olarak yürütür. Akışkan simülasyonu (yoğunluk difüzyonu, hız alanı güncellemesi, basınç projeksiyonu) doğrudan 3B voksel grid üzerinde çözülür; parçacıklar yalnızca grid verisini örnekleyerek (sample) görselleştirme yapar. Bu ayrım, hem hesaplama verimliliğini katlar hem de oynanış mantığının grid verisine doğrudan erişmesini sağlar (örneğin mermi tüneli açma işlemi grid üzerinde analitik bir küre silme işlemidir; sahnede kaç parçacık olduğundan bağımsız olarak anında yürütülür).

---

### 3.2. Unity VFX Graph: GPU-First & Context Tabanlı Yürütme
* **Mimari Özellikleri:**
  1. **Context Blokları:** `Spawn` $\to$ `Initialize` $\to$ `Update` $\to$ `Output` şeklinde 4 temel yaşam aşaması sunar. Her context, GPU compute shader'ında bir kernel fonksiyonuna karşılık gelir.
  2. **Fizik İzolasyonu ve SDF:** VFX Graph doğrudan PhysX ile konuşmaz. Milyonlarca parçacığı CPU fiziğine sokmak imkansız olduğundan, sahne statik SDF (Signed Distance Field) dokularına veya derinlik tamponuna (depth buffer) dönüştürülüp GPU üzerinde çarpıştırılır.
  3. **CPU Readback Sınırı:** Parçacıklar tamamen GPU belleğinde yaşadığı için CPU tarafındaki oyun mantığına (örneğin "bu kıvılcım oyuncuya çarptı mı?") geri okuma yapamaz; asenkron gecikme (pipeline stall) yaratır.
* **Tulpar İçin Mimari Çıkarım:**
  * GPU-first yaklaşım görsel zenginlik için harikadır; ancak oynanış mantığını (hasar verme, buton tetikleme) yönlendirecek parçacıklar için **CPU-fallback ve çift kanallı olay tamponu (Data Channels)** şarttır.

---

### 3.3. Godot Engine (`GPUParticles3D` & `CPUParticles3D`): İki Seviyeli Mimari
* **Mimari Özellikleri:**
  1. **İki Ayrı Pipeline:** Düşük donanımlar veya fizik etkileşimleri için `CPUParticles3D`, yüksek parçacık sayıları için `GPUParticles3D` sunar.
  2. **`ParticleProcessMaterial`:** Sanatçıların %90'ının ihtiyaç duyduğu parametreleri (yayılım şekli, hız sapması, yerçekimi, renk rampası, boyut eğrisi) tek bir standart veri yapısında toplar.
  3. **`ShaderMaterial` Köprüsü:** Sanatçı basit materyalden sıkıldığında "Convert to Shader" seçeneğiyle materyali anında açık GLSL parçacık shader koduna dönüştürerek özelleştirebilir.
* **Tulpar İçin Mimari Çıkarım:**
  * 5 dakikada efekt yapabilme ergonomisi hayati önemdedir. Tulpar sanatçıyı zorunlu olarak devasa bir node graph açmaya zorlamamalı; Godot gibi tek tıkla çalışan standart parametre yığınları sunmalı, gerekirse grafik katmanına geçilmelidir.

#### 3.3.1. Godot'nun Çift Pipeline Geçiş Mekanizması ve Tulpar Otomasyonu
Godot, `CPUParticles3D` ve `GPUParticles3D` düğümlerini iki ayrı sınıf olarak sunar; birinden diğerine **otomatik geçiş** yoktur. Sanatçı, parçacık sayısını artırdığında veya mobil hedefte donanım zorlandığında elle düğüm türünü değiştirmek zorundadır. Bu durum öğrenme eğrisini düşük tutsa da büyük ölçekli yapımlarda iş akışını kesintiye uğratır.

* **TulparEngine Farkı:** Tulpar, tek bir `ParticleSystem` bileşeni altında `simulation_mode` alanıyla (`Auto`, `Cpu`, `Gpu`) bu geçişi akıllıca otomatikleştirir. `Auto` modunda motor, her emitter için şu sezgisel kararı verir:
  1. **CPU SIMD Yolu:** Aktif parçacık sayısı $< 2.000$ **veya** doğrudan analitik zemin/fizik çarpışması (`enable_collision = true`) devredeyse simülasyon CPU SIMD arenasında yürütülür.
  2. **GPU Compute Yolu:** Parçacık sayısı $\ge 2.000$ **ve** karmaşık fizik etkileşimi kapalıysa simülasyon doğrudan Vulkan compute kernel'ına devredilir.
  3. **Özel Renderer Koruması:** Ribbon Trail veya Clustered Light Injection gibi CPU teğet hesaplaması gerektiren özel alt sistemler aktifse GPU yerine optimize CPU yolu devrede kalır.
Bu geçiş sanatçıya hissettirilmeden şeffaf biçimde yapılır; `ParticleEmitterConfig` parametreleri birebir aynı kalır, yalnızca yürütme arka ucu değişir.

---

### 3.4. Valve Source 2: Renderer Zenginliği ve Bileşen Operatör Mimarisi
* **Mimari Özellikleri:**
  1. **Bileşen Ayrımı:** Her efekt `Initializer` (doğum değerleri), `Operator` (karelik güncellemeler), `Constraint` (çarpışma ve sınırlandırmalar) ve `Renderer` (çizim tipi) C++ sınıflarına ayrılmıştır. 502'den fazla C++ sınıfı ve 77 enum bu yapıyı destekler.
  2. **Renderer Çeşitliliği:** Standart quad haricinde `Rope/Ribbon`, `Stretched Streak`, `Projected Decal`, `Cable`, `Dynamic Light`, `Meshlet` ve `Cloth Strand` yerel renderer'ları içerir.
* **Tulpar İçin Mimari Çıkarım:**
  * Parçacık yalnızca bir sprite değildir. TulparEngine'de `Quad`, `Streak`, `Plane`, `Sphere`, `Cube`, `Torus`, `Cone`, `Cylinder` ve `Ribbon` yerel renderer tipleri birinci sınıf vatandaş olmalıdır.

---

### 3.5. CryEngine: SecondGen ve Ebeveyn-Çocuk İlişkisi
* **Mimari Özellikleri:**
  1. **SecondGen (İkinci Nesil Alt-Yayıcılar):** Bir parçacık ömrünü tamamladığında, zemine çarptığında veya belirli bir mesafeyi kat ettiğinde ebeveyn (parent) konumundan anında yeni çocuk (child) parçacıklar doğurur (örneğin roketin havada patlayıp kıvılcımlar saçması, kıvılcımların da yere çarpınca duman doğurması).
  2. **Dış Sistem Nüfuzu (Attributes):** CryEngine'ın FlowGraph, TrackView ve C++ entity sistemi, parçacık efektlerinin parametrelerini (renk, şiddet, hız) gerçek zamanlı olarak manipüle edebilir.
  3. **Bounding Box Dinamik Çözünürlüğü:** Efektin dünya uzayındaki sınır kutusunu (AABB) hesaplayarak kameradan uzaklaştıkça çözünürlüğü ve parçacık sıklığını dinamik düşürür.
* **Tulpar İçin Mimari Çıkarım:**
  * Hiyerarşik parent-child ilişkisi karmaşık zincirleme efektler için zorunludur.
  * Dinamik AABB tabanlı çözünürlük ölçekleme, mobilde bant genişliğini korur.

---

### 3.6. Bevy Engine (`bevy_hanabi`): ECS-Native & Dual-World Tasarımı
* **Mimari Özellikleri:**
  1. **ECS-Native Veri:** Parçacıklar entity sistemiyle entegredir; ancak veri parçalanmasını önlemek için SoA (Structure of Arrays) formatında GPU compute tamponlarına paketlenir.
  2. **Dual-World:** Ana oyun mantığı `Main World` üzerinde deterministik çalışırken, parçacık render ve simülasyon verisi `Render World` üzerinde asenkron olarak işlenir.
* **Tulpar İçin Mimari Çıkarım:**
  * Tulpar'ın L4 Simülasyon katmanı SoA tabanlı arena belleği ile çalışmaktadır. Parçacık verisi de bu arena düzenini koruyarak bellek tahsissiz çalışmalıdır.

---

### 3.7. O3DE (Open 3D Engine): `OpenParticleSystem`
* **Mimari Özellikleri:**
  1. PopcornFX üçüncü parti lisans bağımlılığı terk edilmiş ve 2026 sürümünde tamamen açık kaynaklı `OpenParticleSystem` geliştirilmiştir.
  2. **Card-Based Inspector:** Modüller açılıp katlanabilen bağımsız kartlar (Collapsible Cards) halinde UI üzerinde sunulur.
  3. **Event Modülü:** Parçacık ölümü veya çarpışma anında motorun olay veri yoluna (EBus) mesaj fırlatır.
* **Tulpar İçin Mimari Çıkarım:**
  * Dış lisans bağımlılığı olmayan, motora gömülü açık kaynak mimari en güvenli yoldur.
  * Olay veri yolu (Event Bus) parçacıkların oyun dünyasıyla konuşmasını sağlar.

---

### 3.8. Flax Engine: Timeline + Graph Mimarisi
* **Mimari Özellikleri:**
  1. Parçacık sistemini sinematik bir "timeline" üzerinde tanımlar; her emitter bağımsız bir ses veya animasyon kanalı gibi zaman çizgisine oturur.
  2. CPU ve GPU simülasyonu arasında tek bir bayrakla geçiş yapılabilir; GPU modunda node graph arka planda HLSL compute shader'a derlenir.
* **Tulpar İçin Mimari Çıkarım:**
  * VFX'in animasyon ve sesle zamansal senkronizasyonu (örneğin kılıç savurma anında tam 0.4. saniyede kıvılcım çıkması) sinematik timeline entegrasyonuyla mükemmelleşir.

---

### 3.9. Roblox Engine: Basitlik, Hafiflik ve Attachment Modeli
* **Mimari Özellikleri:**
  1. `ParticleEmitter`, herhangi bir fiziksel gövdeye (`BasePart`) veya uzaysal bağlantı noktasına (`Attachment`) bağlanır.
  2. Minimum parametre setiyle (Rate, Lifetime, Speed, Spread, Acceleration, Drag) mobil cihazlarda milyonlarca oyuncuda sıfır donma ile çalışır.
* **Tulpar İçin Mimari Çıkarım:**
  * Aşırı mühendislikten kaçınılmalıdır. Temel fiziksel parametrelerin doğru matematiksel formülasyonu, 50 adet gereksiz ayardan çok daha etkilidir.

---

### 3.10. GameMaker: Üç Adımlı Katı Yaşam Döngüsü
* **Mimari Özellikleri:**
  1. Üç adımlı açık döngü: `part_system_create()` $\to$ `part_emitter_create()` $\to$ `part_type_create()`.
  2. Parçacık türü (Particle Type) bağımsız bir şablondur; 100 farklı emitter aynı tür şablonunu paylaşabilir.
* **Tulpar İçin Mimari Çıkarım:**
  * Parçacık tipi şablonu ile sahnede onu yayan emitter nesnesini ayırmak, bellek ayak izini minimize eder.

---

### 3.11. Stride Engine (Eski Xenko): Bounding Box Culling Disiplini
* **Mimari Özellikleri:**
  1. `ParticleSystemComponent` ve `ParticleEmitter` hiyerarşisi.
  2. Bounding box culling disiplini: Kamera görüş alanı (frustum) dışına çıkan parçacık sistemleri anında simülasyon ve render boru hattından düşürülür.
* **Tulpar İçin Mimari Çıkarım:**
  * Frustum culling olmaksızın arkada kalan parçacıkları simüle etmek mobil cihazların pilini tüketir.

---

### 3.12. Panda3D: Metin Tabanlı Yapılandırma ve ParticlePanel
* **Mimari Özellikleri:**
  1. Parçacık sistemleri metin dosyaları (`.ptf`) ile dışarıdan okunur.
  2. Üç temel yayılım modu sunar: `ExplicitLaunchVector`, `RadiateEmission`, `CustomVolume`.
* **Tulpar İçin Mimari Çıkarım:**
  * Efekt parametrelerinin tamamen insan tarafından okunabilir ve versiyon kontrolüne (git) uygun formatta serileştirilmesi şarttır.

---

### 3.13. jMonkeyEngine: `Jme-VFX` ve Geometri Genişletmesi
* **Mimari Özellikleri:**
  1. `ParticleEmitter` doğrudan motorun temel sahne düğümü olan `Geometry` sınıfından türetilmiştir.
  2. Modern `Jme-VFX` kütüphanesi ile görsel grafik düğüm desteği sonradan eklenmiştir.
* **Tulpar İçin Mimari Çıkarım:**
  * Parçacık yayıcının motorun standart sahne hiyerarşisine (Scene Graph / ECS) yabancı olmaması, standart transform matrisleri ve materyal sistemini doğrudan kullanabilmesi gerekir.

---

### 3.14. Endüstri Motorları Karşılaştırma Matrisi

| Motor | Mimarisi | Simülasyon Yeri | Modüler Yapı | Alt-Yayıcı (Sub-Emitter) | Fizik Çarpışması | Mobil TBDR Uyumu |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Unreal Niagara** | Stack + Graph | GPU Compute / CPU | Emitter / System | Çok Güçlü (Events) | SDF / Depth / PhysX | Orta (Bant genişliği tüketir) |
| **Unity VFX Graph** | Context Graph | Salt GPU Compute | Context Blokları | Orta (GPU içi) | SDF / Depth | Düşük (Masaüstü odaklı) |
| **Godot Engine** | Material + Shader | GPU / CPU İki Hat | Modüler Kartlar | Sınırlı | CPU / Basit Plane | **Yüksek** |
| **Valve Source 2** | Operatör Tabanlı | CPU / GPU Hibrit | Initializer/Operator | Güçlü | Sahne Derinliği | Yüksek (Optimize) |
| **CryEngine** | Veri Odaklı (SoA) | CPU Vektörel / GPU | SecondGen Parent | **Mükemmel (SecondGen)** | Bounding Box Fiziği | Orta |
| **Bevy (hanabi)** | ECS-Native | GPU Compute / SoA | Entity Bileşenleri | Geliştirilmekte | Henüz Yok | Yüksek |
| **O3DE** | Card Tabanlı | CPU / GPU | Modüler Kartlar | Güçlü (EBus Events) | Derinlik / PhysX | Orta |
| **Flax Engine** | Timeline + Graph | GPU / CPU Toggle | Track / Emitter | Var | Derinlik Tamponu | Yüksek |
| **Roblox** | Attachment Tabanlı | CPU Hafifletilmiş | Tekil Bileşen | Yok | Temel Çarpışma | **Çok Yüksek** |
| **TulparEngine** | **Hibrit (Stack+Graph)**| **GPU-First + CPU Fallback** | **7 Standart Modül** | **SecondGen + Events** | **Analitik Plane / Jolt** | **En Yüksek (0-Alloc + Q-Res)**|

---

## 4. SANATÇI DENEYİMİ VE UX AKIŞI (ARTIST UX WORKFLOW)

Bir VFX sanatçısının zihnindeki hayali motora aktarırken geçtiği bilişsel akış **8 adımdan** oluşur. Bu akış, bilişsel yükü en aza indirecek şekilde tasarlanmıştır:

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  1. SAHNEYE  │ ──> │  2. ŞABLON   │ ──> │ 3. YAYILIM   │ ──> │  4. GÖRÜNÜM  │
│     EKLE     │     │     SEÇ      │     │  GEOMETRİSİ  │     │   VE RENK    │
└──────────────┘     └──────────────┘     └──────────────┘     └──────────────┘
                                                                       │
┌──────────────┐     ┌──────────────┐     ┌──────────────┐             │
│ 8. PRESET VE │ <── │ 7. TELEMETRİ │ <── │6. ETKİLEŞİM &│ <───────────┘
│   KAYDETME   │     │ VE OVERDRAW  │     │  OLAY ZİNCİRİ│
└──────────────┘     └──────────────┘     └──────────────┘
```

### 4.1. Bilişsel 8-Adım İş Akışı

1. **Sahneye Ekle (Spawn Emitter):**  
   Kullanıcı hiyerarşi panelinden veya kısayolla (`Ctrl+Shift+P`) sahneye bir parçacık bileşeni ekler. Sistem varsayılan olarak optimize edilmiş bir başlangıç konfigürasyonuyla ayağa kalkar.
2. **Şablon Seç (Select Physical Template):**  
   Sanatçı boş bir tuvalde kaybolmaz. 16 doğrulanmış fiziksel şablondan birini seçer:  
   *Ateş, Duman, Kıvılcım, Yağmur, Kar, Büyü Küresi, Patlama, Enerji Portalı, Lazer Işını, Volkanik Lav, Gezegen Satürn Halkası, Şok Dalgası, Galaksi Girdabı, Kutup Işıkları (Aurora), Meteor İzi, Enerji Kalkanı.*
3. **Yayılım Geometrisini Belirle (Emission Shape & Velocity):**  
   Parçacıkların uzayda nereden fışkıracağı 6 analitik geometriden biriyle belirlenir:  
   *Nokta (Point), 1B Doğrusal Işın (Linear Beam), 2B Disk (Planar Ring), 2B Perde (Vertical Curtain), 3B Konik Huni (Conical Fountain), 3B Küresel Hacim (Spherical Volume).* Başlangıç hızı ve açısal sapması ayarlanır.
4. **Görünüm ve Renk Evrimini Ayarla (Appearance & Color Ramp):**  
   Parçacığın yaşamı boyunca boyutsal evrimi (büyüme/küçülme), canlı renk gradyanı (Multi-color stop ramp) ve render modeli (Sprite Quad, Stretched Streak, Sphere, Torus, Ribbon Trail) belirlenir.
5. **Kuvvetler ve Akışkan Fiziğini Bağla (Forces & Fluid Dynamics):**  
   Yerçekimi ivmesi, Stokes aerodinamik sürtünmesi (drag) ve havadaki girdapları oluşturan 3B Simplex Curl Noise türbülans kuvveti devreye alınır.
6. **Etkileşim ve Olay Zincirini Kur (Interaction & Niagara Events):**  
   Zemin çarpışma düzlemi ($y=0$), sekme elastikiyeti (restitution) ve parçacık öldüğünde doğacak alt-parçacık (SecondGen Sub-Emitter) sayısı tanımlanır. Oynanış olayları (Hasar, Fizik İtkisi, Islaklık Haritası) işaretlenir.
7. **Telemetri ve Overdraw Bütçesini Doğrula (Performance Telemetry):**  
   Aktif parçacık sayısı, GPU/CPU milisaniye çizim maliyeti, mobil overdraw göstergesi ve instanced draw call durumu tek bakışta izlenir.
8. **Önayar Olarak Kaydet ve Paylaş (Save Preset & Reuse):**  
   Hazırlanan efekt tek tıkla proje varlık kütüphanesine (`.vfx`) kaydedilir veya `ComponentClipboard` ile sahnedeki diğer nesnelere kopyalanır.

---

### 4.2. UX Paradigma Karşılaştırmaları

* **Niagara UX Dersi:** Yığın (stack) tabanlı editör, modüllerin yukarıdan aşağıya mantıksal sıralanmasını sağlar. Her modülün ait olduğu aşamayı (`Spawn`, `Update`, `Event`) bilmek sanatçının hata yapmasını engeller.
* **Unity VFX Graph UX Dersi:** Görsel çizge (node graph) karmaşık matematiksel operasyonlar için harikadır; ancak basit bir ateş efekti yapmak isteyen sanatçı için 4 farklı context düğümü kurup bağlamak aşırı yorucudur.
* **Godot UX Dersi:** Tek bir Inspector materyalinde tüm parametreleri kompakt slider'larla toplamak öğrenme eğrisini sıfıra indirir. 5 dakikada çalışan efekt üretilir.
* **TulparEngine Hibrit Çözümü:**  
  Tulpar, ana editör penceresinde **Godot ve Niagara'nın Inspector basitliğini (7 Modül Kartı)** sunar. İleri düzey teknik sanatçılar ise tek tıkla grafiği açıp özel compute düğümlerini bağlayabilir.

---

### 4.3. TulparEngine Entegre Editör Inspector Tasarımı

Aşağıda `app/editor_app.cpp` içerisinde çalışan profesyonel açık kaynak VFX arayüzünün mimari yerleşimi gösterilmiştir:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       TULPARENGINE VFX MÜFETTİŞİ                            │
├─────────────────────────────────────────────────────────────────────────────┤
│  [► Oynat/Durdur]  [↺ Sıfırla]  [⚡ Püskürt (+25)]   Şablon: [Ateş Efekti ▼] │
├─────────────────────────────────────────────────────────────────────────────┤
│  ▼ 1. EMİSYON & YAŞAM DÖNGÜSÜ (EMISSION & LIFECYCLE)                       │
│     • Yayma Hızı (Spawn Rate)          : [======|             ] 65.0 /s     │
│     • Min - Maks Ömür (Lifetime)       : 0.80 s  -  1.60 s                  │
│     • Teorik Kararlı Popülasyon        : ~78 aktif parçacık                 │
├─────────────────────────────────────────────────────────────────────────────┤
│  ▼ 2. YAYILIM GEOMETRİSİ VE HIZ (EMISSION SHAPE & VELOCITY)                │
│     • Yayılım Biçimi (Shape)           : [2B Düzlemsel Disk / Halka      ▼] │
│     • Başlangıç Hızı (Base Velocity)   : X: 0.00  Y: 2.80  Z: 0.00  m/s     │
│     • Rastgele Sapma (Velocity Jitter) : X: 0.40  Y: 0.60  Z: 0.40          │
│     • Analiz Göstergesi                : Hız: 2.80 m/s | Biçim: Konik Huni  │
├─────────────────────────────────────────────────────────────────────────────┤
│  ▼ 3. KUVVETLER VE AKIŞKAN FİZİĞİ (FORCES & DYNAMICS)                       │
│     • Yerçekimi İvmesi (Gravity)       : [===|                ] -9.80 m/s²  │
│     • Stokes Hava Direnci (Drag)       : [==|                 ] 0.20        │
│     • Curl Noise Türbülans Gücü        : [====|               ] 2.50 m/s²   │
│     • Curl Noise Frekansı              : 0.80 /m                            │
│     • Zemin Çarpışması & Sekme         : [X] Etkin (y=0) | Elastikiyet: %55 │
├─────────────────────────────────────────────────────────────────────────────┤
│  ▼ 4. RENDER MODELİ VE GÖRÜNÜM (RENDERER & APPEARANCE)                     │
│     • İlkel Model (Primitive Mesh)     : [3B Enerji Simiti (Torus)       ▼] │
│     • Şerit / Kuyruk İzi (Ribbon Trail): [X] Etkin                          │
│     • Boyut Evrimi (Size Start -> End) : 0.22 m -> 0.03 m (%14 Küçülen)     │
│     • Renk Gradyanı (Canlı Spektrum)   : [▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓] │
│     • Başlangıç / Bitiş Rengi          : RGB(1.0, 0.6, 0.1) -> (0.2, ...)   │
├─────────────────────────────────────────────────────────────────────────────┤
│  ▼ 5. OLAY ZİNCİRLERİ & DATA CHANNELS (NIAGARA EVENTS)                      │
│     • Ölümde Alt-Yayıcı (Sub-Emitter)  : 4 adet kıvılcım (SecondGen)        │
│     • Veri Kanalları (Data Channels)   : [X] Hasar  [X] Fizik İtki  [ ] Su  │
├─────────────────────────────────────────────────────────────────────────────┤
│  ▼ 6. CS2 HACİMSEL VOKSEL DUMANI (RESPONSIVE VOXEL SMOKE)                  │
│     • [Duman Doldur]  [Mermi Tüneli Aç]  [Bomba Şok Dalgası]  [Sıfırla]     │
├─────────────────────────────────────────────────────────────────────────────┤
│  ▼ 7. MOBİL TBDR & GPU TELEMETRİSİ (PERFORMANCE HUD)                       │
│     • Clustered Işık Enjeksiyonu       : Etkin (2.5 m etki yarıçapı)        │
│     • Bellek & GPU Tahsisi             : 0 alloc/frame | 1 Instanced Draw   │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 5. MODÜL TASARIMI VE VERİ SÖZLEŞMELERİ

TulparEngine parçacık mimarisi, monolitik ve kontrolsüz yapılar yerine her biri matematiksel olarak tanımlı girdi/çıktı sözleşmesine sahip **10 standart açık kaynak modülü** üzerinden çalışır:

| No | Modül Adı | Temel İşlev | Örnek Motor | Tulpar C++ Temsili |
| :--- | :--- | :--- | :--- | :--- |
| **1** | **Emission** | Zaman bazlı spawn hızı, burst patlamaları, prewarm | Tümü (Niagara/Godot) | `spawn_rate`, `burst()`, `lifetime_min/max` |
| **2** | **Shape** | Parçacıkların doğduğu analitik veya hacimsel geometri | Godot, Niagara, O3DE | `ParticleEmissionShape` (Point, Beam, Ring...) |
| **3** | **Velocity** | Başlangıç vektörü, konik açısal sapma, teğetsel hız | Tümü (Roblox/Unity) | `base_velocity`, `velocity_jitter` |
| **4** | **Color Over Life** | Yaşam süresi boyunca HDR renk ve opaklık eğrisi | Tümü | `color_start`, `color_end`, Canlı Gradyan Barı |
| **5** | **Size Over Life** | Yaşam süresi boyunca boyutsal büyüme/sönümlenme | Tümü | `size_start`, `size_end` |
| **6** | **Noise & Fluid** | 3B Simplex divergence-free Curl Noise türbülansı | Niagara, VFX Graph | `curl_noise_strength`, `curl_noise_frequency` |
| **7** | **Collision** | Zemin düzlemi, derinlik tamponu veya SDF çarpışması | Godot, CryEngine | `enable_collision`, `restitution`, `friction` |
| **8** | **Sub-Emitter** | Ölümde veya çarpmada yeni parçacık doğumu (SecondGen)| CryEngine, Niagara | `spawn_on_death_count`, `sub_emitter_cfg` |
| **9** | **Events & Channels**| Oyun mantığına veri aktarımı (Hasar, İtki, Islaklık) | Niagara, O3DE | `ParticleDataChannelQueue`, Event Bus |
| **10**| **Renderer** | Çizim geometrisi (Sprite, Streak, Mesh, Ribbon) | Source 2, Flax | `shape` (0..7), `ribbon`, WBOIT |

### 5.2. Modül Başına Matematiksel Sözleşmeler

#### Modül 1: Emisyon & Yaşam Döngüsü
* **Zaman Formülü:** $N_{yeni} = \lfloor \Delta t \cdot \text{Rate} + \text{Akümülatör} \rfloor$
* **Popülasyon Tahmini:** $P_{kararlı} = \text{Rate} \times \frac{\text{Lifetime}_{min} + \text{Lifetime}_{max}}{2}$

#### Modül 2: Yayılım Geometrisi (Emission Shape)
* **Nokta (Point):** $\vec{p} = \vec{p}_{merkez}$
* **Düzlemsel Halka (Planar Ring):** $\theta \sim U(0, 2\pi), r \sim U(r_{ic}, r_{dis}) \implies \vec{p} = (r\cos\theta, 0, r\sin\theta)$
* **Konik Huni (Conical Fountain):** $\phi \sim U(0, 2\pi), \theta \sim U(0, \theta_{koni}) \implies \vec{v} = v_0 (\sin\theta\cos\phi, \cos\theta, \sin\theta\sin\phi)$

#### Modül 3: Kuvvetler, Sürtünme ve Akışkan Fiziği
* **Stokes Hava Direnci:** $\vec{F}_{drag} = -k_{drag} \cdot \vec{v}$
* **3B Divergence-Free Curl Noise:**
  $$\vec{v}_{curl} = \nabla \times \vec{\Psi}(\vec{p})$$
  $$\vec{v}_{curl} = \left( \frac{\partial \Psi_z}{\partial y} - \frac{\partial \Psi_y}{\partial z},\; \frac{\partial \Psi_x}{\partial z} - \frac{\partial \Psi_z}{\partial x},\; \frac{\partial \Psi_y}{\partial x} - \frac{\partial \Psi_x}{\partial y} \right)$$
  Bu vektör alanı matematiksel olarak $\nabla \cdot \vec{v}_{curl} = 0$ koşulunu sağladığından parçacıklar bir noktada sıkışıp kümelenmez; duman ve alev gibi doğal akışkan girdapları (vortices) oluşturur.

#### Modül 4: Renk ve Boyut Evrimi
* **Normalleştirilmiş Yaşam Oranı:** $t_{norm} = 1.0 - \frac{\text{Kalan Ömür}}{\text{Toplam Ömür}} \in [0.0, 1.0]$
* **Boyut İnterpolasyonu:** $S(t) = S_{start} + t_{norm} \cdot (S_{end} - S_{start})$
* **Renk İnterpolasyonu:** $C(t) = C_{start} \cdot (1 - t_{norm}) + C_{end} \cdot t_{norm}$

---

### 5.3. Grafik Editörü Mimarisi (Node-Based VFX Graph)

TulparEngine'in ileri düzey görsel programlama katmanı, Unity VFX Graph ve Unreal Niagara'nın görsel çizge güçlerini birleştiren **dört katmanlı bir hiyerarşik düğüm mimarisi** kullanır:

| Katman | İşlev | Örnek Düğüm / Blok |
| :--- | :--- | :--- |
| **Event** | Grafiğin tetikleme ve aktivasyon girişi | `OnPlay`, `OnStop`, `OnImpact`, `CustomEvent`, `GPUEvent` |
| **Context** | Yaşam döngüsü aşaması konteyneri (yürütme kapsayıcısı) | `Spawn` $\to$ `Initialize` $\to$ `Update` $\to$ `Output` |
| **Block** | Context içinde parçacık özniteliğini yazan atomik davranış | `Set Position`, `Add Velocity`, `Curl Noise`, `Color Over Life` |
| **Operator** | Bağımsız, saf matematiksel hesaplama düğümü | `Add`, `Multiply`, `Cross`, `SampleCurve`, `Noise3D`, `GetAttribute` |

* **İki Yönlü İş Akışı (Two-Way Flow):**
  1. **Dikey Yürütme Hattı (Control Flow):** Context'ler dikey eksende `Flow` portlarıyla birbirine bağlanarak parçacığın doğumdan ölüme yaşam akışını tanımlar.
  2. **Yatay İfade Ağı (Expression Graph):** Operator düğümleri yatay eksende bir matematiksel veri ağı oluşturarak Block parametrelerini (örneğin hıza eklenen dinamik bir sinüs dalgasını) besler.

* **Derleme Akışı (Compilation Pipeline):**  
  Kullanıcı VFX grafiğini kaydettiğinde (`Ctrl+S`), görsel editör çizgeyi bir Ara Temsile (IR / AST) dönüştürür ve hedef donanıma göre iki farklı çıktı üretir:
  1. **GPU Hedefi:** Doğrudan SPIR-V uyumlu GLSL Compute Shader kaynağı üretilir; `renderer/particle_gpu_indirect.hpp` (`TbdrParticleBinner`) bu shader'ı derleyip Vulkan compute boru hattına bağlar.
  2. **CPU Hedefi:** SIMD-optimize edilmiş C++ döngüleri üretilir; motor `IParticleModule` arayüzünü uygulayan dinamik bir çalışma zamanı modülü olarak döngüyü işletir.
  3. **Otomatik Seçim (`simulation_mode`):** `ParticleEmitterConfig::simulation_mode` alanı (`Auto`, `Cpu`, `Gpu`) üzerinden yürütme hedefini yönetir.

---

## 6. DETERMINİZM, VARYASYON VE AĞ SENKRONİZASYONU (NETCODE)

Pek çok oyun motorunda parçacık efektleri kontrolsüz rastgelelik tohumlarıyla (`rand()`, `std::random_device`, sistem saati) üretilir. Bu durum simülasyon kaydı (replay) veya geri alma tabanlı ağ mimarisinde (rollback netcode) felakete yol açar: iki oyuncunun ekranında patlayan parçacıklar farklı yönlere uçar, farklı duman sütunları oluşur ve oyuncu görüş çizgisi (line-of-sight) desekronize olur.

### 6.1. Xorshift32 Deterministik Rastgelelik
TulparEngine'de doğum anı rastgelelikleri (ömür varyasyonu, hız sapması, boyut, renk) `core/math/random.hpp` içerisindeki **deterministik `Rng` (xorshift32)** algoritmasıyla üretilir.

$$\text{state} \leftarrow \text{state} \oplus (\text{state} \ll 13)$$
$$\text{state} \leftarrow \text{state} \oplus (\text{state} \gg 17)$$
$$\text{state} \leftarrow \text{state} \oplus (\text{state} \ll 5)$$

### 6.2. Bit-Identical Doğrulama ve Rollback Netcode Uyumu
* **Platformlar Arası Bit Eşitliği:** Aynı tohum (seed) ve aynı `emit()` çağrı sırası verildiğinde; x86_64, ARM64, Windows, Linux veya Android platformlarında **bit-eş aynı parçacık pozisyon ve hız dizisi** elde edilir.
* **Birim Test Kanıtı:** `tests/test_particles.cpp` altındaki `particles_same_seed_yields_bit_identical_emit_sequence` birim testi, iki ayrı sistemde üretilen parçacıkların float belleğinin bit düzeyinde aynı olduğunu garanti altına alır.
* **Sıfır Ağ Bant Genişliği:** `sim/rollback.hpp` veya `sim/replay.hpp` çalışırken devasa parçacık pozisyon dizilerini ağ üzerinden göndermeye gerek kalmaz; yalnızca başlangıç tohumu (4 bayt) ve tetikleme anı gönderilerek her istemcide bit-birebir aynı simülasyon çoğaltılır.

#### 6.2.1. Sunum Katmanı Efektleri ve Rollback Ayrımı
Geri alma tabanlı ağ mimarisinde (Rollback Netcode), yerel istemci tahmin hatası yaptığında fiziksel dünya son doğrulanan durumdan geriye sarılır ve her `step` çağrısı $0 \dots N$ kez yeniden simüle edilir (re-simulation). Bu süreçte parçacıkların yönetimi ikiye ayrılır:

1. **Oynanışı Etkileyen Efektler (`affects_gameplay = true`):**
   * *Örnekler:* Görüş çizgisini (line-of-sight) tıkayan CS2 tarzı voksel sis dumanı, temas halinde hasar veren alev alanı, patlama itki dalgası.
   * *Yürütme:* Bu emitter'ların rastgelelik durumu `sim/rollback.hpp` tarafından yönetilen deterministik durum vektörüne kaydedilir. Re-simülasyon sırasında her karede aynı parçacıklar aynı koordinatlarda yeniden doğar.
2. **Yalnızca Görsel / Kozmetik Efektler (`affects_gameplay = false`):**
   * *Örnekler:* Kılıç sürtünme kıvılcımları, ayak tozu, dekoratif ortam yaprakları, ekran sarsıntısı spriteları.
   * *Yürütme (Fire-and-Forget):* Bu efektler rollback re-simülasyon döngüsünde çalıştırılmaz (`if (!ctx.is_replay)`). Yalnızca gerçek dünya zamanında ilerleyen **canlı karede (live frame)** bir kez tetiklenir. Bu ayrım, rollback sırasında yüz binlerce geçici parçacığın gereksiz yere yaratılıp yok edilmesini önler ve CPU bütçesini tamamen fiziksel geri sarma hesaplarına bırakır.

---

### 6.3. Parametre Eşleme ve Deterministik Varyasyon
* Sanatçı, efektin her oynatılışında farklı bir varyasyon görmesini isterse tohumu `seed + variation_id` ile kaydırabilir. Rastgelelik kontrol altındadır, asla körü körüne bırakılmaz.

---

## 7. PERFORMANS MİMARİSİ VE MOBİL TBDR OPTİMİZASYONU

Mobil platformlardaki (ARM Mali, Qualcomm Adreno, Apple GPU) en büyük darboğaz aritmetik shader işlem gücü değil, **bellek bant genişliği (memory bandwidth)** ve **aşırı çizimdir (overdraw)**.

### 7.1. GPU vs. CPU Simülasyon Ayrımı
* **CPU Simülasyonu:** Az sayıda (<2.000), karmaşık fiziksel çarpışma yapan, mermi kovanları gibi çevreyle doğrudan etkileşen parçacıklar için kullanılır. SIMD ve Arena belleğiyle sıfır maliyetle çözülür.
* **GPU Simülasyonu:** Çok sayıda (10.000 - 1.000.000), görsel yoğunluk sağlayan kıvılcım, kar, yağmur ve duman gibi efektler için Vulkan Compute Pipeline üzerinden yürütülür.

#### 7.1.1. Vulkan Compute Pipeline ve SSBO Tasarımı
GPU parçacık simülasyonu, Vulkan'da `VkPipeline` ve `VkDescriptorSet` mekanizmaları üzerinden donanım hızlandırmalı olarak yürütülür.

* **SSBO Bellek Düzeni (Structure of Arrays / Packed Buffer):** Parçacık verisi, C++ ve SPIR-V (GLSL/HLSL) arasında `std430` hizalama kurallarına tam uyacak şekilde 16 baytlık vektör bloklarıyla paketlenir:
  ```cpp
  struct GpuParticleData {
    Vec4 position_and_life; // xyz = Konum (m), w = Kalan Yasam Suresi (s)
    Vec4 velocity_and_size; // xyz = Hiz Vektoru (m/s), w = Anlik Boyut (m)
    Vec4 color;             // rgba = Canli HDR Renk ve Opaklik
  };
  ```
* **İş Grubu (Workgroup) Boyutu:** Her compute dispatch komutunda `local_size_x = 256` seçilir. Örneğin 65.536 parçacık için $65.536 / 256 = 256$ workgroup GPU çekirdeklerine dağıtılır. Her invocation tek bir parçacığın hareket denklemini SIMD genişliğinde yürütür; CPU tarafında sıfır karelik döngü çalışır.
* **Compute-to-Graphics Senkronizasyonu (Pipeline Barrier):** Compute shader SSBO'ya yazmayı tamamladıktan sonra raster grafik aşamasının bu veriyi doğrudan vertex/instance girdisi olarak okuyabilmesi için komut tamponuna bellek bariyeri enjekte edilir:
  ```cpp
  VkBufferMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
  barrier.buffer = particle_ssbo;
  // vkCmdPipelineBarrier icinde VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT -> VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
  ```
  Bu işlem aynı `VkCommandBuffer` içinde kaydedildiği için donanım seviyesinde GPU-CPU beklemesi (bubble) oluşmaz.
* **Dolaylı Çizim (Indirect Draw):** `renderer/particle_gpu_indirect.hpp` modülü, compute aşamasında hayatta kalan parçacık sayısını atomik olarak bir `VkDrawIndirectCommand` tamponuna yazar; ardından `vkCmdDrawIndirect` çağrısı CPU'ya hiçbir veri geri okuması (readback) yapmadan doğrudan GPU üzerinde kapalı bir döngüde yalnızca yaşayan parçacıkları ekrana çizer.

---

### 7.2. Mobil TBDR Darboğazları ve Çeyrek Çözünürlüklü Tampon (Quarter-Res RT)
Büyük duman ve alev parçacıkları tam ekran çözünürlüğünde ($1080p / 1440p$) üst üste bindiğinde piksel başına 15-25 kat overdraw üretir. Bu durum TBDR GPU'ların karo tamponlarını (tile cache) taşırır ve dramatik FPS düşüşlerine yol açar.
* **Çözüm:** Büyük hacimli şeffaf parçacıklar yarım veya çeyrek çözünürlüklü ayrı bir Offscreen Render Target'a çizilir ($\frac{1}{4}$ piksel iş yükü).
* **Bilateral Upsampling:** Düşük çözünürlüklü parçacık tamponu ana sahne derinlik haritası (depth buffer) ile kenar korumalı harmanlanarak kompozit edilir. Hard-edge kenar kusurları tamamen engellenir.

#### 7.2.1. Ağırlıklı Harmanlanmış Sıra-Bağımsız Şeffaflık (WBOIT) Matematiksel Formülasyonu
WBOIT (Weighted Blended Order-Independent Transparency), McGuire ve Bavoil (2013) tarafından önerilen ve Volition'un *Agents of Mayhem* oyununda yoğun parçacık sahnelerinde CPU sıralama yükünü tamamen ortadan kaldırmak için kullanılan endüstri standardı bir tekniktir.

Geleneksel arka-yüzden-öne CPU sıralaması ($O(N \log N)$), per-nesne sıralama yaptığı için birbirinin içine geçen parçacıklarda sıralama "popping" kusurlarına yol açar. WBOIT, sıralı harmanlamayı ağırlıklı bir ortalama integrali ile analitik olarak çözer:

$$C = \frac{\sum_{n} S_n \alpha_n w(z_n, \alpha_n)}{\sum_{n} \alpha_n w(z_n, \alpha_n)}, \quad R = \prod_{n} (1 - \alpha_n)$$
$$\text{Son Renk} = C \cdot (1 - R) + D \cdot R$$

Burada $S_n$ parçacığın çıkış rengi, $\alpha_n$ opaklığı, $z_n$ normalize kamera derinliği, $w(z, \alpha)$ McGuire derinlik ağırlık fonksiyonu ve $D$ ana sahne arka plan rengidir. McGuire ağırlık fonksiyonu derinlikte katı bir sönümleme uygular:

$$w(z, \alpha) = \alpha \cdot \max\left(10^{-2},\; \min\left(3 \cdot 10^{3},\; \frac{10}{10^{-5} + \left(\frac{z}{200}\right)^{4}}\right)\right)$$

* **Partikül Sistemine Özgü Additivite Terimi (Emissive Extension):** TulparEngine'de `renderer/wboit_pass.hpp` modülü, `VoxelSmokeGrid` veya `ParticleGaussian` gibi yarı-saydam alev ve enerji efektleri için denklemi **ek bir additivite (toplanabilirlik) ölçeği** ile genişletir. Emissive parçacıklar alfa değerini doğrudan sıfıra yaklaştırıp renk akümülatörüne eklenerek, ayrı bir additive blending geçişi açmaya gerek kalmadan hem şeffaf dumanı hem de parıldayan alevi tek bir WBOIT geçişinde doğru harmanlar.

#### 7.2.2. TBDR Erken Derinlik Testi ve Transient Attachment Optimizasyonu
Mobil TBDR GPU'ları (ARM Mali, Qualcomm Adreno, Apple GPU), ekranı $16 \times 16$ veya $32 \times 32$ piksellik donanımsal karolara (tile) bölerek çalışır. Çip üstü ultra-hızlı SRAM tamponlarında çalışan Gizli Yüzey Eleme (Hidden Surface Removal - HSR) ve Erken Derinlik Testi (Early-Z), overdraw yükünü donanım seviyesinde azaltır.

1. **Salt Okunur Sahne Derinlik Girişi (`VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL`):** Parçacık geçişi başladığında, önceki opak sahnenin ürettiği Z-tamponu parçacık boru hattına derinlik testi açık (`depthTestEnable = VK_TRUE`), ancak derinlik yazması kapalı (`depthWriteEnable = VK_FALSE`) olarak bağlanır. Karo binning aşamasında opak duvarların arkasında kalan milyonlarca parçacık pikseli fragment shader'a dahi girmeden donanımca anında elenir.
2. **Geçici Ek (Transient Attachment) ve Tembel Bellek (Lazy Allocation):**  
   Çeyrek çözünürlüklü parçacık render hedefi veya WBOIT biriktirme tamponları yalnızca o kare içinde geçerlidir; ana RAM'e (DRAM) geri yazılmalarına gerek yoktur. Bu tamponlar Vulkan üzerinde `VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT` bayrağıyla açılır ve cihaz belleğinden `VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT` ile istenir. Donanım bu tamponu fiziksel DRAM'e hiç ayırmadan doğrudan çip üstü karo hafızasında (on-chip tile buffer) tüketip yok eder; bu sayede mobil cihazda bellek bant genişliği ve pil tüketimi dramatik ölçüde korunur.

---

### 7.3. Sıfır Bellek Tahsisi Disiplini (0 Alloc Arena)
* Simülasyon sırasında hiçbir `malloc`, `new`, `std::vector::push_back` çağrısı yapılamaz (`faz0_gate_zero_allocations_per_frame`).
* Parçacık havuzu sahne açılışında `SystemArena` üzerinden tek bir bitişik blok halinde tahsis edilir:
  ```cpp
  particles_ = arena.alloc_array<Particle>(max_particles);
  ```
* Ölen parçacıkların temizlenmesi **Takas-ile-Silme (Swap-with-Last)** algoritmasıyla $O(1)$ sürede yapılır; bellek dizisi her an sıkı (dense) tutulur, delik ve parçalanma (fragmentation) oluşmaz.

### 7.4. Clustered Forward+ Işık Enjeksiyonu
Partiküllerin çevreyi aydınlatması (ör. alevin duvara vurması) için sahneye ek bir dinamik ışık geçişi açılmaz. Motorun clustered forward+ ışık ızgarasına parçacık çekirdeği anlık nokta ışık olarak eklenir; ek draw call oluşturulmaz.

### 7.5. Dinamik Bounding Box ve Mesafe Bazlı LOD / Culling
* Stride ve CryEngine'den esinlenilen dinamik AABB sistemiyle, kamera frustum'ı dışındaki efektler simülasyon döngüsünden düşürülür.
* Kameraya uzak mesafedeki emitter'ların spawn hızları mesafe karesiyle orantılı olarak seyreltilir (Distance LOD).

---

## 8. TULPARENGINE YENİ NESİL HİBRİT VFX MİMARİSİ VE C++ SPESİFİKASYONU

TulparEngine kod tabanında halihazırda test edilmiş, derlenmiş ve üretimde olan çekirdek C++ spesifikasyonu:

### 8.1. Çekirdek Veri Yapıları (`ParticleEmitterConfig`)

```cpp
namespace tulpar::engine::content {

enum class ParticleEmissionShape : uint8_t {
  Point = 0,             // Tek bir merkez noktadan cikis
  LinearBeam = 1,        // Tek eksende hizli dogrusal isin
  PlanarRing = 2,        // XZ duzleminde dairesel halka / disk
  VerticalCurtain = 3,   // XY duzleminde dikey perde / duvar
  ConicalFountain = 4,   // Yukari yonlu acisal konik huni
  SphericalVolume = 5,   // 3B esit kuresel hacim
};

enum class ParticleSimulationMode : uint8_t {
  Auto = 0,              // Parcacik sayisi ve carpismaya gore otomatik CPU/GPU
  Cpu = 1,               // Zorunlu CPU SIMD Arenasi (fizik/ribbon uyumlu)
  Gpu = 2,               // Zorunlu Vulkan Compute Pipeline
};

struct ParticleEmitterConfig {
  Vec3 spawn_pos{};
  Vec3 base_velocity{};
  Vec3 velocity_jitter{};
  float lifetime_min = 1.0f;
  float lifetime_max = 1.0f;
  float size_start = 0.2f;
  float size_end = 0.0f;
  Vec3 color_start{1.0f, 0.6f, 0.1f};
  Vec3 color_end{0.2f, 0.2f, 0.2f};
  Vec3 gravity{0.0f, -2.0f, 0.0f};
  bool custom_gravity = true;

  // Akiskan Turbulansi & Aerodinamik (Modul 3 & 6)
  float curl_noise_strength = 0.0f;
  float curl_noise_frequency = 1.0f;
  float drag = 0.0f;

  // Fiziksel Carpisma & Sekme (Modul 7)
  bool enable_collision = false;
  float collision_plane_y = 0.0f;
  float restitution = 0.6f;
  float friction = 0.1f;

  // Render & Geometri Modeli (Modul 10)
  // 0: Quad, 1: Streak, 2: Plane, 3: Sphere, 4: Cube, 5: Torus, 6: Cone, 7: Cylinder
  uint32_t shape = 0;
  bool ribbon = false;

  // Olay Zincirleri & Alt-Yayici (Modul 8 & 9)
  uint32_t spawn_on_death_count = 0;
  class ParticleSystem *sub_emitter = nullptr;
  ParticleEmitterConfig *sub_emitter_cfg = nullptr;

  // Rollback ve Yurutme Hedefi
  bool affects_gameplay = false;
  ParticleSimulationMode simulation_mode = ParticleSimulationMode::Auto;
};

} // namespace tulpar::engine::content
```

### 8.2. İleri Düzey Simülasyon ve Render Bileşenleri
Motorda aktif olarak çalışan tamamlayıcı alt sistemler:
1. **`content/ribbon.hpp` (`RibbonTrail`):** Harekete bağlı sürekli şerit geometrisi, UV koordinatlandırma ve teğet hesaplama (kılıç izleri, mermi arkası izleri).
2. **`content/particle_channel.hpp` (`ParticleDataChannelQueue`):** Niagara tarzı oyun içi hasar (`kChannelDamage`), fizik darbesi (`kChannelPhysicsImpulse`) ve dinamik zemin ıslanma haritası (`GroundWetnessMap`).
3. **`content/particle_gaussian.hpp` (`ParticleGaussian`):** 3D Gaussian Splatting tabanlı anizotropik hacimsel elipsoid parçacıkları ($3\times3$ kovaryans matrisi $\Sigma = R \cdot S \cdot S^T \cdot R^T$).
4. **`renderer/wboit_pass.hpp` (`WBOIT`):** Derinlik sıralaması gerektirmeyen şeffaflık (Weighted Blended Order-Independent Transparency) ve hareket vektörleri (Motion Vectors).
5. **`sim/voxel_smoke.hpp` (`VoxelSmokeGrid`):** CS2 tarzı $24 \times 24 \times 24$ etkileşimli hacimsel voksel dumanı (mermi tüneli delme, el bombası şok dalgası saçılımı).
6. **`renderer/particle_gpu_indirect.hpp` (`TbdrParticleBinner`):** Vulkan GPU Indirect Compute & Draw komutları.

### 8.3. Genişletilebilir C++ Modül API'si (`IParticleModule`)
Yeni nesil modüller için tanımlanan soyut arayüz:
```cpp
namespace tulpar::engine::content {

class IParticleModule {
public:
  virtual ~IParticleModule() = default;
  virtual void on_emitter_spawn(ParticleEmitterConfig &cfg) {}
  virtual void on_particle_spawn(Particle &p, const ParticleEmitterConfig &cfg, Rng &rng) {}
  virtual void on_particle_update(Particle &p, float dt, const ParticleEmitterConfig &cfg) {}
  virtual void on_particle_death(const Particle &p, ParticleDataChannelQueue &events) {}
};

} // namespace tulpar::engine::content
```

---

## 9. SONUÇ VE TASARIM MANİFESTOSU

TulparEngine VFX alt sistemi, sıradan bir motor özelliği değil, sanatçı ile donanım arasındaki en kusursuz köprüdür. Bu mimari şu **beş sarsılmaz ilke** üzerinde yükselir:

> ### 1. Sanatçı İçin Boya Paleti, Mühendis İçin Cerrahi Neşter
> Görsel sanatçı 30 saniye içinde standart şablonlardan (Ateş, Duman, Lazer, Portal) birini seçip boyutunu ve rengini canlı gradyandan ayarlayabilmeli; motor mühendisi ise arka plandaki curl noise frekansına, SIMD bellek hizalamasına ve GPU compute komutlarına müdahale edebilmelidir.
>
> ### 2. Rastgelelik Değil, Matematiksel Determinizm
> Hiçbir parçacık kontrolsüzce savrulmaz. Her rastgelelik tohumu deterministiktir (Xorshift32); bu sayede en çılgın patlama efekti dahi ağ üzerinden 0 bant genişliğiyle tam senkronize çoğaltılabilir, simülasyon geçmişi geri sarılabilir (rollback netcode).
>
> ### 3. Mobilde Sıfır Taviz (0 Alloc, 0 FPS Drop)
> Masaüstünde çalışan bir efekt mobil cihazda motoru çökertemez veya aşırı ısıtamaz. Clustered forward+ ışık entegrasyonu, çeyrek çözünürlüklü tamponlama (quarter-res RT) ve sıfır dinamik bellek tahsisiyle (arena discipline) mobil TBDR donanımlarında daima $60\text{ FPS}$ korunur.
>
> ### 4. Her Piksel Bir Amaç Taşır (Overdraw Bilinci)
> Sanatçıya ekranı rastgele dumanla doldurma hakkı verilirken, arka planda overdraw ısı haritası ve bütçe göstergeleri şeffaf olarak maliyeti raporlar.
>
> ### 5. Görsel Bir Şölen, Katı Bir Disiplin
> TulparEngine'in VFX editörü bir araç değil, sanatçının elindeki en ilham verici oyuncağıdır. Öğrenmesi kolay, ustalaşması sınırsız, donanıma saygısı mutlaktır.

---
*Doküman TulparEngine v0.2.0 resmi VFX çekirdek mimari şartnamesi olarak onaylanmıştır.*
