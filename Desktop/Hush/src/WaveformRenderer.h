#pragma once

#include "WaveformData.h"
#include <memory>
#include <chrono>
#include <atomic>

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

namespace KhDetector {

/**
 * @brief High-performance OpenGL waveform renderer with VBO double-buffering
 * 
 * Features:
 * - Scrolling waveform display with 50ms SMAA line recycling
 * - Spectral overlay visualization
 * - Colored hit flags
 * - VBO double-buffering for >120 FPS
 * - Real-time performance monitoring
 */
class WaveformRenderer {
public:
    /**
     * @brief Constructor
     * 
     * @param config Waveform display configuration
     */
    explicit WaveformRenderer(const WaveformConfig& config = WaveformConfig{});
    
    /**
     * @brief Destructor
     */
    ~WaveformRenderer();
    
    // Non-copyable, movable
    WaveformRenderer(const WaveformRenderer&) = delete;
    WaveformRenderer& operator=(const WaveformRenderer&) = delete;
    WaveformRenderer(WaveformRenderer&&) = default;
    WaveformRenderer& operator=(WaveformRenderer&&) = default;
    
    /**
     * @brief Initialize OpenGL resources
     * 
     * @param viewportWidth Viewport width in pixels
     * @param viewportHeight Viewport height in pixels
     * @return true if initialization succeeded
     */
    bool initialize(int viewportWidth, int viewportHeight);
    
    /**
     * @brief Cleanup OpenGL resources
     */
    void cleanup();
    
    /**
     * @brief Update waveform data and render frame
     * 
     * @param samples New waveform samples to add
     * @param spectralFrames New spectral data to add
     */
    void render(const std::vector<WaveformSample>& samples,
                const std::vector<SpectralFrame>& spectralFrames = {});
    
    /**
     * @brief Set viewport size
     */
    void setViewport(int width, int height);
    
    /**
     * @brief Update configuration
     */
    void setConfig(const WaveformConfig& config);
    const WaveformConfig& getConfig() const;
    
    /**
     * @brief Performance statistics
     */
    struct PerformanceStats {
        float currentFPS = 0.0f;
        float averageFPS = 0.0f;
        float renderTimeMs = 0.0f;
        int vertexCount = 0;
        int drawCalls = 0;
        bool vboSwapped = false;
        
        // Frame timing
        std::chrono::high_resolution_clock::time_point lastFrameTime;
        std::chrono::duration<float> frameDuration{0.0f};
    };
    
    const PerformanceStats& getPerformanceStats() const;
    
    /**
     * @brief Check if renderer is initialized
     */
    bool isInitialized() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/**
 * @brief SMAA (Subpixel Morphological Anti-Aliasing) helper
 * 
 * Provides anti-aliasing for smooth waveform lines at high frame rates
 */
class SMAAHelper {
public:
    SMAAHelper();
    ~SMAAHelper();
    
    bool initialize(int width, int height);
    void cleanup();
    void resize(int width, int height);
    
    /**
     * @brief Begin SMAA pass
     * 
     * @return Framebuffer ID to render to
     */
    GLuint beginSMAAPass();
    
    /**
     * @brief End SMAA pass and apply anti-aliasing
     */
    void endSMAAPass();
    
    bool isInitialized() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/**
 * @brief Vertex structure for waveform rendering
 */
struct WaveformVertex {
    float position[2];  // x, y
    float color[4];     // r, g, b, a
    float texCoord[2];  // u, v (for spectral overlay)
    float flags;        // Packed flags (hit, spectral, etc.)
    
    WaveformVertex() : flags(0.0f) {
        position[0] = position[1] = 0.0f;
        color[0] = color[1] = color[2] = color[3] = 1.0f;
        texCoord[0] = texCoord[1] = 0.0f;
    }
    
    WaveformVertex(float x, float y, float r, float g, float b, float a = 1.0f) : flags(0.0f) {
        position[0] = x; position[1] = y;
        color[0] = r; color[1] = g; color[2] = b; color[3] = a;
        texCoord[0] = texCoord[1] = 0.0f;
    }
    
    // Flag bits
    static constexpr float HIT_FLAG = 1.0f;
    static constexpr float SPECTRAL_FLAG = 2.0f;
    static constexpr float GRID_FLAG = 4.0f;
};

/**
 * @brief VBO manager for double-buffering
 */
class VBOManager {
public:
    VBOManager();
    ~VBOManager();
    
    bool initialize(size_t maxVertices);
    void cleanup();
    
    /**
     * @brief Get current VBO for writing
     */
    GLuint getCurrentVBO();
    
    /**
     * @brief Get previous VBO for reading
     */
    GLuint getPreviousVBO();
    
    /**
     * @brief Swap buffers
     */
    void swapBuffers();
    
    /**
     * @brief Update vertex data
     */
    bool updateVertices(const std::vector<WaveformVertex>& vertices);
    
    size_t getMaxVertices() const;
    size_t getCurrentVertexCount() const;

private:
    static constexpr int kNumBuffers = 2;
    GLuint vbos_[kNumBuffers];
    GLuint vaos_[kNumBuffers];
    int currentBuffer_;
    size_t maxVertices_;
    size_t currentVertexCount_;
    bool initialized_;
};

/**
 * @brief Shader program for waveform rendering
 */
class WaveformShader {
public:
    WaveformShader();
    ~WaveformShader();
    
    bool initialize();
    void cleanup();
    void use();
    
    // Uniform setters
    void setProjectionMatrix(const float* matrix);
    void setTimeOffset(float offset);
    void setAmplitudeScale(float scale);
    void setColors(const WaveformConfig::Colors& colors);
    void setFlags(int flags);
    
    GLuint getProgram() const;
    bool isInitialized() const;

private:
    GLuint program_;
    GLuint vertexShader_;
    GLuint fragmentShader_;
    
    // Uniform locations
    GLint projectionMatrixLoc_;
    GLint timeOffsetLoc_;
    GLint amplitudeScaleLoc_;
    GLint waveformColorLoc_;
    GLint spectralColorLoc_;
    GLint hitFlagColorLoc_;
    GLint backgroundColorLoc_;
    GLint gridColorLoc_;
    GLint flagsLoc_;
    
    bool initialized_;
    
    bool compileShader(GLuint shader, const char* source);
    bool linkProgram();
    void findUniformLocations();
};

} // namespace KhDetector 