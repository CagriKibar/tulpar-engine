#include "content/particles.hpp"
#include "content/particle_channel.hpp"
#include "content/particle_gaussian.hpp"
#include "content/particle_meshlet.hpp"
#include "content/particle_module.hpp"
#include "content/particle_rvt.hpp"
#include "content/ribbon.hpp"
#include "content/vfx_graph.hpp"
#include "audio/particle_audio.hpp"
#include "core/memory/arena.hpp"
#include "renderer/particle_gpu_indirect.hpp"
#include "renderer/wboit_pass.hpp"
#include "sim/mls_mpm.hpp"
#include "sim/neural_particles.hpp"
#include "sim/voxel_smoke.hpp"
#include "tests/test.hpp"

using namespace tulpar::engine;
using namespace tulpar::engine::content;
using namespace tulpar::engine::sim;
using namespace tulpar::engine::renderer;
using namespace tulpar::engine::audio;

ENGINE_TEST(particles_emit_stops_silently_at_capacity) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 16, "particles_cap"));
  ParticleSystem ps;
  CHECK(ps.init(sys, 2, Vec3{0, 0, 0}));

  ParticleEmitterConfig cfg;
  cfg.spawn_pos = Vec3{1, 2, 3};
  cfg.lifetime_min = cfg.lifetime_max = 1.0f;
  Rng rng(1234);

  CHECK(ps.emit(cfg, 3, rng) == 2); // kapasite 2 -- ucuncusu SESSIZCE kesildi
  CHECK(ps.alive_count() == 2);
  CHECK(nearly_equal(ps.particle(0).pos, Vec3{1, 2, 3}));
  CHECK(nearly_equal(ps.particle(0).lifetime, 1.0f));
}

ENGINE_TEST(particles_gravity_integration_matches_hand_trace) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 16, "particles_grav"));
  ParticleSystem ps;
  CHECK(ps.init(sys, 1, Vec3{0, -10.0f, 0}));

  ParticleEmitterConfig cfg;
  cfg.base_velocity = Vec3{0, 5, 0};
  cfg.lifetime_min = cfg.lifetime_max = 0.25f;
  Rng rng(42);
  CHECK(ps.emit(cfg, 1, rng) == 1);

  // adim 1: vel=(0,5,0)+(0,-10,0)*0.1=(0,4,0); pos=(0,0,0)+(0,4,0)*0.1=(0,0.4,0); age=0.1<0.25
  ps.update(0.1f);
  CHECK(ps.alive_count() == 1);
  CHECK(nearly_equal(ps.particle(0).vel, Vec3{0, 4.0f, 0}));
  CHECK(nearly_equal(ps.particle(0).pos, Vec3{0, 0.4f, 0}));

  // adim 2: vel=(0,3,0); pos=(0,0.7,0); age=0.2<0.25
  ps.update(0.1f);
  CHECK(ps.alive_count() == 1);
  CHECK(nearly_equal(ps.particle(0).vel, Vec3{0, 3.0f, 0}));
  CHECK(nearly_equal(ps.particle(0).pos, Vec3{0, 0.7f, 0}));

  // adim 3: age=0.3>=0.25 -> olur, havuzdan cikar
  ps.update(0.1f);
  CHECK(ps.alive_count() == 0);
}

ENGINE_TEST(particles_swap_removal_preserves_surviving_particles) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 16, "particles_swap"));
  ParticleSystem ps;
  CHECK(ps.init(sys, 3, Vec3{0, 0, 0})); // yercekimi yok -- konum sabit kalmali

  Rng rng(7);
  ParticleEmitterConfig cfg_a;
  cfg_a.spawn_pos = Vec3{1, 0, 0};
  cfg_a.lifetime_min = cfg_a.lifetime_max = 1.0f;
  CHECK(ps.emit(cfg_a, 1, rng) == 1); // index 0

  ParticleEmitterConfig cfg_b;
  cfg_b.spawn_pos = Vec3{2, 0, 0};
  cfg_b.lifetime_min = cfg_b.lifetime_max = 0.05f; // kisa omurlu -- ilk update'te olecek
  CHECK(ps.emit(cfg_b, 1, rng) == 1); // index 1

  ParticleEmitterConfig cfg_c;
  cfg_c.spawn_pos = Vec3{3, 0, 0};
  cfg_c.lifetime_min = cfg_c.lifetime_max = 1.0f;
  CHECK(ps.emit(cfg_c, 1, rng) == 1); // index 2

  ps.update(0.1f); // b (index1) olur -> c (index2) index1'e TAKAS edilir
  CHECK(ps.alive_count() == 2);
  // a hic tasinmadi (index0), c takasla index1'e geldi -- ikisi de BOZULMAMIS olmali.
  CHECK(nearly_equal(ps.particle(0).pos, Vec3{1, 0, 0}));
  CHECK(nearly_equal(ps.particle(1).pos, Vec3{3, 0, 0}));
}

