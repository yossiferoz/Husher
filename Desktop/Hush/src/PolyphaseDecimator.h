#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <cstring>

// SIMD includes
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
    #include <emmintrin.h>
    #define KHDETECTOR_USE_SSE2 1
#endif

#if defined(__SSE4_1__) || (defined(_MSC_VER) && _MSC_VER >= 1600)
    #include <smmintrin.h>
    #define KHDETECTOR_USE_SSE41 1
#endif

#if defined(__AVX__) || (defined(_MSC_VER) && _MSC_VER >= 1700)
    #include <immintrin.h>
    #define KHDETECTOR_USE_AVX 1
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #include <arm_neon.h>
    #define KHDETECTOR_USE_NEON 1
#endif

#ifndef DECIM_FACTOR
    #define DECIM_FACTOR 3  // Default: 48kHz -> 16kHz
#endif

namespace KhDetector {

/**
 * @brief Poly-phase FIR decimator for efficient downsampling
 * 
 * This class implements a polyphase FIR filter for decimation, which is more
 * efficient than filtering followed by downsampling. It uses SIMD instructions
 * where available for optimal performance.
 * 
 * @tparam DecimationFactor The integer decimation factor (e.g., 3 for 48kHz->16kHz)
 * @tparam FilterLength The total FIR filter length
 */
template<int DecimationFactor = DECIM_FACTOR, int FilterLength = DecimationFactor * 16>
class PolyphaseDecimator
{
public:
    static constexpr int kDecimationFactor = DecimationFactor;
    static constexpr int kFilterLength = FilterLength;
    static constexpr int kPhaseLength = FilterLength / DecimationFactor;
    
    static_assert(DecimationFactor > 0, "Decimation factor must be positive");
    static_assert(FilterLength % DecimationFactor == 0, "Filter length must be divisible by decimation factor");

    /**
     * @brief Construct a new Polyphase Decimator
     * 
     * @param cutoffFreq Normalized cutoff frequency (0.0 to 1.0, where 1.0 = Nyquist)
     * @param transitionWidth Normalized transition width for the filter
     */
    PolyphaseDecimator(float cutoffFreq = 0.45f, float transitionWidth = 0.1f)
        : inputBuffer_(FilterLength, 0.0f)
        , bufferIndex_(0)
    {
        designLowpassFilter(cutoffFreq, transitionWidth);
        createPolyphaseFilters();
    }

    /**
     * @brief Process stereo input and produce mono decimated output
     * 
     * @param leftInput Left channel input samples
     * @param rightInput Right channel input samples
     * @param output Output buffer for decimated mono samples
     * @param numInputSamples Number of input samples per channel
     * @return Number of output samples produced
     */
    int processStereoToMono(const float* leftInput, const float* rightInput, 
                           float* output, int numInputSamples)
    {
        int outputCount = 0;
        
        for (int i = 0; i < numInputSamples; ++i) {
            // Convert stereo to mono (simple average)
            float monoSample = (leftInput[i] + rightInput[i]) * 0.5f;
            
            // Add to circular buffer
            inputBuffer_[bufferIndex_] = monoSample;
            bufferIndex_ = (bufferIndex_ + 1) % FilterLength;
            
            // Check if we should produce an output sample
            if ((i % DecimationFactor) == (DecimationFactor - 1)) {
                output[outputCount++] = computeFilteredSample();
            }
        }
        
        return outputCount;
    }

    /**
     * @brief Process mono input and produce decimated output
     * 
     * @param input Input samples
     * @param output Output buffer for decimated samples
     * @param numInputSamples Number of input samples
     * @return Number of output samples produced
     */
    int processMono(const float* input, float* output, int numInputSamples)
    {
        int outputCount = 0;
        
        for (int i = 0; i < numInputSamples; ++i) {
            // Add to circular buffer
            inputBuffer_[bufferIndex_] = input[i];
            bufferIndex_ = (bufferIndex_ + 1) % FilterLength;
            
            // Check if we should produce an output sample
            if ((i % DecimationFactor) == (DecimationFactor - 1)) {
                output[outputCount++] = computeFilteredSample();
            }
        }
        
        return outputCount;
    }

