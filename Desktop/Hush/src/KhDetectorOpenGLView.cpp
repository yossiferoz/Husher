#include "KhDetectorOpenGLView.h"
#include "vstgui/lib/cstring.h"
#include "vstgui/lib/cgraphicspath.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <string>

namespace KhDetector {

// Vertex shader source
static const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

out vec3 vertexColor;
uniform mat4 projection;
uniform float time;

void main()
{
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    vertexColor = aColor;
}
)";

// Fragment shader source
static const char* fragmentShaderSource = R"(
#version 330 core
in vec3 vertexColor;
out vec4 FragColor;

uniform float alpha;
uniform float time;

void main()
{
    // Add some subtle animation to the color
    vec3 color = vertexColor;
    color += 0.1 * sin(time * 2.0);
    FragColor = vec4(color, alpha);
}
)";

KhDetectorOpenGLView::KhDetectorOpenGLView(const VSTGUI::CRect& size, 
                                         std::atomic<bool>& hitState,
                                         std::shared_ptr<KhDetector::WaveformBuffer4K> waveformBuffer)
    : COpenGLView(size)
    , mHitState(hitState)
    , mWaveformBuffer(waveformBuffer)
    , mFrameTimeHistory(kFrameTimeHistorySize, 1.0f / 60.0f)  // Initialize with 60 FPS
{
    // Initialize timing
    mStartTime = mLastFrameTime = std::chrono::high_resolution_clock::now();
    
    // Create font for text rendering
    mFont = VSTGUI::makeOwned<VSTGUI::CFontDesc>("Arial", 12);
    
    // Initialize waveform renderer
    mWaveformRenderer = std::make_unique<KhDetector::WaveformRenderer>(mWaveformConfig);
    
    // Initialize spectral analyzer for 20ms frames at 16kHz (320 samples)
    mSpectralAnalyzer = std::make_unique<KhDetector::SpectralAnalyzer>(320);
    mSpectralAnalyzer->setSampleRate(16000.0f);
    
    // Configure for high performance
    mWaveformConfig.targetFPS = 120;
    mWaveformConfig.enableVSync = false;
    mWaveformConfig.enableSMAA = true;
    mWaveformConfig.updateIntervalMs = 50.0f;  // 50ms SMAA line recycling
    
    std::cout << "KhDetectorOpenGLView: Created with waveform visualization, size " 
              << size.getWidth() << "x" << size.getHeight() << std::endl;
}

KhDetectorOpenGLView::~KhDetectorOpenGLView()
{
    stopAnimation();
    cleanupOpenGL();
    
    std::cout << "KhDetectorOpenGLView: Destroyed" << std::endl;
}

void KhDetectorOpenGLView::draw(VSTGUI::CDrawContext* pContext)
{
    // Update animation state
    updateAnimation();
    
    // Let the OpenGL view handle the actual drawing
    COpenGLView::draw(pContext);
}

void KhDetectorOpenGLView::setViewSize(const VSTGUI::CRect& rect, bool invalid)
{
    COpenGLView::setViewSize(rect, invalid);
    
    std::cout << "KhDetectorOpenGLView: Size changed to " 
              << rect.getWidth() << "x" << rect.getHeight() << std::endl;
}

void KhDetectorOpenGLView::drawOpenGL(const VSTGUI::CRect& updateRect)
{
    if (!mOpenGLInitialized) {
        if (!initializeOpenGL()) {
            std::cerr << "KhDetectorOpenGLView: Failed to initialize OpenGL" << std::endl;
            return;
        }
    }
    
    // Update FPS and hit state
    updateFPS();
    updateHitState();
    
    // Render the scene
    renderScene();
    
    // Update statistics
    mStats.frameCount++;
}

void KhDetectorOpenGLView::platformOpenGLViewCreated()
{
    std::cout << "KhDetectorOpenGLView: OpenGL context created" << std::endl;
    
    // Print OpenGL version info
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    
    std::cout << "OpenGL Version: " << (version ? version : "Unknown") << std::endl;
    std::cout << "OpenGL Vendor: " << (vendor ? vendor : "Unknown") << std::endl;
    std::cout << "OpenGL Renderer: " << (renderer ? renderer : "Unknown") << std::endl;
}

