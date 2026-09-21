# TULPARENGINE — SİS, HACİMSEL ATMOSFER VE SIFIR MALİYETLİ MOBİL SİS MİMARİSİ

> **Doküman Türü:** Çekirdek Render Mimarisi, 25 Açık Kaynak Motor Analizi, 16 Farklı Sis Türü Spesifikasyonu ve Uygulama Planı  
> **Konum:** `docs/SIS_VE_HACIMSEL_ATMOSFER_MIMARISI.md`  
> **Hazırlayan:** TulparEngine Baş Mimarı, Render & Atmosfer Çalışma Grubu  
> **Kapsanan Motorlar (25 Adet):** Unreal Engine 5, Unity (URP/HDRP), Godot 4, Google Filament, Gaijin Dagor Engine (EdenSpark), O3DE (Open 3D Engine), CryEngine, Valve Source 2, id Tech 6/7, Frostbite, Bevy, Flax Engine, Wicked Engine, PlayCanvas, Babylon.js, Three.js, Diligent Engine, Torque3D, Urho3D, The Forge, Banshee, Stride, Armory3D, Hazel, Raylib.  
> **Özel Sis Araştırmaları:** Inigo Quilez (Kapalı Form Üstel Yükseklik Sisi), Matej Lou (Analitik Hacimsel İhtimaller / Volumetric Primitives), Bart Wronski (SIGGRAPH 2014 Froxels), Sébastien Hillaire (SIGGRAPH 2016/2020 Atmosfer & Gökyüzü), Morgan McGuire & Louis Bavoil (WBOIT 2013).  
> **Sözleşme:** Sıfır Dinamik Tahsis (0-alloc), Mobil TBDR'da Tam 0 FPS Düşüşü (0 Bant Genişliği Maliyeti), Analitik Kapalı Form ALU Entegrali, PBR Doğrusal Uzay Uyumlu.

---

## 1. GİRİŞ VE STRATEJİK HEDEF

Oyun motorlarında "sis" tek bir kaydırma çubuğundan (slider) ibaret basit bir efekt değildir. Sinematik bir AAA oyunda ve ileri düzey bir oyun motorunda sis; açık dünya ufuk derinliğinden zindan mahzenlerine, bataklık zemin dumanından el feneri ışık hüzmelerine kadar **en az 16 farklı fiziksel ve görsel türe** ayrılır.

Ancak açık kaynak ekosisteminde geliştiriciler çoğunlukla iki aşırı tuzağa düşer:
1. **İlkel Karton Kutu Tuzağı (1995 Yaklaşımı):** Sisi bir atmosfer veya hacim olarak çözmek yerine, ekrana düz 2B karton kareler (quad billboards) atıp bunlara beyaz doku basmak. Bu yaklaşım ekranda duman veya sis yerine havada uçuşan "kare kare" gri plakalar üretir.
2. **Ağır 3B Froxel / Raymarch Tuzağı (Mobil Katili):** Masaüstü GPU'lar için tasarlanmış $160 \times 90 \times 64$ boyutunda 3B doku froxel ızgaraları mobil cihazlara (ARM Mali, Qualcomm Adreno) sokulduğunda, bellek bant genişliğini boğarak cihazı saniyeler içinde aşırı ısıtır ve FPS'yi çökertir.

**TulparEngine'in Amacı:**  
Dünyanın en iyi 25 açık kaynak motorunun ve matematiksel sis araştırmalarının senteziyle; **16 farklı sis türünü** destekleyen, **mobil cihazlarda tam 0 FPS düşüşü (< 0.01 ms GPU maliyeti)** sağlayan ve editörde tek tıkla kontrol edilebilen devrimsel bir hibrit sis ekosistemi kurmaktır.

---

## 2. OYUN MOTORLARINDAKİ 16 FARKLI SİS TÜRÜ VE MATEMATİKSEL ANATOMİSİ

Bir oyun motorunda bulunması gereken 16 sis türü, fiziksel temelleri ve TulparEngine'deki karşılıkları:

```
                            TULPAR FOG TAXONOMY (16 SİS TÜRÜ)
                                            │
        ┌───────────────────┬───────────────┴───────────────┬───────────────────┐
        ▼                   ▼                               ▼                   ▼
┌───────────────┐   ┌───────────────┐               ┌───────────────┐   ┌───────────────┐
│  KÜRESEL VE   │   │  HACİMSEL VE  │               │     IŞIK VE   │   │   TEMATİK VE  │
│  ATMOSFERİK   │   │    YEREL      │               │   SAÇILIMLI   │   │    OYNAYIŞ    │
├───────────────┤   ├───────────────┤               ├───────────────┤   ├───────────────┤
│1. Linear Dist │   │6. AABB Box Fog│               │10. God Rays   │   │13. Underwater │
│2. Exp Dist    │   │7. Sphere Fog  │               │11. Spot Cone  │   │14. Fog of War │
│3. Exp² Dist   │   │8. Animated    │               │12. Clustered  │   │15. Toxic/Magic│
│4. Height Fog  │   │9. Soft Part.  │               │    Injection  │   │16. Heat Haze  │
│5. Dual-Layer  │   │               │               │               │   │               │
└───────────────┘   └───────────────┘               └───────────────┘   └───────────────┘
```