ENGINE_TEST(particles_size_interpolates_linearly_over_life) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 16, "particles_size"));
  ParticleSystem ps;
  CHECK(ps.init(sys, 1, Vec3{0, 0, 0}));

  ParticleEmitterConfig cfg;
  cfg.lifetime_min = cfg.lifetime_max = 1.0f;
  cfg.size_start = 2.0f;
  cfg.size_end = 10.0f;
  Rng rng(5);
  CHECK(ps.emit(cfg, 1, rng) == 1);
  CHECK(nearly_equal(ps.particle(0).size, 2.0f)); // dogum ani -- size_start

  ps.update(0.25f); // t=0.25 -> 2 + (10-2)*0.25 = 4
  CHECK(nearly_equal(ps.particle(0).size, 4.0f));

  ps.update(0.25f); // t=0.5 -> 2 + 8*0.5 = 6
  CHECK(nearly_equal(ps.particle(0).size, 6.0f));
}

ENGINE_TEST(particles_velocity_jitter_stays_within_configured_bounds) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 16, "particles_jitter"));
  ParticleSystem ps;
  CHECK(ps.init(sys, 32, Vec3{0, 0, 0}));

  ParticleEmitterConfig cfg;
  cfg.velocity_jitter = Vec3{1.0f, 1.0f, 1.0f};
  cfg.lifetime_min = 0.5f;
  cfg.lifetime_max = 1.5f;
  Rng rng(777);
  CHECK(ps.emit(cfg, 32, rng) == 32);

  for (uint32_t i = 0; i < 32; i++) {
    const Particle &p = ps.particle(i);
    CHECK(p.vel.x >= -1.0f && p.vel.x <= 1.0f);
    CHECK(p.vel.y >= -1.0f && p.vel.y <= 1.0f);
    CHECK(p.vel.z >= -1.0f && p.vel.z <= 1.0f);
    CHECK(p.lifetime >= 0.5f && p.lifetime < 1.5f);
  }
}

ENGINE_TEST(particles_same_seed_yields_bit_identical_emit_sequence) {
  SystemArena sys1;
  CHECK(sys1.reserve(1u << 16, "particles_det1"));
  SystemArena sys2;
  CHECK(sys2.reserve(1u << 16, "particles_det2"));
  ParticleSystem ps1, ps2;
  CHECK(ps1.init(sys1, 8, Vec3{0, -9.8f, 0}));
  CHECK(ps2.init(sys2, 8, Vec3{0, -9.8f, 0}));

  ParticleEmitterConfig cfg;
  cfg.base_velocity = Vec3{0, 2, 0};
  cfg.velocity_jitter = Vec3{0.5f, 0.5f, 0.5f};
  cfg.lifetime_min = 1.0f;
  cfg.lifetime_max = 2.0f;
  Rng rng1(999), rng2(999); // AYNI tohum

  CHECK(ps1.emit(cfg, 8, rng1) == 8);
  CHECK(ps2.emit(cfg, 8, rng2) == 8);
  for (uint32_t i = 0; i < 8; i++) {
    CHECK(nearly_equal(ps1.particle(i).vel, ps2.particle(i).vel));
    CHECK(nearly_equal(ps1.particle(i).lifetime, ps2.particle(i).lifetime));
  }
}