void KhDetectorOpenGLView::platformOpenGLViewSizeChanged()
{
    if (mOpenGLInitialized) {
        auto rect = getViewSize();
        glViewport(0, 0, static_cast<GLsizei>(rect.getWidth()), static_cast<GLsizei>(rect.getHeight()));
        
        std::cout << "KhDetectorOpenGLView: OpenGL viewport changed to " 
                  << rect.getWidth() << "x" << rect.getHeight() << std::endl;
    }
}

void KhDetectorOpenGLView::platformOpenGLViewWillDestroy()
{
    std::cout << "KhDetectorOpenGLView: OpenGL context will be destroyed" << std::endl;
    cleanupOpenGL();
}

void KhDetectorOpenGLView::onTimer()
{
    // Trigger a redraw
    invalid();
}

void KhDetectorOpenGLView::startAnimation()
{
    if (!mAnimationActive) {
        mAnimationActive = true;
        
        // Create timer for animation updates
        uint32_t intervalMs = static_cast<uint32_t>(1000.0f / mConfig.refreshRate);
        mTimer = VSTGUI::makeOwned<VSTGUI::CVSTGUITimer>([this](VSTGUI::CVSTGUITimer*) {
            onTimer();
        }, intervalMs, true);
        
        std::cout << "KhDetectorOpenGLView: Animation started at " 
                  << mConfig.refreshRate << " FPS" << std::endl;
    }
}

void KhDetectorOpenGLView::stopAnimation()
{
    if (mAnimationActive) {
        mAnimationActive = false;
        
        if (mTimer) {
            mTimer->stop();
            mTimer = nullptr;
        }
        
        std::cout << "KhDetectorOpenGLView: Animation stopped" << std::endl;
    }
}

void KhDetectorOpenGLView::setConfig(const Config& config)
{
    mConfig = config;
    
    // Restart animation with new refresh rate if active
    if (mAnimationActive) {
        stopAnimation();
        startAnimation();
    }
    
    std::cout << "KhDetectorOpenGLView: Configuration updated" << std::endl;
}

bool KhDetectorOpenGLView::initializeOpenGL()
{
    if (mOpenGLInitialized) {
        return true;
    }
    
    std::cout << "KhDetectorOpenGLView: Initializing OpenGL resources..." << std::endl;
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Create shader program
    mShaderProgram = createShaderProgram();
    if (mShaderProgram == 0) {
        std::cerr << "Failed to create shader program" << std::endl;
        return false;
    }
    
    // Setup vertex data
    setupVertexData();
    
    // Set initial viewport
    auto rect = getViewSize();
    int width = static_cast<int>(rect.getWidth());
    int height = static_cast<int>(rect.getHeight());
    glViewport(0, 0, width, height);
    
    // Initialize waveform renderer
    if (mWaveformRenderer && !mWaveformRenderer->initialize(width, height)) {
        std::cerr << "Failed to initialize waveform renderer" << std::endl;
        // Continue without waveform rendering
    }
    
    mOpenGLInitialized = true;
    std::cout << "KhDetectorOpenGLView: OpenGL initialization complete" << std::endl;
    
    return true;
}

void KhDetectorOpenGLView::cleanupOpenGL()
{
    if (!mOpenGLInitialized) {
        return;
    }
    
    std::cout << "KhDetectorOpenGLView: Cleaning up OpenGL resources..." << std::endl;
    
    if (mVertexArray) {
#ifdef __APPLE__
        glDeleteVertexArraysAPPLE(1, &mVertexArray);
#else
        glDeleteVertexArrays(1, &mVertexArray);
#endif
        mVertexArray = 0;
    }
    
    if (mVertexBuffer) {
        glDeleteBuffers(1, &mVertexBuffer);
        mVertexBuffer = 0;
    }
    
    if (mShaderProgram) {
        glDeleteProgram(mShaderProgram);
        mShaderProgram = 0;
    }
    
    mOpenGLInitialized = false;
}

