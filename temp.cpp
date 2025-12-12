#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

//-------------------------------------------
// Fully compile-time circle mask
//-------------------------------------------
template<size_t Width,
         size_t Height,
         size_t CenterX,
         size_t CenterY,
         size_t Radius>
class ConstevalCircleMask{
    public:
        using value_type   = uint8_t;
        using storage_type = std::array<value_type, Width * Height>;

        

        // Precomputed compile-time data
        static constexpr storage_type mask = generate();

        static constexpr value_type at(size_t y, size_t x) noexcept {
            return mask[y * Width + x];
        }

    protected:
        // Generate at compile time only
        static consteval storage_type generate(void) {
            storage_type data; // zero-initialized

            constexpr size_t r2 = Radius * Radius;

            for (size_t y = 0; y < Height; ++y) {
                for (size_t x = 0; x < Width; ++x) {
                    const size_t dx = (x > CenterX) ? (x - CenterX) : (CenterX - x);
                    const size_t dy = (y > CenterY) ? (y - CenterY) : (CenterY - y);
                    const size_t d2 = dx * dx + dy * dy;

                    data[y * Width + x] = (d2 <= r2) ? 1u : 0u;
                }
            }

            return data;
        }

    ConstevalCircleMask(void) = delete;
};


#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include <cmath>

// x86 AVX2
#if defined(__AVX2__)
  #include <immintrin.h>
#endif

// ARM NEON
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  #include <arm_neon.h>
#endif

//-------------------------------------------
// Runtime SIMD circle matrix
//-------------------------------------------
class SimdCircleMatrix
{
public:
    using value_type = float; // 1.0f inside circle, 0.0f outside

    SimdCircleMatrix(size_t width,
                     size_t height,
                     float center_x,
                     float center_y,
                     float radius)
        : m_width{width}
        , m_height{height}
        , m_center_x{center_x}
        , m_center_y{center_y}
        , m_radius2{radius * radius}
        , m_data(width * height, 0.0f)
    {
        generate();
    }

    size_t width(void)  const noexcept { return m_width; }
    size_t height(void) const noexcept { return m_height; }

    value_type operator()(size_t y, size_t x) const noexcept {
        return m_data[y * m_width + x];
    }

    const std::vector<value_type>& raw(void) const noexcept {
        return m_data;
    }

private:
    void generate(void) noexcept {
        m_data.reserve(m_width * m_height);
#if defined(__AVX2__)
        generate_avx2();
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
        generate_neon();
#else
        generate_scalar();
#endif
    }

    //---------------------------------------
    // Scalar fallback (portable, simple)
    //---------------------------------------
    void generate_scalar() noexcept {
        for (size_t y = 0; y < m_height; ++y) {
            const float dy  = static_cast<float>(y) - m_center_y;
            const float dy2 = dy * dy;

            for (size_t x = 0; x < m_width; ++x) {
                const float dx  = static_cast<float>(x) - m_center_x;
                const float d2  = dx * dx + dy2;
                m_data[y * m_width + x] = (d2 <= m_radius2) ? 1.0f : 0.0f;
            }
        }
    }

    //---------------------------------------
    // x86 AVX2 version (process 8 pixels at a time)
    //---------------------------------------
#if defined(__AVX2__)
    void generate_avx2() noexcept {
        const __m256 cxv = _mm256_set1_ps(m_center_x);
        const __m256 r2v = _mm256_set1_ps(m_radius2);
        const __m256 ones  = _mm256_set1_ps(1.0f);
        const __m256 zeros = _mm256_set1_ps(0.0f);

        for (size_t y = 0; y < m_height; ++y) {
            const float dy  = static_cast<float>(y) - m_center_y;
            const float dy2 = dy * dy;
            const __m256 dy2v = _mm256_set1_ps(dy2);

            size_t x = 0;
            for (; x + 7 < m_width; x += 8) {
                // xv = [x, x+1, ..., x+7]
                const __m256 xv = _mm256_set_ps(
                    static_cast<float>(x + 7),
                    static_cast<float>(x + 6),
                    static_cast<float>(x + 5),
                    static_cast<float>(x + 4),
                    static_cast<float>(x + 3),
                    static_cast<float>(x + 2),
                    static_cast<float>(x + 1),
                    static_cast<float>(x + 0)
                );

                const __m256 dx   = _mm256_sub_ps(xv, cxv);
                const __m256 dx2  = _mm256_mul_ps(dx, dx);
                const __m256 d2   = _mm256_add_ps(dx2, dy2v);
                const __m256 cmp  = _mm256_cmp_ps(d2, r2v, _CMP_LE_OQ);
                // blend 0.0f / 1.0f based on mask
                const __m256 res  = _mm256_blendv_ps(zeros, ones, cmp);

                _mm256_storeu_ps(&m_data[y * m_width + x], res);
            }

            // Tail
            for (; x < m_width; ++x) {
                const float dx  = static_cast<float>(x) - m_center_x;
                const float d2  = dx * dx + dy2;
                m_data[y * m_width + x] = (d2 <= m_radius2) ? 1.0f : 0.0f;
            }
        }
    }
#endif

    //---------------------------------------
    // ARM NEON version (process 4 pixels at a time)
    //---------------------------------------
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    void generate_neon() noexcept {
        const float32x4_t cxv = vdupq_n_f32(m_center_x);
        const float32x4_t r2v = vdupq_n_f32(m_radius2);
        const float32x4_t ones  = vdupq_n_f32(1.0f);
        const float32x4_t zeros = vdupq_n_f32(0.0f);

        for (size_t y = 0; y < m_height; ++y) {
            const float dy  = static_cast<float>(y) - m_center_y;
            const float dy2 = dy * dy;
            const float32x4_t dy2v = vdupq_n_f32(dy2);

            size_t x = 0;
            for (; x + 3 < m_width; x += 4) {
                // xv = [x, x+1, x+2, x+3]
                const float xs[4] = {
                    static_cast<float>(x + 0),
                    static_cast<float>(x + 1),
                    static_cast<float>(x + 2),
                    static_cast<float>(x + 3)
                };
                const float32x4_t xv  = vld1q_f32(xs);
                const float32x4_t dx  = vsubq_f32(xv, cxv);
                const float32x4_t dx2 = vmulq_f32(dx, dx);
                const float32x4_t d2  = vaddq_f32(dx2, dy2v);

                const uint32x4_t mask = vcleq_f32(d2, r2v);
                const float32x4_t res = vbslq_f32(mask, ones, zeros);

                vst1q_f32(&m_data[y * m_width + x], res);
            }

            // Tail
            for (; x < m_width; ++x) {
                const float dx  = static_cast<float>(x) - m_center_x;
                const float d2  = dx * dx + dy2;
                m_data[y * m_width + x] = (d2 <= m_radius2) ? 1.0f : 0.0f;
            }
        }
    }
#endif

    size_t m_width;
    size_t m_height;
    float m_center_x;
    float m_center_y;
    float m_radius2;
    std::vector<value_type> m_data;
};