ENGINE_TEST(particles_drag_and_curl_turbulence_integration) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 16, "particles_drag"));
  ParticleSystem ps;
  CHECK(ps.init(sys, 4, Vec3{0, 0, 0}));

  ParticleEmitterConfig cfg;
  cfg.base_velocity = Vec3{10.0f, 0.0f, 0.0f};
  cfg.drag = 2.0f;
  cfg.curl_noise_strength = 4.0f;
  cfg.curl_noise_frequency = 0.5f;
  cfg.lifetime_min = cfg.lifetime_max = 1.0f;
  Rng rng(42);

  CHECK(ps.emit(cfg, 1, rng) == 1);
  const float dt = 0.1f;
  ps.update(dt);

  // drag = 2.0f, dt = 0.1f -> drag factor = 1 - 2*0.1 = 0.8
  // 10.0 * 0.8 = 8.0, arti curl noise katkisi
  const Particle &p = ps.particle(0);
  CHECK(p.vel.x < 10.0f); // surtunme ile yavasladi
  CHECK(p.age == 0.1f);
  CHECK(p.subuv_frame == 0.1f);
}

ENGINE_TEST(particles_ground_collision_bounces_with_restitution_and_friction) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 16, "particles_bounce"));
  ParticleSystem ps;
  CHECK(ps.init(sys, 2, Vec3{0, 0, 0}));

  ParticleEmitterConfig cfg;
  cfg.spawn_pos = Vec3{0.0f, 0.2f, 0.0f};
  cfg.base_velocity = Vec3{10.0f, -4.0f, 0.0f};
  cfg.enable_collision = true;
  cfg.collision_plane_y = 0.0f;
  cfg.restitution = 0.5f; // %50 sekme
  cfg.friction = 0.3f;    // %30 yanal kayip
  cfg.lifetime_min = cfg.lifetime_max = 1.0f;
  Rng rng(11);

  CHECK(ps.emit(cfg, 1, rng) == 1);
  // dt = 0.1f -> pos.y = 0.2 + (-4.0)*0.1 = -0.2 <= 0.0f -> carpisma!
  ps.update(0.1f);

  const Particle &p = ps.particle(0);
  CHECK(nearly_equal(p.pos.y, 0.0f));
  CHECK(nearly_equal(p.vel.y, 2.0f)); // -(-4.0) * 0.5 = 2.0f
  CHECK(nearly_equal(p.vel.x, 7.0f)); // 10.0 * (1 - 0.3) = 7.0f
}

ENGINE_TEST(ribbon_trail_geometry_and_tangents) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 16, "ribbon_test"));
  RibbonTrail trail;
  CHECK(trail.init(sys, 16));

  trail.add_point({0, 0, 0}, {1, 1, 1, 1}, 0.5f, 1.0f);
  trail.add_point({1, 0, 0}, {1, 1, 1, 1}, 0.5f, 1.0f);
  trail.add_point({2, 0, 0}, {1, 1, 1, 1}, 0.5f, 1.0f);
  CHECK(trail.count() == 3);

  RibbonVertex vertices[16];
  uint32_t indices[32];
  uint32_t vcount = 0, icount = 0;

  trail.build_geometry({0, 0, 10}, vertices, vcount, indices, icount);
  CHECK(vcount == 6);  // 3 nokta x 2 vertex
  CHECK(icount == 12); // 2 serit x 6 indeks

  // UV uclari
  CHECK(nearly_equal(vertices[0].uv.x, 0.0f));
  CHECK(nearly_equal(vertices[4].uv.x, 1.0f));
}

ENGINE_TEST(gaussian_particles_covariance_and_velocity_stretch) {
  ParticleGaussian g;
  g.pos = {1.0f, 2.0f, 3.0f};
  g.scale = {0.2f, 0.2f, 0.2f};
  g.rot = Quat::identity();

  Mat3Sym cov = g.compute_covariance();
  CHECK(cov.determinant() > 0.0f); // Pozitif yari-belirli (SPSD)

  // Merkezde yogunluk tam 1.0
  float center_density = g.evaluate_density(g.pos);
  CHECK(nearly_equal(center_density, 1.0f, 1e-3f));

  // Uzakta yogunluk 0
  float far_density = g.evaluate_density({10.0f, 10.0f, 10.0f});
  CHECK(far_density < 1e-4f);

  // Hiz yonunde esneme
  g.velocity = {10.0f, 0.0f, 0.0f};
  g.stretch_along_velocity(0.5f);
  CHECK(g.scale.z > g.scale.x); // Z ekseni hiza gore uzatildi
}