void KhDetectorOpenGLView::updateAnimation()
{
    // Update hit flash animation
    if (mHitFlashTime > 0.0f) {
        float deltaTime = 1.0f / mConfig.refreshRate;
        mHitFlashTime -= deltaTime;
        
        if (mHitFlashTime <= 0.0f) {
            mHitFlashTime = 0.0f;
            mHitFlashAlpha = 0.0f;
        } else {
            // Smooth fade out
            float normalizedTime = 1.0f - (mHitFlashTime / mConfig.hitFlashDuration);
            mHitFlashAlpha = mConfig.hitFlashIntensity * easeOutCubic(1.0f - normalizedTime);
        }
    }
}

void KhDetectorOpenGLView::updateFPS()
{
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto deltaTime = std::chrono::duration<float>(currentTime - mLastFrameTime).count();
    mLastFrameTime = currentTime;
    
    // Update frame time history
    mFrameTimeHistory[mFrameTimeIndex] = deltaTime;
    mFrameTimeIndex = (mFrameTimeIndex + 1) % kFrameTimeHistorySize;
    
    // Calculate current and average FPS
    if (deltaTime > 0.0f) {
        mStats.currentFPS = 1.0f / deltaTime;
    }
    
    // Calculate average FPS from history
    float avgFrameTime = 0.0f;
    for (float frameTime : mFrameTimeHistory) {
        avgFrameTime += frameTime;
    }
    avgFrameTime /= static_cast<float>(kFrameTimeHistorySize);
    
    if (avgFrameTime > 0.0f) {
        mStats.averageFPS = 1.0f / avgFrameTime;
    }
}

void KhDetectorOpenGLView::updateHitState()
{
    bool currentHit = mHitState.load();
    
    // Detect hit state change
    if (currentHit && !mLastHitState) {
        // Hit started - trigger flash
        mHitFlashTime = mConfig.hitFlashDuration;
        mHitFlashAlpha = mConfig.hitFlashIntensity;
        mStats.hitCount++;
        mStats.lastHitTime = getTimeSeconds();
        
        std::cout << "KhDetectorOpenGLView: Hit detected! Flash triggered." << std::endl;
    }
    
    mLastHitState = currentHit;
}

void KhDetectorOpenGLView::renderScene()
{
    // Clear the screen
    renderBackground();
    
    // Render waveform if available
    if (mWaveformRenderer && mWaveformRenderer->isInitialized() && mWaveformBuffer) {
        // Get recent waveform samples
        auto samples = mWaveformBuffer->getSamplesInTimeWindow(mWaveformConfig.timeWindowSeconds);
        
        // Generate spectral frames if we have enough samples
        std::vector<KhDetector::SpectralFrame> spectralFrames;
        if (mSpectralAnalyzer && samples.size() >= 320) { // 20ms frame at 16kHz
            // Process samples in 320-sample chunks for spectral analysis
            for (size_t i = 0; i + 320 <= samples.size(); i += 160) { // 50% overlap
                std::vector<float> frameData(320);
                for (size_t j = 0; j < 320; ++j) {
                    frameData[j] = samples[i + j].amplitude;
                }
                spectralFrames.push_back(mSpectralAnalyzer->analyze(frameData.data(), 320));
            }
        }
        
        // Render waveform with spectral overlay
        mWaveformRenderer->render(samples, spectralFrames);
    }
    
    // Render hit flash if active (overlay on top of waveform)
    if (mConfig.showHitIndicator && mHitFlashAlpha > 0.0f) {
        renderHitFlash();
    }
    
    // Note: FPS counter is rendered using VSTGUI text rendering in draw context
    // We'll handle this in a separate method that uses the regular draw context
}

