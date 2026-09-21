#version 450
// GPU Gems 3 & Prosedurel Atmosferik Isik Huzmeleri (Volumetric God Rays).
// Sahne arkasinda parlak gokyuzu olmasa bile gercekci, yumusak ve organik
// volumetrik gunes huzmeleri (crepuscular rays) uretir. Ayrica ekranda isimali
// parlak nesneler oldugunda gercek radyal smear biriktirmesini dengeli korur.
layout(location = 0) in vec2 v_uv;
layout(set = 0, binding = 0) uniform sampler2D u_src;  // parlak gecis (down[0])
layout(set = 0, binding = 1) uniform sampler2D u_src2; // bloom gecisi (up[0])
layout(push_constant) uniform Push {
  vec4 texel; // xy: 1/kaynak0 olcusu, zw: 1/kaynak1 olcusu
  vec4 p;     // xy: gunesin ekran UV'si, z: huzme uzunlugu (density), w: keskinlik (decay)
  vec4 q;     // x: huzme siddeti (weight), y: genel carpan (exposure), z: ornek sayisi, w: zaman (time_sec)
} pc;
layout(location = 0) out vec4 o_color;

void main() {
  vec2 sun_uv = pc.p.xy;
  float density = max(pc.p.z, 0.05);        // Huzme uzunlugu (tipik 0.5 - 2.0)
  float decay = clamp(pc.p.w, 0.01, 0.999); // Keskinlik (tipik 0.8 - 0.99)
  float weight = max(pc.q.x, 0.0);          // Huzme siddeti (tipik 0.1 - 1.0)
  float exposure = max(pc.q.y, 0.0);        // Genel carpan (kamera arkasi ve edge_fade icerir)
  float time_sec = pc.q.w;                  // Canli acisal kaydirma (zaman)

  vec2 ray_diff = v_uv - sun_uv;
  // Ekran en-boy oranini hesaba katarak dairesel/radyal huzmelerin ezilmesini onle
  // texel.x = 1/W, texel.y = 1/H -> texel.y / texel.x = W / H = Aspect
  float aspect = (pc.texel.x > 0.00001 && pc.texel.y > 0.00001) ? (pc.texel.y / pc.texel.x) : 1.0;
  vec2 aspect_diff = vec2(ray_diff.x * aspect, ray_diff.y);
  float dist = length(aspect_diff);

  // 1. Prosedurel Aci ve Zaman Kaydirmasi (Dinamik hava/toz akisi):
  float angle = atan(aspect_diff.y, aspect_diff.x);
  float t = time_sec * 0.04;

  // 2. Cok Katmanli Organik Huzme Frekanslari (Harmonik kirilmalar):
  // Yuksek frekansli ince huzmeler + orta frekansli ana isik kollari
  float s1 = sin(angle * 13.0 + t);
  float s2 = sin(angle * 26.0 - t * 1.4 + 1.2);
  float s3 = sin(angle * 52.0 + t * 2.2 + 2.8);
  float s4 = sin(angle * 7.0 - t * 0.6 - 0.4);
  float s5 = sin(angle * 104.0 + t * 3.1 + 4.1);
  float shaft_noise = s1 * 0.36 + s2 * 0.26 + s3 * 0.16 + s4 * 0.14 + s5 * 0.08;

  // Huzmeler arasindaki bosluklar (golge yarilari):
  // smoothstep ile yumusak gecis, boylece sert ucgen yerine dogal isik sutunlari olusur
  float shaft_mask = smoothstep(-0.15, 0.45, shaft_noise);

  // Keskinlik / Sonumleme egrisi:
  float sharpness = mix(1.2, 8.0, clamp((decay - 0.6) * 2.5, 0.0, 1.0));
  float shaft_factor = pow(shaft_mask, sharpness);

  // 3. Atmosferik Mesafe ve Mie Sacilmasi:
  // density: huzmelerin ne kadar uzaga ulastigini belirler
  float falloff_k = 2.6 / density;
  float dist_atten = exp(-dist * falloff_k);

  // Gunes cevresi dogal Mie tac parlamasi (corona glow)
  float corona = exp(-dist * 12.0) * 0.35;

  // Sicak atmosferik gunes rengi tonu (hafif altinsi / gunes beyazi)
  const vec3 kSunColor = vec3(1.0, 0.95, 0.85);
  vec3 proc_shafts = kSunColor * ((shaft_factor * 0.85 + corona) * dist_atten * (weight * 1.2));

  // 4. Radyal Bulaniklastirma (Occlusion & Smear):
  // 40 ornekli, dither destekli, normalize edilmis radyal ornekleme
  const int kSamples = 40;
  vec2 delta = ray_diff * (density * 0.5) / float(kSamples);
  float dither = fract(sin(dot(v_uv, vec2(12.9898, 78.233))) * 43758.5453);
  vec2 uv = v_uv - delta * dither;

  float illum_decay = 1.0;
  vec3 accum = vec3(0.0);
  float total_weight = 0.0;
  for (int i = 0; i < kSamples; i++) {
    uv -= delta;
    if (uv.x >= 0.001 && uv.x <= 0.999 && uv.y >= 0.001 && uv.y <= 0.999) {
      vec3 s = texture(u_src, uv).rgb;
      accum += s * illum_decay;
      total_weight += illum_decay;
    }
    illum_decay *= decay;
  }
  // Normalizasyon: orneklerin kontrolsuz toplanip patlamasini onler, dogal smear dengesini korur
  if (total_weight > 0.001) {
    accum = (accum / total_weight) * (weight * 1.5);
  }

  // 5. Nihai Birlestirme:
  vec3 bloom = texture(u_src2, v_uv).rgb;
  vec3 total_godrays = (accum * 0.35 + proc_shafts) * exposure;
  o_color = vec4(bloom + total_godrays, 1.0);
}