ENGINE_TEST(particle_meshlet_clustering_and_culling) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 18, "meshlet_test"));
  ParticleSystem ps;
  CHECK(ps.init(sys, 100, Vec3{0, 0, 0}));

  ParticleEmitterConfig cfg;
  cfg.lifetime_min = cfg.lifetime_max = 5.0f;
  Rng rng(99);
  CHECK(ps.emit(cfg, 100, rng) == 100);

  ParticleMeshletBuilder builder;
  CHECK(builder.init(sys, 100));
  builder.build_clusters(ps);

  // 100 parcacik -> 64 + 36 = 2 kume
  CHECK(builder.cluster_count() == 2);
  CHECK(builder.cluster(0).particle_count == 64);
  CHECK(builder.cluster(1).particle_count == 36);
}

ENGINE_TEST(cs2_voxel_smoke_bullet_carve_and_diffuse) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 20, "smoke_test"));
  VoxelSmokeGrid smoke;
  CHECK(smoke.init(sys, 16, 16, 16, {0, 0, 0}, 0.5f));

  // Duman enjekte et
  smoke.inject_smoke({4.0f, 4.0f, 4.0f}, 2.0f, 1.0f);
  float d_center = smoke.sample_density({4.0f, 4.0f, 4.0f});
  CHECK(d_center > 0.5f);

  // Mermi deligi ac (X ekseni boyunca hat delme)
  smoke.carve_bullet_tunnel({0.0f, 4.0f, 4.0f}, {1.0f, 0.0f, 0.0f}, 8.0f, 0.6f);
  float d_after_bullet = smoke.sample_density({4.0f, 4.0f, 4.0f});
  CHECK(d_after_bullet == 0.0f);

  // Difuzyon adimi: etraftaki duman deligi geri doldurur
  VoxelSmokeParams sp;
  sp.diffusion_rate = 0.5f;
  sp.dissipation_rate = 0.0f;
  smoke.update(0.3f, sp);

  float d_refilled = smoke.sample_density({4.0f, 4.0f, 4.0f});
  CHECK(d_refilled > 0.0f); // delik geri dolmaya basladi
}

ENGINE_TEST(mls_mpm_simulation_and_mass_conservation) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 20, "mpm_test"));
  MlsMpmSimulator mpm;
  CHECK(mpm.init(sys, 16, 16, 16, 32, 0.25f, {0, 0, 0}));

  // 8 parcacik ekle
  for (int i = 0; i < 8; i++) {
    CHECK(mpm.add_particle({1.0f + (float)i * 0.1f, 2.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, 1.0f));
  }
  CHECK(mpm.particle_count() == 8);
  CHECK(nearly_equal(mpm.total_mass(), 8.0f));

  // 5 adim simule et
  for (int step = 0; step < 5; step++) {
    mpm.step(0.01f, {0.0f, -9.8f, 0.0f});
  }

  // Kutle korunumu: parcacik kutlesi hicbir zaman yok olmaz
  CHECK(nearly_equal(mpm.total_mass(), 8.0f, 1e-4f));

  // Zemin guvenlik siniri: parcaciklar yerin dibine gecemez
  for (uint32_t i = 0; i < mpm.particle_count(); i++) {
    CHECK(mpm.particle(i).pos.y >= 0.5f);
  }
}

ENGINE_TEST(particles_subemitter_spawn_on_death_and_collision) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 18, "subemitter_test"));
  ParticleSystem parent_ps, child_ps;
  CHECK(parent_ps.init(sys, 4, Vec3{0, 0, 0}));
  CHECK(child_ps.init(sys, 32, Vec3{0, 0, 0}));

  ParticleEmitterConfig child_cfg;
  child_cfg.lifetime_min = child_cfg.lifetime_max = 1.0f;
  child_cfg.base_velocity = Vec3{0, 1, 0};

  ParticleEmitterConfig parent_cfg;
  parent_cfg.lifetime_min = parent_cfg.lifetime_max = 0.1f; // 0.1s sonra olecek
  parent_cfg.spawn_on_death_count = 4;
  parent_cfg.sub_emitter = &child_ps;
  parent_cfg.sub_emitter_cfg = &child_cfg;

  Rng rng(123);
  CHECK(parent_ps.emit(parent_cfg, 1, rng) == 1);
  CHECK(child_ps.alive_count() == 0);

  // 0.15s sonra parent_ps parcacigi olur, child_ps icinde 4 parcacik dogar
  parent_ps.update(0.15f, &rng);
  CHECK(parent_ps.alive_count() == 0);
  CHECK(child_ps.alive_count() == 4);
}