void KhDetectorOpenGLView::renderBackground()
{
    auto& bg = mConfig.backgroundColor;
    glClearColor(bg.red / 255.0f, bg.green / 255.0f, bg.blue / 255.0f, bg.alpha / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void KhDetectorOpenGLView::renderHitFlash()
{
    if (mShaderProgram == 0 || mVertexArray == 0) {
        return;
    }
    
    glUseProgram(mShaderProgram);
    
    // Set up projection matrix (orthographic)
    auto rect = getViewSize();
    float width = static_cast<float>(rect.getWidth());
    float height = static_cast<float>(rect.getHeight());
    
    // Simple orthographic projection
    float projection[16] = {
        2.0f / width, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / height, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f
    };
    
    GLint projLoc = glGetUniformLocation(mShaderProgram, "projection");
    if (projLoc >= 0) {
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);
    }
    
    // Set uniforms
    GLint alphaLoc = glGetUniformLocation(mShaderProgram, "alpha");
    if (alphaLoc >= 0) {
        glUniform1f(alphaLoc, mHitFlashAlpha);
    }
    
    GLint timeLoc = glGetUniformLocation(mShaderProgram, "time");
    if (timeLoc >= 0) {
        glUniform1f(timeLoc, getTimeSeconds());
    }
    
    // Draw flash overlay (full screen quad)
#ifdef __APPLE__
    glBindVertexArrayAPPLE(mVertexArray);
#else
    glBindVertexArray(mVertexArray);
#endif
    glDrawArrays(GL_TRIANGLES, 0, 6);
#ifdef __APPLE__
    glBindVertexArrayAPPLE(0);
#else
    glBindVertexArray(0);
#endif
    
    glUseProgram(0);
}

GLuint KhDetectorOpenGLView::createShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    // Check compilation status
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

GLuint KhDetectorOpenGLView::createShaderProgram()
{
    GLuint vertexShader = createShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader) glDeleteShader(vertexShader);
        if (fragmentShader) glDeleteShader(fragmentShader);
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    // Check linking status
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
        glDeleteProgram(program);
        program = 0;
    }
    
    // Clean up shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

void KhDetectorOpenGLView::setupVertexData()
{
    // Full screen quad vertices (for flash overlay)
    float vertices[] = {
        // Positions    // Colors (red for hit flash)
        -1.0f, -1.0f,   1.0f, 0.4f, 0.4f,
         1.0f, -1.0f,   1.0f, 0.4f, 0.4f,
         1.0f,  1.0f,   1.0f, 0.4f, 0.4f,
        
        -1.0f, -1.0f,   1.0f, 0.4f, 0.4f,
         1.0f,  1.0f,   1.0f, 0.4f, 0.4f,
        -1.0f,  1.0f,   1.0f, 0.4f, 0.4f
    };
    
#ifdef __APPLE__
    glGenVertexArraysAPPLE(1, &mVertexArray);
#else
    glGenVertexArrays(1, &mVertexArray);
#endif
    glGenBuffers(1, &mVertexBuffer);
    
#ifdef __APPLE__
    glBindVertexArrayAPPLE(mVertexArray);
#else
    glBindVertexArray(mVertexArray);
#endif
    
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
#ifdef __APPLE__
    glBindVertexArrayAPPLE(0);
#else
    glBindVertexArray(0);
#endif
}

float KhDetectorOpenGLView::getTimeSeconds() const
{
    auto currentTime = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<float>(currentTime - mStartTime).count();
}

float KhDetectorOpenGLView::smoothStep(float edge0, float edge1, float x)
{
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float KhDetectorOpenGLView::easeOutCubic(float t)
{
    return 1.0f - std::pow(1.0f - t, 3.0f);
}

void KhDetectorOpenGLView::setWaveformBuffer(std::shared_ptr<KhDetector::WaveformBuffer4K> buffer)
{
    mWaveformBuffer = buffer;
}

void KhDetectorOpenGLView::setWaveformConfig(const KhDetector::WaveformConfig& config)
{
    mWaveformConfig = config;
    if (mWaveformRenderer) {
        mWaveformRenderer->setConfig(config);
    }
}

const KhDetector::WaveformConfig& KhDetectorOpenGLView::getWaveformConfig() const
{
    return mWaveformConfig;
}

// Factory function
std::unique_ptr<KhDetectorOpenGLView> createOpenGLView(
    const VSTGUI::CRect& size, 
    std::atomic<bool>& hitState)
{
    return std::make_unique<KhDetectorOpenGLView>(size, hitState);
}

} // namespace KhDetector 