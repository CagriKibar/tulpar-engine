// L6 CONTENT — 3D Gaussian Splatting Partikülleri (PhysGaussian / Anisotropic Gaussians).
// 2026 SIGGRAPH / 3DGS VFX arastirmalarinin motor entegrasyonu.
// 2B billboard kartonlar yerine 3B anizotropik Gauss elipsoidleri kullanilir.
// Kovaryans matrisi Sigma = R * S * S^T * R^T uzerinden her yonden hacimsel
// derinlik, hiza gore esneme ve yumusak kesisim saglar.
#pragma once
#include <cmath>
#include <cstdint>

#include "core/math/vec.hpp"
#include "core/memory/arena.hpp"

namespace tulpar::engine::content {

// Simetrik 3x3 Kovaryans Matrisi (ust ucgen 6 eleman)
struct Mat3Sym {
  float c00 = 1.0f; // cov(x, x)
  float c01 = 0.0f; // cov(x, y)
  float c02 = 0.0f; // cov(x, z)
  float c11 = 1.0f; // cov(y, y)
  float c12 = 0.0f; // cov(y, z)
  float c22 = 1.0f; // cov(z, z)

  // Determinant: det(Sigma)
  float determinant() const {
    return c00 * (c11 * c22 - c12 * c12) -
           c01 * (c01 * c22 - c12 * c02) +
           c02 * (c01 * c12 - c11 * c02);
  }

  // Tersi: Sigma^-1 (ters kovaryans / hassasiyet matrisi)
  bool invert(Mat3Sym &out_inv) const {
    const float det = determinant();
    if (std::fabs(det) < 1e-8f) return false;
    const float inv_det = 1.0f / det;

    out_inv.c00 = (c11 * c22 - c12 * c12) * inv_det;
    out_inv.c01 = (c02 * c12 - c01 * c22) * inv_det;
    out_inv.c02 = (c01 * c12 - c02 * c11) * inv_det;
    out_inv.c11 = (c00 * c22 - c02 * c02) * inv_det;
    out_inv.c12 = (c02 * c01 - c00 * c12) * inv_det;
    out_inv.c22 = (c00 * c11 - c01 * c01) * inv_det;
    return true;
  }
};

struct ParticleGaussian {
  Vec3 pos{};            // Ortalama konum (mu)
  Vec3 scale{0.1f, 0.1f, 0.1f}; // 3B yaricap olcekleri (s_x, s_y, s_z)
  Quat rot = Quat::identity();  // Yonelim kuaterniyonu (q)
  Vec4 color{1.0f, 1.0f, 1.0f, 1.0f}; // RGBA
  Vec3 velocity{};       // Hiz vektoru
  float lifetime = 1.0f;
  float age = 0.0f;

  // Sigma = R * S * S^T * R^T kovaryans matrisini hesaplar.
  // Matematiksel kanit: M = R * S alinirsa, Sigma = M * M^T oldugundan
  // Sigma her zaman simetrik pozitif yari-belirlidir (SPSD).
  Mat3Sym compute_covariance() const {
    // R matrisini kuaterniyondan cikar (sutun bazli r0, r1, r2)
    const float xx = rot.x * rot.x, yy = rot.y * rot.y, zz = rot.z * rot.z;
    const float xy = rot.x * rot.y, xz = rot.x * rot.z, yz = rot.y * rot.z;
    const float wx = rot.w * rot.x, wy = rot.w * rot.y, wz = rot.w * rot.z;

    // R sutunlari
    const Vec3 r0{1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy)};
    const Vec3 r1{2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx)};
    const Vec3 r2{2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy)};

    // M = R * S (olcekli sutunlar)
    const Vec3 m0 = r0 * scale.x;
    const Vec3 m1 = r1 * scale.y;
    const Vec3 m2 = r2 * scale.z;

    // Sigma = M * M^T = m0*m0^T + m1*m1^T + m2*m2^T
    Mat3Sym cov;
    cov.c00 = m0.x * m0.x + m1.x * m1.x + m2.x * m2.x;
    cov.c01 = m0.x * m0.y + m1.x * m1.y + m2.x * m2.y;
    cov.c02 = m0.x * m0.z + m1.x * m1.z + m2.x * m2.z;
    cov.c11 = m0.y * m0.y + m1.y * m1.y + m2.y * m2.y;
    cov.c12 = m0.y * m0.z + m1.y * m1.z + m2.y * m2.z;
    cov.c22 = m0.z * m0.z + m1.z * m1.z + m2.z * m2.z;
    return cov;
  }

  // 3B uzayda verilen bir x noktasindaki Gauss yogunlugunu hesaplar:
  // G(x) = exp(-0.5 * (x - mu)^T * Sigma^-1 * (x - mu))
  float evaluate_density(Vec3 x) const {
    const Vec3 d = x - pos;
    Mat3Sym cov = compute_covariance();
    Mat3Sym inv_cov;
    if (!cov.invert(inv_cov)) return 0.0f;

    // Mahalanobis mesafesi karesi: d^T * Sigma^-1 * d
    const float d_cov_x = inv_cov.c00 * d.x + inv_cov.c01 * d.y + inv_cov.c02 * d.z;
    const float d_cov_y = inv_cov.c01 * d.x + inv_cov.c11 * d.y + inv_cov.c12 * d.z;
    const float d_cov_z = inv_cov.c02 * d.x + inv_cov.c12 * d.y + inv_cov.c22 * d.z;

    const float mahalanobis_sq = d.x * d_cov_x + d.y * d_cov_y + d.z * d_cov_z;
    if (mahalanobis_sq > 16.0f) return 0.0f; // 4-sigma disinda pratik olarak sifir

    return std::exp(-0.5f * mahalanobis_sq);
  }

  // Hiza gore anizotropik esneme (PhysGaussian / damlacik & alev kuyruklanmasi)
  void stretch_along_velocity(float stretch_factor = 0.2f) {
    const float speed = length(velocity);
    if (speed < 1e-4f) return;

    const Vec3 dir = velocity * (1.0f / speed);
    // Z eksenini hiz yonune donduren kuaterniyon
    const Vec3 z_axis{0.0f, 0.0f, 1.0f};
    const Vec3 rot_axis = cross(z_axis, dir);
    const float d = dot(z_axis, dir);

    if (d < -0.9999f) {
      rot = Quat::axis_angle({1.0f, 0.0f, 0.0f}, kPi);
    } else if (d < 0.9999f) {
      const float angle = std::acos(d < -1.0f ? -1.0f : (d > 1.0f ? 1.0f : d));
      rot = Quat::axis_angle(normalize(rot_axis), angle);
    } else {
      rot = Quat::identity();
    }

    // Z yaricapini hiza bagli olarak uzat
    scale.z = scale.x * (1.0f + stretch_factor * speed);
  }
};

} // namespace tulpar::engine::content