ENGINE_TEST(particle_data_channels_and_wetness_map) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 18, "channel_test"));

  // 1. Data Channel Kuyrugu
  ParticleDataChannelQueue queue;
  CHECK(queue.init(sys, 16));
  CHECK(queue.push_damage({1, 2, 3}, 25.0f, 42));
  CHECK(queue.push_impulse({0, 1, 0}, {0, 10, 0}, 100));
  CHECK(queue.count() == 2);

  ParticleDataEvent ev1, ev2;
  CHECK(queue.pop(ev1));
  CHECK(ev1.channel == kChannelDamage);
  CHECK(ev1.value == 25.0f);
  CHECK(ev1.target_id == 42);

  CHECK(queue.pop(ev2));
  CHECK(ev2.channel == kChannelPhysicsImpulse);
  CHECK(nearly_equal(ev2.impulse.y, 10.0f));

  // 2. Dinamik Zemin Islanmasi (Wetness Map)
  GroundWetnessMap wetness;
  CHECK(wetness.init(sys, {0, 0, 0}, 10.0f));

  wetness.add_splash({5.0f, 0.0f, 5.0f}, 1.0f, 0.8f);
  CHECK(wetness.sample_wetness({5.0f, 0.0f, 5.0f}) > 0.5f);
  CHECK(wetness.sample_wetness({0.0f, 0.0f, 0.0f}) == 0.0f);

  // Kuruma adimi
  wetness.update(2.0f, 0.2f);
  CHECK(wetness.sample_wetness({5.0f, 0.0f, 5.0f}) < 0.8f);
}

ENGINE_TEST(renderer_wboit_weight_composite_and_motion_vectors) {
  // Derinlik agirligi: yakin mesafede buyuk, uzakta sonumlenmeli
  float w_near = compute_wboit_weight(1.0f, 0.5f);
  float w_far = compute_wboit_weight(50.0f, 0.5f);
  CHECK(w_near > 0.0f);
  CHECK(w_near > w_far);

  // Parcacik fragment degerlendirmesi
  WboitFragmentOutput frag = evaluate_wboit_fragment({1.0f, 0.0f, 0.0f, 0.5f}, 2.0f);
  CHECK(frag.accum.w > 0.0f);
  CHECK(nearly_equal(frag.revealage, 0.5f));

  // Kompozit harmanlama
  Vec4 bg{0.0f, 0.0f, 0.0f, 1.0f};
  Vec4 comp = composite_wboit(frag.accum, frag.revealage, bg);
  CHECK(comp.x > 0.0f); // Kirmizi katki yapildi

  // Hareket vektoru
  Vec4 clip_cur{0.2f, 0.4f, 1.0f, 1.0f};
  Vec4 clip_prev{0.0f, 0.0f, 1.0f, 1.0f};
  Vec2 mv = compute_particle_motion_vector(clip_cur, clip_prev);
  CHECK(mv.x > 0.0f && mv.y > 0.0f);
}