### Grup A: Küresel ve Atmosferik Sisler (Global Atmospheric Fog)

#### 1. Doğrusal Mesafe Sisi (Linear Distance Fog)
- **Formül:** $f = \text{clamp}\left(\frac{d - d_{start}}{d_{end} - d_{start}}, 0, 1\right)$
- **Karakteristik:** Başlangıç ve bitiş mesafesi arasında doğrusal artış.
- **Kullanım:** Retro / Stylized oyunlar (PS1/N64 hissi) veya kesin mesafe sınırlaması gereken hafif sahneler.

#### 2. Üstel Mesafe Sisi (Exponential Distance Fog - Beer-Lambert Yasası)
- **Formül:** $f = 1 - e^{-d \cdot \rho}$ ($\rho$: sis yoğunluğu)
- **Karakteristik:** Fiziksel ışık sönümleme yasası. Mesafe arttıkça yumuşak ve doğal bir sis doyması sağlar.
- **Kullanım:** Standart açık dünya sahneleri.

#### 3. Üstel Kare Mesafe Sisi (Exponential Squared Fog - Exp²)
- **Formül:** $f = 1 - e^{-(d \cdot \rho)^2}$
- **Karakteristik:** Yakın mesafede son derece temiz ve berrak; ancak belirli bir kritik eşikten sonra aniden yoğunlaşan sis duvarı.
- **Kullanım:** Açık deniz ufkunda ada gizleme, sisli ormanlar, tekinsiz korku atmosferi.

#### 4. Üstel Yükseklik Sisi (Exponential Height Fog / Ground Fog - Inigo Quilez Modeli)
- **Formül:** $\rho(y) = \rho_0 e^{-\lambda (y - y_0)}$
- **Kapalı Form İntegrali:** $\tau = \frac{\rho_0}{\lambda} \cdot \frac{e^{-\lambda y_{cam}} - e^{-\lambda y_{pix}}}{y_{pix} - y_{cam}} \cdot d$
- **Karakteristik:** Vadilerde, dere yataklarında ve çukurlarda biriken; dağlara çıkıldıkça incelen gerçekçi zemin sisi.
- **Mobil Maliyeti:** **0 Doku, 0 Döngü, 0 FPS Kaybı!** (Tek formüllü saf ALU).

#### 5. Çift Kademeli Yükseklik Sisi (Dual-Layer Exponential Height Fog - Unreal Engine 5 Standardı)
- **Formül:** $\tau_{toplam} = \tau_1(y, d; \rho_1, \lambda_1) + \tau_2(y, d; \rho_2, \lambda_2)$
- **Karakteristik:** 
  - *1. Katman:* Zemin seviyesinde dar ve aşırı yoğun vadi sisi.
  - *2. Katman:* Gökyüzüne doğru uzanan çok hafif, seyreltik gök kubbe sisi.
- **Kullanım:** Dağlık arazilerde hem aşağıdaki nehri sisle kaplayıp hem de zirveden gökyüzünü görebilme imkanı sunar.

---

### Grup B: Sınırlandırılmış Hacimsel Yerel Sisler (Bounded Volumetric Volumes)

#### 6. Hacimsel Yerel Kutu Sisi (Local AABB Fog Volume)
- **Matematik:** Işın-AABB kesişimi ($t_{giris}, t_{cikis}$) + kenar yumuşatma fonksiyonu (boundary feathering).
- **Karakteristik:** Yalnızca belirli bir oda, mağara, zindan odası, su üstü veya kuyu tabanına hapsolmuş sis kutusu.
- **Kullanım:** Dışarıda güneşli hava varken içeriye girildiğinde başlayan mahzen sisi.

#### 7. Hacimsel Küresel Sis (Local Spherical Fog Volume - Matej Lou Modeli)
- **Matematik:** Küre merkezi $\mathbf{C}$ ve yarıçap $R$ için kuadratik yoğunluk integrali: $\rho(r) = \rho_0 \cdot \max\left(0, 1 - \frac{\|\mathbf{p} - \mathbf{C}\|^2}{R^2}\right)$
- **Karakteristik:** Raymarching gerektirmeyen kapalı formlu küre integrali.
- **Kullanım:** Zehirli gaz mantarları, duman bombası etki alanı, büyü çemberi sisi.

