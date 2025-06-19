#pragma once

#include "vstgui/lib/copenglview.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/lib/cstring.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "WaveformRenderer.h"
#include "WaveformData.h"
#include <atomic>
#include <chrono>
#include <memory>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

namespace KhDetector {

/**
 * @brief OpenGL-based view for KhDetector plugin GUI
 * 
 * Features:
 * - Scrolling waveform display with spectral overlay
 * - Colored hit flags at >120 FPS
 * - VBO double-buffering for smooth performance
 * - SMAA anti-aliasing for smooth lines
 * - Real-time FPS counter display
 * - Hit detection flash indicator
 */
class KhDetectorOpenGLView : public VSTGUI::COpenGLView
{
public:
    /**
     * @brief Constructor
     * 
     * @param size Initial view size
     * @param hitState Atomic reference to hit state from processor
     * @param waveformBuffer Shared waveform data buffer
     */
    KhDetectorOpenGLView(const VSTGUI::CRect& size, 
                        std::atomic<bool>& hitState,
                        std::shared_ptr<KhDetector::WaveformBuffer4K> waveformBuffer = nullptr);
    
    /**
     * @brief Destructor
     */
    ~KhDetectorOpenGLView() override;

    // CView overrides
    void draw(VSTGUI::CDrawContext* pContext) override;
    void setViewSize(const VSTGUI::CRect& rect, bool invalid = true) override;
    
    // COpenGLView overrides
    void drawOpenGL(const VSTGUI::CRect& updateRect) override;
    void platformOpenGLViewCreated() override;
    void platformOpenGLViewSizeChanged() override;
    void platformOpenGLViewWillDestroy() override;
    
    // Timer callback
    void onTimer();
    
    /**
     * @brief Start/stop the animation timer
     */
    void startAnimation();
    void stopAnimation();
    
    /**
     * @brief Set waveform buffer for data visualization
     */
    void setWaveformBuffer(std::shared_ptr<KhDetector::WaveformBuffer4K> buffer);
    
    /**
     * @brief Update waveform configuration
     */
    void setWaveformConfig(const KhDetector::WaveformConfig& config);
    const KhDetector::WaveformConfig& getWaveformConfig() const;
    
    /**
     * @brief Configuration options
     */
    struct Config
    {
        float refreshRate = 60.0f;          // Target refresh rate (Hz)
        float hitFlashDuration = 0.5f;      // Flash duration in seconds
        float hitFlashIntensity = 1.0f;     // Flash intensity (0-1)
        bool showFPS = true;                // Show FPS counter
        bool showHitIndicator = true;       // Show hit flash indicator
        VSTGUI::CColor backgroundColor = VSTGUI::CColor(20, 20, 30, 255);
        VSTGUI::CColor textColor = VSTGUI::CColor(255, 255, 255, 255);
        VSTGUI::CColor hitColor = VSTGUI::CColor(255, 100, 100, 255);
    };
    
    /**
     * @brief Update configuration
     */
    void setConfig(const Config& config);
    const Config& getConfig() const { return mConfig; }
    
    /**
     * @brief Get performance statistics
     */
    struct Statistics
    {
        float currentFPS = 0.0f;
        float averageFPS = 0.0f;
        uint64_t frameCount = 0;
        uint64_t hitCount = 0;
        float lastHitTime = 0.0f;
    };
    
    const Statistics& getStatistics() const { return mStats; }

private:
    // Configuration
    Config mConfig;
    
    // Hit state reference
    std::atomic<bool>& mHitState;
    
    // Waveform visualization
    std::shared_ptr<KhDetector::WaveformBuffer4K> mWaveformBuffer;
    std::unique_ptr<KhDetector::WaveformRenderer> mWaveformRenderer;
    std::unique_ptr<KhDetector::SpectralAnalyzer> mSpectralAnalyzer;
    KhDetector::WaveformConfig mWaveformConfig;
    
    // Animation state
    bool mAnimationActive = false;
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> mTimer;
    
    // Hit detection state
    bool mLastHitState = false;
    float mHitFlashTime = 0.0f;
    float mHitFlashAlpha = 0.0f;
    
    // FPS calculation
    std::chrono::high_resolution_clock::time_point mLastFrameTime;
    std::chrono::high_resolution_clock::time_point mStartTime;
    std::vector<float> mFrameTimeHistory;
    size_t mFrameTimeIndex = 0;
    static constexpr size_t kFrameTimeHistorySize = 60;
    
    // Statistics
    mutable Statistics mStats;
    
    // OpenGL state
    bool mOpenGLInitialized = false;
    GLuint mShaderProgram = 0;
    GLuint mVertexBuffer = 0;
    GLuint mVertexArray = 0;
    
    // Fonts and text rendering
    VSTGUI::SharedPointer<VSTGUI::CFontDesc> mFont;
    
    /**
     * @brief Initialize OpenGL resources
     */
    bool initializeOpenGL();
    
    /**
     * @brief Cleanup OpenGL resources
     */
    void cleanupOpenGL();
    
    /**
     * @brief Update animation state
     */
    void updateAnimation();
    
    /**
     * @brief Update FPS calculation
     */
    void updateFPS();
    
    /**
     * @brief Update hit detection state
     */
    void updateHitState();
    
    /**
     * @brief Render the scene
     */
    void renderScene();
    
    /**
     * @brief Render FPS counter
     */
    void renderFPS();
    
    /**
     * @brief Render hit flash indicator
     */
    void renderHitFlash();
    
    /**
     * @brief Render background
     */
    void renderBackground();
    
    /**
     * @brief Create and compile shader
     */
    GLuint createShader(GLenum type, const char* source);
    
    /**
     * @brief Create shader program
     */
    GLuint createShaderProgram();
    
    /**
     * @brief Setup vertex data
     */
    void setupVertexData();
    
    /**
     * @brief Convert time to seconds since start
     */
    float getTimeSeconds() const;
    
    /**
     * @brief Smooth step interpolation
     */
    static float smoothStep(float edge0, float edge1, float x);
    
    /**
     * @brief Ease out cubic interpolation
     */
    static float easeOutCubic(float t);
};

/**
 * @brief Factory function to create OpenGL view
 */
std::unique_ptr<KhDetectorOpenGLView> createOpenGLView(
    const VSTGUI::CRect& size, 
    std::atomic<bool>& hitState
);

} // namespace KhDetector 