ENGINE_TEST(sim_npa_neural_particle_automata_and_wendland) {
  // Wendland C4
  CHECK(wendland_c4(0.0f, 1.0f) > 0.0f);
  CHECK(wendland_c4(1.5f, 1.0f) == 0.0f);

  // Morton 30 bit
  uint32_t m1 = morton30(0, 0, 0);
  uint32_t m2 = morton30(1, 0, 0);
  CHECK(m1 == 0);
  CHECK(m2 > 0);

  // NPA Simulatru
  SystemArena sys;
  CHECK(sys.reserve(1u << 18, "npa_test"));
  NeuralParticleAutomata npa;
  CHECK(npa.init(sys, 16, 1.0f));

  npa.add_particle({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
  npa.add_particle({0.5f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
  CHECK(npa.count() == 2);

  DenseLayer identity_layer{}; // bos ag ile adveksiyon ve komsu testi
  npa.step(0.05f, identity_layer);

  // Morfojenez hedef konuma cekim
  CHECK(npa.particle(0).pos.x > 0.0f);
}

ENGINE_TEST(renderer_gpu_indirect_and_tbdr_binning) {
  ParticleDispatchIndirectCommand dispatch;
  ParticleDrawIndexedIndirectCommand draw;

  TbdrParticleBinner::compute_dispatch_args(128, dispatch, draw);
  CHECK(dispatch.x == 2); // 128 / 64 = 2 threadgroup
  CHECK(draw.instance_count == 128);
  CHECK(draw.index_count == 6);

  HanabiBufferSlots slots;
  CHECK(slots.read_slot == 0 && slots.write_slot == 1);
  slots.swap();
  CHECK(slots.read_slot == 1 && slots.write_slot == 0);

  uint32_t tx = 0, ty = 0;
  CHECK(TbdrParticleBinner::project_to_tile({32.0f, 48.0f}, 1920, 1080, tx, ty));
  CHECK(tx == 2 && ty == 3); // 32/16 = 2, 48/16 = 3
}

ENGINE_TEST(audio_particle_sonifier_impact_frequencies) {
  // Su damlasi: alcak kabarcik tonu
  auto water_acoustics = ParticleSonifier::evaluate_impact(0.01f, 5.0f, ParticleSurfaceMaterial::kWater);
  CHECK(water_acoustics.frequency_hz >= 500.0f && water_acoustics.frequency_hz <= 800.0f);

  // Metal: tiz rezonans
  auto metal_acoustics = ParticleSonifier::evaluate_impact(0.05f, 10.0f, ParticleSurfaceMaterial::kMetal);
  CHECK(metal_acoustics.frequency_hz >= 2500.0f);
  CHECK(metal_acoustics.decay_time > water_acoustics.decay_time);

  // Sentez dalga ornegi
  float sample = ParticleSonifier::generate_sample(0.001f, metal_acoustics);
  CHECK(!std::isnan(sample));
}

ENGINE_TEST(content_persistent_debris_rvt_baking) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 18, "rvt_test"));
  PersistentDebrisBaker baker;
  CHECK(baker.init(sys, 32, 32, {0, 0, 0}, 10.0f));

  // Enkaz damgala
  baker.bake_particle({5.0f, 0.0f, 5.0f}, 0.5f, {0.8f, 0.2f, 0.1f, 1.0f}, 0.05f);

  float thick = baker.sample_thickness({5.0f, 0.0f, 5.0f});
  CHECK(thick > 0.01f);

  Vec4 col = baker.sample_color({5.0f, 0.0f, 5.0f});
  CHECK(col.x > 0.5f); // Kirmizimsi yaprak/moloz rengi
}

ENGINE_TEST(particles_emission_shapes_sample_expected_bounds) {
  SystemArena sys;
  CHECK(sys.reserve(1u << 18, "particles_shapes"));
  ParticleSystem ps;
  CHECK(ps.init(sys, 64, Vec3{0, 0, 0}));
  Rng rng(42);

  // 1. Duzlemsel Halka (Planar Ring): XZ duzleminde kalmali, Y degismemeli
  ParticleEmitterConfig ring_cfg;
  ring_cfg.spawn_pos = Vec3{0, 5, 0};
  ring_cfg.emission_shape = ParticleEmissionShape::PlanarRing;
  ring_cfg.emission_radius = 4.0f;
  ring_cfg.emission_inner_radius = 2.0f;
  CHECK(ps.emit(ring_cfg, 10, rng) == 10);
  for (uint32_t i = 0; i < 10; i++) {
    const Particle &p = ps.particle(i);
    CHECK(nearly_equal(p.pos.y, 5.0f)); // Y degismemeli
    const float dist_xz = std::sqrt(p.pos.x * p.pos.x + p.pos.z * p.pos.z);
    CHECK(dist_xz >= 1.95f && dist_xz <= 4.05f); // 2..4 yaricapinda
  }

  // 2. Kuresel Hacim (Spherical Volume)
  ps.clear();
  ParticleEmitterConfig sphere_cfg;
  sphere_cfg.spawn_pos = Vec3{10, 10, 10};
  sphere_cfg.emission_shape = ParticleEmissionShape::SphericalVolume;
  sphere_cfg.emission_radius = 3.0f;
  CHECK(ps.emit(sphere_cfg, 20, rng) == 20);
  for (uint32_t i = 0; i < 20; i++) {
    const Particle &p = ps.particle(i);
    const float dist = length(p.pos - Vec3{10, 10, 10});
    CHECK(dist <= 3.05f); // kure icinde
  }
}

ENGINE_TEST(particles_simulation_mode_heuristic_resolution) {
  ParticleEmitterConfig cfg;
  cfg.simulation_mode = ParticleSimulationMode::Auto;

  // Az parcacik + carpisma yok -> CPU
  cfg.enable_collision = false;
  CHECK(resolve_simulation_mode(cfg, 500) == ParticleSimulationMode::Cpu);

  // Cok parcacik (>=2000) + serbest akis -> GPU
  CHECK(resolve_simulation_mode(cfg, 2500) == ParticleSimulationMode::Gpu);

  // Cok parcacik ama carpisma var -> CPU fallback
  cfg.enable_collision = true;
  CHECK(resolve_simulation_mode(cfg, 5000) == ParticleSimulationMode::Cpu);

  // Zorunlu mod secildiginde otomatik ezilmez
  cfg.simulation_mode = ParticleSimulationMode::Gpu;
  CHECK(resolve_simulation_mode(cfg, 100) == ParticleSimulationMode::Gpu);
}

ENGINE_TEST(particles_vfx_module_pipeline_executes_in_order) {
  VfxModulePipeline pipeline;
  StokesDragModule drag_mod;
  drag_mod.drag = 0.5f;
  GroundCollisionModule coll_mod;
  coll_mod.plane_y = 0.0f;
  coll_mod.restitution = 0.5f;
  SizeOverLifeModule size_mod;
  size_mod.size_start = 1.0f;
  size_mod.size_end = 0.2f;

  CHECK(pipeline.add_module(&drag_mod));
  CHECK(pipeline.add_module(&coll_mod));
  CHECK(pipeline.add_module(&size_mod));
  CHECK(pipeline.count() == 3);

  Particle p;
  p.pos = Vec3{0.0f, -0.5f, 0.0f}; // zeminin altina girmeye calisiyor
  p.vel = Vec3{10.0f, -4.0f, 0.0f};
  p.age = 0.5f;
  p.lifetime = 1.0f;

  ParticleEmitterConfig cfg;
  pipeline.execute_particle_update(p, 0.1f, cfg);

  // Drag hiz azaltmali
  CHECK(p.vel.x < 10.0f);
  // GroundCollision zemine sabitleyip Y hizini ters cevirmeli
  CHECK(nearly_equal(p.pos.y, 0.0f));
  CHECK(p.vel.y > 0.0f);
  // Size 1.0 -> 0.2 omur yarisi (~0.6) olmali
  CHECK(p.size < 1.0f && p.size > 0.2f);
}

ENGINE_TEST(particles_vfx_graph_compiles_to_emitter_config_and_glsl) {
  VfxGraph graph;
  graph.build_fire_graph();
  CHECK(graph.node_count() >= 5);
  CHECK(graph.connection_count() >= 4);

  ParticleEmitterConfig cfg;
  CHECK(graph.compile_to_config(cfg));
  CHECK(nearly_equal(cfg.base_velocity.y, 2.8f));
  CHECK(nearly_equal(cfg.gravity.y, -9.8f));
  CHECK(cfg.emission_shape == ParticleEmissionShape::PlanarRing);
  CHECK(nearly_equal(cfg.curl_noise_strength, 2.5f));

  char glsl[512]{};
  size_t written = graph.compile_to_glsl_compute(glsl, sizeof(glsl));
  CHECK(written > 50);
  CHECK(std::strstr(glsl, "#version 450") != nullptr);
  CHECK(std::strstr(glsl, "ParticleBuffer") != nullptr);
}

ENGINE_TEST(renderer_wboit_emissive_additive_extension_math) {
  Vec4 flame_color{1.0f, 0.6f, 0.2f, 0.4f}; // parlak alev, %40 alpha
  float depth = 10.0f;

  WboitFragmentOutput standard = evaluate_wboit_fragment(flame_color, depth);
  WboitFragmentOutput emissive = evaluate_wboit_fragment_emissive(flame_color, depth, 0.5f);

  // Emissive additivite, renk akümülatörüne daha fazla isik enjekte etmeli
  CHECK(emissive.accum.x > standard.accum.x);
  CHECK(emissive.accum.w > standard.accum.w);
  // Ancak arka plan gecirgenligini standart alfa gibi tamamen karartmamali
  CHECK(emissive.revealage >= standard.revealage * 0.8f);
}



