#include "WaveformData.h"
#include <cmath>
#include <algorithm>
#include <complex>
#include <numeric>

namespace KhDetector {

/**
 * @brief Simple FFT implementation for spectral analysis
 */
class SimpleFFT {
public:
    static void fft(std::vector<std::complex<float>>& data) {
        const size_t N = data.size();
        if (N <= 1) return;
        
        // Bit-reversal permutation
        for (size_t i = 1, j = 0; i < N; ++i) {
            size_t bit = N >> 1;
            for (; j & bit; bit >>= 1) {
                j ^= bit;
            }
            j ^= bit;
            if (i < j) {
                std::swap(data[i], data[j]);
            }
        }
        
        // Cooley-Tukey FFT
        for (size_t len = 2; len <= N; len <<= 1) {
            float angle = -2.0f * M_PI / len;
            std::complex<float> wlen(std::cos(angle), std::sin(angle));
            
            for (size_t i = 0; i < N; i += len) {
                std::complex<float> w(1.0f, 0.0f);
                for (size_t j = 0; j < len / 2; ++j) {
                    std::complex<float> u = data[i + j];
                    std::complex<float> v = data[i + j + len / 2] * w;
                    data[i + j] = u + v;
                    data[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }
    }
    
    static size_t nextPowerOfTwo(size_t n) {
        size_t power = 1;
        while (power < n) {
            power <<= 1;
        }
        return power;
    }
};

/**
 * @brief Implementation class for SpectralAnalyzer
 */
class SpectralAnalyzer::Impl {
public:
    Impl(int frameSize) : frameSize_(frameSize), sampleRate_(16000.0f) {
        setFrameSize(frameSize);
    }
    
    void setFrameSize(int frameSize) {
        frameSize_ = frameSize;
        fftSize_ = SimpleFFT::nextPowerOfTwo(frameSize_);
        
        // Prepare windowing function (Hann window)
        window_.resize(frameSize_);
        for (int i = 0; i < frameSize_; ++i) {
            window_[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (frameSize_ - 1)));
        }
        
        // Prepare FFT buffer
        fftBuffer_.resize(fftSize_);
        magnitudes_.resize(fftSize_ / 2 + 1);
    }
    
    void setSampleRate(float sampleRate) {
        sampleRate_ = sampleRate;
    }
    
    SpectralFrame analyze(const float* audioData, int numSamples) {
        SpectralFrame frame;
        
        if (numSamples != frameSize_) {
            return frame; // Invalid frame size
        }
        
        // Apply windowing and copy to FFT buffer
        std::fill(fftBuffer_.begin(), fftBuffer_.end(), std::complex<float>(0.0f, 0.0f));
        for (int i = 0; i < frameSize_; ++i) {
            fftBuffer_[i] = std::complex<float>(audioData[i] * window_[i], 0.0f);
        }
        
        // Perform FFT
        SimpleFFT::fft(fftBuffer_);
        
        // Calculate magnitudes
        for (size_t i = 0; i < magnitudes_.size(); ++i) {
            magnitudes_[i] = std::abs(fftBuffer_[i]);
        }
        
        // Downsample to display bins
        const int numDisplayBins = SpectralFrame::kNumBins;
        const int binsPerDisplayBin = static_cast<int>(magnitudes_.size()) / numDisplayBins;
        
        for (int i = 0; i < numDisplayBins; ++i) {
            float sum = 0.0f;
            int startBin = i * binsPerDisplayBin;
            int endBin = std::min(startBin + binsPerDisplayBin, static_cast<int>(magnitudes_.size()));
            
            for (int j = startBin; j < endBin; ++j) {
                sum += magnitudes_[j];
            }
            
            frame.magnitudes[i] = sum / (endBin - startBin);
        }
        
        // Calculate spectral features
        frame.spectralCentroid = calculateSpectralCentroid();
        frame.spectralRolloff = calculateSpectralRolloff();
        frame.fundamentalFreq = findFundamentalFrequency();
        
        return frame;
    }
    
private:
    int frameSize_;
    float sampleRate_;
    size_t fftSize_;
    
    std::vector<float> window_;
    std::vector<std::complex<float>> fftBuffer_;
    std::vector<float> magnitudes_;
    
    float calculateSpectralCentroid() {
        float numerator = 0.0f;
        float denominator = 0.0f;
        
        for (size_t i = 1; i < magnitudes_.size(); ++i) {
            float freq = (i * sampleRate_) / (2.0f * fftSize_);
            float magnitude = magnitudes_[i];
            
            numerator += freq * magnitude;
            denominator += magnitude;
        }
        
        return (denominator > 0.0f) ? numerator / denominator : 0.0f;
    }
    
    float calculateSpectralRolloff() {
        float totalEnergy = 0.0f;
        for (size_t i = 1; i < magnitudes_.size(); ++i) {
            totalEnergy += magnitudes_[i] * magnitudes_[i];
        }
        
        float rolloffThreshold = 0.85f * totalEnergy;
        float cumulativeEnergy = 0.0f;
        
        for (size_t i = 1; i < magnitudes_.size(); ++i) {
            cumulativeEnergy += magnitudes_[i] * magnitudes_[i];
            if (cumulativeEnergy >= rolloffThreshold) {
                return (i * sampleRate_) / (2.0f * fftSize_);
            }
        }
        
        return sampleRate_ / 2.0f; // Nyquist frequency
    }
    
    float findFundamentalFrequency() {
        // Simple peak detection for fundamental frequency
        size_t maxBin = 1;
        float maxMagnitude = magnitudes_[1];
        
        // Look for peak in lower frequency range (up to 2kHz)
        size_t maxSearchBin = std::min(static_cast<size_t>((2000.0f * fftSize_) / sampleRate_), 
                                      magnitudes_.size());
        
        for (size_t i = 2; i < maxSearchBin; ++i) {
            if (magnitudes_[i] > maxMagnitude) {
                maxMagnitude = magnitudes_[i];
                maxBin = i;
            }
        }
        
        // Convert bin to frequency
        return (maxBin * sampleRate_) / fftSize_;
    }
};

// SpectralAnalyzer implementation
SpectralAnalyzer::SpectralAnalyzer(int frameSize) : pImpl_(std::make_unique<Impl>(frameSize)) {}

SpectralAnalyzer::~SpectralAnalyzer() = default;

SpectralFrame SpectralAnalyzer::analyze(const float* audioData, int numSamples) {
    return pImpl_->analyze(audioData, numSamples);
}

void SpectralAnalyzer::setFrameSize(int frameSize) {
    pImpl_->setFrameSize(frameSize);
}

void SpectralAnalyzer::setSampleRate(float sampleRate) {
    pImpl_->setSampleRate(sampleRate);
}

} // namespace KhDetector 