#### 8. Canlı Dalgalanan / Türbülanslı Sis (Animated Turbulent / Noise Fog)
- **Matematik:** $\rho'(\mathbf{p}, t) = \rho(\mathbf{p}) \cdot \left[1 + A \cdot \text{Simplex3D}\left(\mathbf{p} \cdot f + \vec{v}_{ruzgar} \cdot t\right)\right]$
- **Karakteristik:** Homojen ve donuk durmayan; rüzgarda kıvrılan, canlı nefes alan sis tabakası.
- **Kullanım:** Bataklıklar, mezarlık sahneleri, dinamik fırtına geçişleri.

#### 9. Yumuşak Parçacık Buhar ve Duman Sisi (Soft Particle Steam & Smoke Puffs)
- **Matematik:** Usulsel Gaussian radyal düşüş $e^{-3 r^2}$ + Sahne derinlik tamponuyla yumuşak sönüm: $\alpha = \min\left(1, \frac{Z_{sahne} - Z_{parcacik}}{\text{fade\_dist}}\right)$
- **Karakteristik:** 2B karton kutu quad'ların köşelerini yok eden, zeminle kesiştiğinde sert çizgi çizmeyen ipeksi buhar.
- **Kullanım:** Gayzerler, fabrika bacaları, patlama tozları, yangın dumanı.

---

### Grup C: Işık Etkileşimli ve Saçılımlı Sisler (Illuminated & Scattering Fog)

#### 10. Atmosferik Işık Hüzmeleri (Volumetric God Rays / Crepuscular Rays)
- **Matematik:** Schlick-Mie faz fonksiyonu: $P(\theta) = \frac{1 - g^2}{(1 + g - 2g \cos \theta)^{3/2}} \quad (g \approx 0.7 - 0.85)$
- **Karakteristik:** Güneşin önünü kapatan binaların, dağların ve ağaç dallarının arasından süzülen altın ışık kılıçları.
- **Tulpar Çözümü:** `rhi/shaders/godray.frag` (ekran uzayı radyal sönümlü, mobilde 0.2 ms).

#### 11. Tozlu Işık Konisi Sisi (Spotlight Volumetric Cone Fog)
- **Matematik:** Analitik ışık konisi poligonu + derinlik geçişli üstel ışık saçılımı.
- **Karakteristik:** El feneri, araba farı veya sahne spotunun havada asılı toz taneciklerini aydınlatması.
- **Kullanım:** Karanlık iç mekanlar, araba kovalamacaları, sinematik gerilim.

#### 12. Clustered Işık Enjeksiyonlu Sis (Clustered Light-Injected Fog)
- **Matematik:** Clustered Forward+ ızgarasında ($16\times 8\times 24$) yerel nokta ışıkların sis hacmine enerji katması.
- **Karakteristik:** Sisli bir sokakta sokak lambalarının etrafında oluşan renkli ışık haleleri (halo).

---

### Grup D: Tematik ve Oynanış Odaklı Sisler (Gameplay & Thematic Fog)

#### 13. Su Altı Sisi ve Dalgaboyu Sönümlemesi (Underwater Murk & Depth Fog)
- **Matematik:** Dalgaboyu bazlı Beer-Lambert sönümü: Kırmızı ışık 5 metrede yok olurken mavi-yeşil 50 metreye kadar ilerler ($\beta_R = 0.22, \beta_G = 0.05, \beta_B = 0.02$).
- **Kullanım:** Su altına girildiğinde derinleştikçe koyulaşan turkuaz/lacivert su altı atmosferi.

#### 14. Savaş Sisi (Fog of War)
- **Matematik:** 2B dinamik keşif ızgarası (Visibility Texture) üzerinden piksel bazlı karartma ve yumuşak kenar enterpolasyonu.
- **Kullanım:** Strateji (RTS), MOBA ve taktik RPG oyunlarında keşfedilmemiş alanları gizleme.

#### 15. Toksik / Mistik / Kimyasal Işıyan Sis (Emissive Chemical & Magic Fog)
- **Matematik:** Kendine ait ışıma enerjisi ($E_{emissive}$) taşıyan, ortam ışığı sıfır olsa bile karanlıkta parlayan hacim:
  $$\mathbf{C}_{final} = \mathbf{C}_{sahne} \cdot T + (\mathbf{C}_{fog} \cdot I_{ambient} + \mathbf{C}_{emissive}) \cdot (1 - T)$$
- **Kullanım:** Yeşil radyoaktif atık çukurları, mor büyü portalları, kızıl lav gazları.

#### 16. Sıcaklık Dalgalanması ve Serap Sisi (Heat Haze & Mirage Fog)
- **Matematik:** Sıcak hava yoğunluk gradyanının ($dn/dy$) neden olduğu kırılma ile ekran koordinatlarının UV perturbasyonu.
- **Kullanım:** Çöl zeminleri, roket motoru egzozları, lav kenarları.