    /**
     * @brief Reset the decimator state
     */
    void reset()
    {
        std::fill(inputBuffer_.begin(), inputBuffer_.end(), 0.0f);
        bufferIndex_ = 0;
    }

    /**
     * @brief Get the group delay of the filter in samples
     */
    constexpr int getGroupDelay() const
    {
        return kPhaseLength / 2;
    }

private:
    std::vector<float> inputBuffer_;
    int bufferIndex_;
    
    // Polyphase filter coefficients
    std::array<std::vector<float>, DecimationFactor> polyphaseFilters_;
    
    /**
     * @brief Design a lowpass FIR filter using windowed sinc method
     */
    void designLowpassFilter(float cutoffFreq, float transitionWidth)
    {
        std::vector<float> h(FilterLength);
        
        // Design lowpass filter with cutoff at 1/DecimationFactor to prevent aliasing
        float fc = std::min(cutoffFreq, 1.0f / DecimationFactor);
        
        // Generate windowed sinc filter
        for (int n = 0; n < FilterLength; ++n) {
            int m = n - FilterLength / 2;
            
            if (m == 0) {
                h[n] = 2.0f * fc;
            } else {
                h[n] = std::sin(2.0f * M_PI * fc * m) / (M_PI * m);
            }
            
            // Apply Hamming window
            h[n] *= 0.54f - 0.46f * std::cos(2.0f * M_PI * n / (FilterLength - 1));
        }
        
        // Normalize
        float sum = 0.0f;
        for (float coeff : h) {
            sum += coeff;
        }
        
        for (int n = 0; n < FilterLength; ++n) {
            h[n] /= sum;
        }
        
        // Store coefficients for polyphase decomposition
        filterCoeffs_ = std::move(h);
    }
    
    /**
     * @brief Create polyphase filter banks from the prototype filter
     */
    void createPolyphaseFilters()
    {
        for (int phase = 0; phase < DecimationFactor; ++phase) {
            polyphaseFilters_[phase].resize(kPhaseLength);
            
            for (int i = 0; i < kPhaseLength; ++i) {
                int coeffIndex = i * DecimationFactor + phase;
                polyphaseFilters_[phase][i] = filterCoeffs_[coeffIndex];
            }
        }
    }
    
    /**
     * @brief Compute filtered sample using current polyphase
     */
    float computeFilteredSample()
    {
        // Determine which polyphase filter to use
        int phase = (DecimationFactor - 1) - (bufferIndex_ % DecimationFactor);
        
        return computeConvolution(polyphaseFilters_[phase]);
    }
    
    /**
     * @brief Compute convolution with SIMD optimization where possible
     */
    float computeConvolution(const std::vector<float>& coeffs)
    {
#if defined(KHDETECTOR_USE_AVX)
        return computeConvolutionAVX(coeffs);
#elif defined(KHDETECTOR_USE_SSE2)
        return computeConvolutionSSE(coeffs);
#elif defined(KHDETECTOR_USE_NEON)
        return computeConvolutionNEON(coeffs);
#else
        return computeConvolutionScalar(coeffs);
#endif
    }
    
#if defined(KHDETECTOR_USE_AVX)
    /**
     * @brief AVX-optimized convolution
     */
    float computeConvolutionAVX(const std::vector<float>& coeffs)
    {
        __m256 sum = _mm256_setzero_ps();
        const int simdLength = (kPhaseLength / 8) * 8;
        
        for (int i = 0; i < simdLength; i += 8) {
            // Load 8 coefficients
            __m256 c = _mm256_loadu_ps(&coeffs[i]);
            
            // Load 8 input samples (with circular buffer handling)
            __m256 x = loadInputSamplesAVX(i);
            
            // Multiply and accumulate
            sum = _mm256_fmadd_ps(x, c, sum);
        }
        
        // Horizontal sum
        float result[8];
        _mm256_storeu_ps(result, sum);
        float total = result[0] + result[1] + result[2] + result[3] + 
                     result[4] + result[5] + result[6] + result[7];
        
        // Handle remaining samples
        for (int i = simdLength; i < kPhaseLength; ++i) {
            int bufferIdx = (bufferIndex_ - 1 - i + FilterLength) % FilterLength;
            total += inputBuffer_[bufferIdx] * coeffs[i];
        }
        
        return total;
    }
    