---

## 3. 25 AÇIK KAYNAK MOTOR SİS KARŞILAŞTIRMA MATRİSİ

| # | Motor | Desteklenen Sis Türleri | Birincil Yöntem | Mobil TBDR Uyumu |
|---|---|---|---|---|
| 1 | **Unreal Engine 5** | 1, 2, 4, 5, 6, 8, 10, 11, 12 | Exponential Height Fog + 3D Froxels | Masaüstü kusursuz, mobilde froxel kapatılır |
| 2 | **Unity (URP/HDRP)** | 1, 2, 4, 6, 10 | URP: Analitik / HDRP: Froxel Volume | URP mobil dostu, HDRP ağır |
| 3 | **Godot Engine 4** | 1, 2, 4, 6, 7, 10 | Environment Fog + FogVolume | Compatibility modu analitik fallback |
| 4 | **Google Filament** | 2, 4, 10 | **Kapalı Form Analitik Yükseklik Sisi** | **Kusursuz (Referans Mobil PBR - 0 Doku)** |
| 5 | **Dagor Engine** | 4, 5, 8, 10 | daSkies çift üstel atmosfer | Konsol/PC mükemmel |
| 6 | **O3DE (Atom)** | 2, 4, 6, 10 | Volumetric Fog Gem | Ağır |
| 7 | **CryEngine** | 2, 4, 6, 7, 10 | Voxel Grid + Height Fog | PC odaklı |
| 8 | **Valve Source 2** | 2, 4, 6, 8, 10 | Flow-map analitik hacimler | VR / PC optimize |
| 9 | **id Tech 6/7** | 4, 10, 12 | Clustered Volumetric Scattering + TAA | Donanım canavarı |
| 10 | **Frostbite** | 4, 6, 10, 12 | Bart Wronski 2014 Froxels | Konsol/PC standardı |
| 11 | **Bevy Engine** | 1, 2, 3, 4 | `bevy_fog` Linear/Exp/Exp² | WebGPU / Mobil dostu |
| 12 | **Flax Engine** | 2, 4, 10 | Exponential Height Fog | Mobil uyumlu |
| 13 | **Wicked Engine** | 4, 6, 10 | Compute 3D Texture Pass | PC odaklı |
| 14 | **PlayCanvas** | 1, 2, 4 | WebGL fragment shader | Mükemmel mobil uyum |
| 15 | **Babylon.js** | 1, 2, 6 | Post-process & shader chunks | Web/Mobil optimize |
| 16 | **Three.js** | 1, 2, 3 | `Fog` & `FogExp2` | Çok hafif |
| 17 | **Diligent Engine**| 2, 4, 10 | Bruneton atmosfer örneği | PC/Mobil |
| 18 | **Torque3D** | 1, 4, 6 | Ground Fog | Eski PC |
| 19 | **Urho3D** | 1, 2, 4 | Zone-based fog | Mobil uyumlu |
| 20 | **The Forge** | 4, 10 | Cross-platform compute | Konsol/Mobil |
| 21 | **Banshee** | 2, 4, 6 | Analitik + yerel hacimler | PC |
| 22 | **Stride Engine** | 1, 2, 4 | Atmospheric & Height Fog | Mobil uyumlu |
| 23 | **Armory 3D** | 2, 4, 6 | Eevee benzeri shader düğümleri | Web/Mobil |
| 24 | **Hazel Engine** | 1, 2 | Basit derinlik doğrusal sisi | Minimalist |
| 25 | **Raylib** | 1, 2 | `raylib-shaders` mesafe sisi | Gömülü sistem dostu |

---

## 4. TULPARENGINE UYGULAMA VE ENTEGRASYON SÖZLEŞMESİ

TulparEngine'de bu 16 tür şu mimari omurgada birleştirilir:

1. **Çekirdek Atmosfer:** Google Filament ve Inigo Quilez analitik formülüyle **0 doku erişimli**, mobil TBDR'da **tam 0 FPS düşüşlü** Küresel Üstel Yükseklik Sisi (`Tür 4 & 5`).
2. **Yerel Hacimler:** Sahneye eklenebilen `kSceneFogVolume` ile AABB Kutu ve Küre sisleri (`Tür 6 & 7`).
3. **Işık Hüzmeleri:** `godray.frag` ile çeyrek çözünürlüklü radyal occlusion god ray hüzmeleri (`Tür 10`).
4. **Yumuşak Parçacık Dokusu:** Karton kutu quad'lar yerine bellekte usulsel üretilen 64x64 Gaussian dairesel radyal alfa dokusu (`Tür 9`).
5. **Editör Arayüzü:** "Dünya (World)" panelinde sanatçıya sunulan tam donanımlı sis laboratuvarı (Tür seçimi, yoğunluk, yükseklik düşüşü, renk, güneş saçılımı).