    __m256 loadInputSamplesAVX(int offset)
    {
        float samples[8];
        for (int i = 0; i < 8; ++i) {
            int bufferIdx = (bufferIndex_ - 1 - offset - i + FilterLength) % FilterLength;
            samples[i] = inputBuffer_[bufferIdx];
        }
        return _mm256_loadu_ps(samples);
    }
#endif

#if defined(KHDETECTOR_USE_SSE2)
    /**
     * @brief SSE2-optimized convolution
     */
    float computeConvolutionSSE(const std::vector<float>& coeffs)
    {
        __m128 sum = _mm_setzero_ps();
        const int simdLength = (kPhaseLength / 4) * 4;
        
        for (int i = 0; i < simdLength; i += 4) {
            // Load 4 coefficients
            __m128 c = _mm_loadu_ps(&coeffs[i]);
            
            // Load 4 input samples
            __m128 x = loadInputSamplesSSE(i);
            
            // Multiply and accumulate
            sum = _mm_add_ps(sum, _mm_mul_ps(x, c));
        }
        
        // Horizontal sum
        float result[4];
        _mm_storeu_ps(result, sum);
        float total = result[0] + result[1] + result[2] + result[3];
        
        // Handle remaining samples
        for (int i = simdLength; i < kPhaseLength; ++i) {
            int bufferIdx = (bufferIndex_ - 1 - i + FilterLength) % FilterLength;
            total += inputBuffer_[bufferIdx] * coeffs[i];
        }
        
        return total;
    }
    
    __m128 loadInputSamplesSSE(int offset)
    {
        float samples[4];
        for (int i = 0; i < 4; ++i) {
            int bufferIdx = (bufferIndex_ - 1 - offset - i + FilterLength) % FilterLength;
            samples[i] = inputBuffer_[bufferIdx];
        }
        return _mm_loadu_ps(samples);
    }
#endif

#if defined(KHDETECTOR_USE_NEON)
    /**
     * @brief NEON-optimized convolution
     */
    float computeConvolutionNEON(const std::vector<float>& coeffs)
    {
        float32x4_t sum = vdupq_n_f32(0.0f);
        const int simdLength = (kPhaseLength / 4) * 4;
        
        for (int i = 0; i < simdLength; i += 4) {
            // Load 4 coefficients
            float32x4_t c = vld1q_f32(&coeffs[i]);
            
            // Load 4 input samples
            float32x4_t x = loadInputSamplesNEON(i);
            
            // Multiply and accumulate
            sum = vmlaq_f32(sum, x, c);
        }
        
        // Horizontal sum
        float32x2_t sum_pair = vadd_f32(vget_low_f32(sum), vget_high_f32(sum));
        float total = vget_lane_f32(vpadd_f32(sum_pair, sum_pair), 0);
        
        // Handle remaining samples
        for (int i = simdLength; i < kPhaseLength; ++i) {
            int bufferIdx = (bufferIndex_ - 1 - i + FilterLength) % FilterLength;
            total += inputBuffer_[bufferIdx] * coeffs[i];
        }
        
        return total;
    }
    
    float32x4_t loadInputSamplesNEON(int offset)
    {
        float samples[4];
        for (int i = 0; i < 4; ++i) {
            int bufferIdx = (bufferIndex_ - 1 - offset - i + FilterLength) % FilterLength;
            samples[i] = inputBuffer_[bufferIdx];
        }
        return vld1q_f32(samples);
    }
#endif

    /**
     * @brief Scalar (non-SIMD) convolution fallback
     */
    float computeConvolutionScalar(const std::vector<float>& coeffs)
    {
        float sum = 0.0f;
        
        for (int i = 0; i < kPhaseLength; ++i) {
            int bufferIdx = (bufferIndex_ - 1 - i + FilterLength) % FilterLength;
            sum += inputBuffer_[bufferIdx] * coeffs[i];
        }
        
        return sum;
    }
    
    std::vector<float> filterCoeffs_;
};

/**
 * @brief Specialized decimator for common audio sample rates
 */
using Decimator48to16 = PolyphaseDecimator<3, 48>;   // 48kHz -> 16kHz
using Decimator44to16 = PolyphaseDecimator<3, 48>;   // 44.1kHz -> ~14.7kHz (approximate)

} // namespace KhDetector 