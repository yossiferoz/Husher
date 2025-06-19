#include "WaveformRenderer.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cmath>

namespace KhDetector {

// Vertex shader source for waveform rendering
static const char* kVertexShaderSource = R"(
#version 330 core

layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in float aFlags;

uniform mat4 uProjectionMatrix;
uniform float uTimeOffset;
uniform float uAmplitudeScale;

out vec4 vColor;
out vec2 vTexCoord;
out float vFlags;

void main()
{
    vec2 pos = aPosition;
    pos.x += uTimeOffset;
    pos.y *= uAmplitudeScale;
    
    gl_Position = uProjectionMatrix * vec4(pos, 0.0, 1.0);
    vColor = aColor;
    vTexCoord = aTexCoord;
    vFlags = aFlags;
}
)";

// Fragment shader source for waveform rendering
static const char* kFragmentShaderSource = R"(
#version 330 core

in vec4 vColor;
in vec2 vTexCoord;
in float vFlags;

uniform vec4 uWaveformColor;
uniform vec4 uSpectralColor;
uniform vec4 uHitFlagColor;
uniform vec4 uBackgroundColor;
uniform vec4 uGridColor;
uniform int uFlags;

out vec4 FragColor;

void main()
{
    vec4 color = vColor;
    
    // Check flags for special rendering
    if (mod(vFlags, 2.0) >= 1.0) { // HIT_FLAG
        color = mix(color, uHitFlagColor, 0.7);
    }
    
    if (mod(floor(vFlags / 2.0), 2.0) >= 1.0) { // SPECTRAL_FLAG
        color = mix(color, uSpectralColor, 0.5);
    }
    
    if (mod(floor(vFlags / 4.0), 2.0) >= 1.0) { // GRID_FLAG
        color = uGridColor;
    }
    
    // Apply some smoothing for anti-aliasing
    float alpha = color.a;
    if (length(vTexCoord - vec2(0.5, 0.5)) > 0.5) {
        alpha *= (1.0 - smoothstep(0.4, 0.5, length(vTexCoord - vec2(0.5, 0.5))));
    }
    
    FragColor = vec4(color.rgb, alpha);
}
)";

// WaveformShader implementation
WaveformShader::WaveformShader() 
    : program_(0), vertexShader_(0), fragmentShader_(0), initialized_(false) {
    // Initialize uniform locations to -1
    projectionMatrixLoc_ = timeOffsetLoc_ = amplitudeScaleLoc_ = -1;
    waveformColorLoc_ = spectralColorLoc_ = hitFlagColorLoc_ = -1;
    backgroundColorLoc_ = gridColorLoc_ = flagsLoc_ = -1;
}

WaveformShader::~WaveformShader() {
    cleanup();
}

bool WaveformShader::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Create vertex shader
    vertexShader_ = glCreateShader(GL_VERTEX_SHADER);
    if (!compileShader(vertexShader_, kVertexShaderSource)) {
        std::cerr << "Failed to compile vertex shader" << std::endl;
        return false;
    }
    
    // Create fragment shader
    fragmentShader_ = glCreateShader(GL_FRAGMENT_SHADER);
    if (!compileShader(fragmentShader_, kFragmentShaderSource)) {
        std::cerr << "Failed to compile fragment shader" << std::endl;
        return false;
    }
    
    // Create program
    program_ = glCreateProgram();
    glAttachShader(program_, vertexShader_);
    glAttachShader(program_, fragmentShader_);
    
    if (!linkProgram()) {
        std::cerr << "Failed to link shader program" << std::endl;
        return false;
    }
    
    findUniformLocations();
    initialized_ = true;
    return true;
}

void WaveformShader::cleanup() {
    if (program_) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    if (vertexShader_) {
        glDeleteShader(vertexShader_);
        vertexShader_ = 0;
    }
    if (fragmentShader_) {
        glDeleteShader(fragmentShader_);
        fragmentShader_ = 0;
    }
    initialized_ = false;
}

void WaveformShader::use() {
    if (initialized_) {
        glUseProgram(program_);
    }
}

bool WaveformShader::compileShader(GLuint shader, const char* source) {
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << std::endl;
        return false;
    }
    return true;
}

bool WaveformShader::linkProgram() {
    glLinkProgram(program_);
    
    GLint success;
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(program_, 512, nullptr, infoLog);
        std::cerr << "Program linking error: " << infoLog << std::endl;
        return false;
    }
    return true;
}

void WaveformShader::findUniformLocations() {
    projectionMatrixLoc_ = glGetUniformLocation(program_, "uProjectionMatrix");
    timeOffsetLoc_ = glGetUniformLocation(program_, "uTimeOffset");
    amplitudeScaleLoc_ = glGetUniformLocation(program_, "uAmplitudeScale");
    waveformColorLoc_ = glGetUniformLocation(program_, "uWaveformColor");
    spectralColorLoc_ = glGetUniformLocation(program_, "uSpectralColor");
    hitFlagColorLoc_ = glGetUniformLocation(program_, "uHitFlagColor");
    backgroundColorLoc_ = glGetUniformLocation(program_, "uBackgroundColor");
    gridColorLoc_ = glGetUniformLocation(program_, "uGridColor");
    flagsLoc_ = glGetUniformLocation(program_, "uFlags");
}

void WaveformShader::setProjectionMatrix(const float* matrix) {
    if (projectionMatrixLoc_ >= 0) {
        glUniformMatrix4fv(projectionMatrixLoc_, 1, GL_FALSE, matrix);
    }
}

void WaveformShader::setTimeOffset(float offset) {
    if (timeOffsetLoc_ >= 0) {
        glUniform1f(timeOffsetLoc_, offset);
    }
}

void WaveformShader::setAmplitudeScale(float scale) {
    if (amplitudeScaleLoc_ >= 0) {
        glUniform1f(amplitudeScaleLoc_, scale);
    }
}

void WaveformShader::setColors(const WaveformConfig::Colors& colors) {
    if (waveformColorLoc_ >= 0) {
        glUniform4fv(waveformColorLoc_, 1, colors.waveform);
    }
    if (spectralColorLoc_ >= 0) {
        glUniform4fv(spectralColorLoc_, 1, colors.spectral);
    }
    if (hitFlagColorLoc_ >= 0) {
        glUniform4fv(hitFlagColorLoc_, 1, colors.hitFlag);
    }
    if (backgroundColorLoc_ >= 0) {
        glUniform4fv(backgroundColorLoc_, 1, colors.background);
    }
    if (gridColorLoc_ >= 0) {
        glUniform4fv(gridColorLoc_, 1, colors.grid);
    }
}

void WaveformShader::setFlags(int flags) {
    if (flagsLoc_ >= 0) {
        glUniform1i(flagsLoc_, flags);
    }
}

GLuint WaveformShader::getProgram() const {
    return program_;
}

bool WaveformShader::isInitialized() const {
    return initialized_;
}

// VBOManager implementation
VBOManager::VBOManager() 
    : currentBuffer_(0), maxVertices_(0), currentVertexCount_(0), initialized_(false) {
    for (int i = 0; i < kNumBuffers; ++i) {
        vbos_[i] = 0;
        vaos_[i] = 0;
    }
}

VBOManager::~VBOManager() {
    cleanup();
}

bool VBOManager::initialize(size_t maxVertices) {
    if (initialized_) {
        return true;
    }
    
    maxVertices_ = maxVertices;
    
    // Generate VBOs and VAOs
    glGenBuffers(kNumBuffers, vbos_);
    glGenVertexArrays(kNumBuffers, vaos_);
    
    // Setup each buffer
    for (int i = 0; i < kNumBuffers; ++i) {
        glBindVertexArray(vaos_[i]);
        glBindBuffer(GL_ARRAY_BUFFER, vbos_[i]);
        
        // Allocate buffer memory
        glBufferData(GL_ARRAY_BUFFER, 
                    maxVertices * sizeof(WaveformVertex), 
                    nullptr, GL_DYNAMIC_DRAW);
        
        // Setup vertex attributes
        // Position
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(WaveformVertex), 
                             (void*)offsetof(WaveformVertex, position));
        glEnableVertexAttribArray(0);
        
        // Color
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(WaveformVertex), 
                             (void*)offsetof(WaveformVertex, color));
        glEnableVertexAttribArray(1);
        
        // Texture coordinates
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(WaveformVertex), 
                             (void*)offsetof(WaveformVertex, texCoord));
        glEnableVertexAttribArray(2);
        
        // Flags
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(WaveformVertex), 
                             (void*)offsetof(WaveformVertex, flags));
        glEnableVertexAttribArray(3);
    }
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    initialized_ = true;
    return true;
}

void VBOManager::cleanup() {
    if (initialized_) {
        glDeleteBuffers(kNumBuffers, vbos_);
        glDeleteVertexArrays(kNumBuffers, vaos_);
        
        for (int i = 0; i < kNumBuffers; ++i) {
            vbos_[i] = 0;
            vaos_[i] = 0;
        }
        
        initialized_ = false;
    }
}

GLuint VBOManager::getCurrentVBO() {
    return initialized_ ? vaos_[currentBuffer_] : 0;
}

GLuint VBOManager::getPreviousVBO() {
    int prevBuffer = (currentBuffer_ + kNumBuffers - 1) % kNumBuffers;
    return initialized_ ? vaos_[prevBuffer] : 0;
}

void VBOManager::swapBuffers() {
    if (initialized_) {
        currentBuffer_ = (currentBuffer_ + 1) % kNumBuffers;
    }
}

bool VBOManager::updateVertices(const std::vector<WaveformVertex>& vertices) {
    if (!initialized_ || vertices.size() > maxVertices_) {
        return false;
    }
    
    currentVertexCount_ = vertices.size();
    
    glBindBuffer(GL_ARRAY_BUFFER, vbos_[currentBuffer_]);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 
                   vertices.size() * sizeof(WaveformVertex), 
                   vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    return true;
}

size_t VBOManager::getMaxVertices() const {
    return maxVertices_;
}

size_t VBOManager::getCurrentVertexCount() const {
    return currentVertexCount_;
}

// WaveformRenderer::Impl
class WaveformRenderer::Impl {
public:
    explicit Impl(const WaveformConfig& config) 
        : config_(config), initialized_(false), viewportWidth_(0), viewportHeight_(0) {
        // Initialize timing
        lastUpdateTime_ = std::chrono::high_resolution_clock::now();
        frameCount_ = 0;
        fpsUpdateTimer_ = lastUpdateTime_;
        
        // Initialize scroll position
        scrollPosition_ = 0.0f;
    }
    
    ~Impl() {
        cleanup();
    }
    
    bool initialize(int viewportWidth, int viewportHeight) {
        if (initialized_) {
            return true;
        }
        
        viewportWidth_ = viewportWidth;
        viewportHeight_ = viewportHeight;
        
        // Initialize shader
        if (!shader_.initialize()) {
            std::cerr << "Failed to initialize waveform shader" << std::endl;
            return false;
        }
        
        // Initialize VBO manager
        const size_t maxVertices = 100000; // Support up to 100k vertices
        if (!vboManager_.initialize(maxVertices)) {
            std::cerr << "Failed to initialize VBO manager" << std::endl;
            return false;
        }
        
        // Initialize SMAA if enabled
        if (config_.enableSMAA) {
            if (!smaaHelper_.initialize(viewportWidth, viewportHeight)) {
                std::cerr << "Warning: Failed to initialize SMAA, continuing without" << std::endl;
                config_.enableSMAA = false;
            }
        }
        
        // Setup OpenGL state
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        
        // Disable VSync if requested
        if (!config_.enableVSync) {
#ifdef __APPLE__
            // macOS specific VSync disable
            GLint swapInterval = 0;
            // Note: This would require platform-specific context access
#endif
        }
        
        initialized_ = true;
        return true;
    }
    
    void cleanup() {
        if (initialized_) {
            shader_.cleanup();
            vboManager_.cleanup();
            smaaHelper_.cleanup();
            initialized_ = false;
        }
    }
    
    void render(const std::vector<WaveformSample>& samples,
                const std::vector<SpectralFrame>& spectralFrames) {
        if (!initialized_) {
            return;
        }
        
        auto frameStart = std::chrono::high_resolution_clock::now();
        
        // Update scroll position based on time
        updateScrollPosition();
        
        // Generate vertices from samples
        std::vector<WaveformVertex> vertices;
        generateWaveformVertices(samples, vertices);
        
        // Add spectral overlay if enabled
        if (config_.showSpectralOverlay && !spectralFrames.empty()) {
            generateSpectralVertices(spectralFrames, vertices);
        }
        
        // Add grid if enabled
        if (config_.showGrid) {
            generateGridVertices(vertices);
        }
        
        // Update VBO with new vertices
        vboManager_.updateVertices(vertices);
        
        // Begin SMAA pass if enabled
        GLuint renderTarget = 0;
        if (config_.enableSMAA && smaaHelper_.isInitialized()) {
            renderTarget = smaaHelper_.beginSMAAPass();
        }
        
        // Clear background
        glClearColor(config_.colors.background[0], config_.colors.background[1], 
                    config_.colors.background[2], config_.colors.background[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Setup projection matrix
        float projMatrix[16];
        setupProjectionMatrix(projMatrix);
        
        // Render waveform
        shader_.use();
        shader_.setProjectionMatrix(projMatrix);
        shader_.setTimeOffset(scrollPosition_);
        shader_.setAmplitudeScale(1.0f);
        shader_.setColors(config_.colors);
        shader_.setFlags(0);
        
        // Draw vertices
        glBindVertexArray(vboManager_.getCurrentVBO());
        glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(vboManager_.getCurrentVertexCount()));
        glBindVertexArray(0);
        
        // End SMAA pass if enabled
        if (config_.enableSMAA && smaaHelper_.isInitialized()) {
            smaaHelper_.endSMAAPass();
        }
        
        // Swap VBO buffers for next frame
        vboManager_.swapBuffers();
        
        // Update performance statistics
        updatePerformanceStats(frameStart);
    }
    
    void setViewport(int width, int height) {
        viewportWidth_ = width;
        viewportHeight_ = height;
        glViewport(0, 0, width, height);
        
        if (config_.enableSMAA && smaaHelper_.isInitialized()) {
            smaaHelper_.resize(width, height);
        }
    }
    
    void setConfig(const WaveformConfig& config) {
        config_ = config;
    }
    
    const WaveformConfig& getConfig() const {
        return config_;
    }
    
    const PerformanceStats& getPerformanceStats() const {
        return stats_;
    }
    
    bool isInitialized() const {
        return initialized_;
    }

private:
    WaveformConfig config_;
    bool initialized_;
    int viewportWidth_, viewportHeight_;
    
    // Rendering components
    WaveformShader shader_;
    VBOManager vboManager_;
    SMAAHelper smaaHelper_;
    
    // Timing and animation
    std::chrono::high_resolution_clock::time_point lastUpdateTime_;
    std::chrono::high_resolution_clock::time_point fpsUpdateTimer_;
    float scrollPosition_;
    int frameCount_;
    
    // Performance tracking
    PerformanceStats stats_;
    
    void updateScrollPosition() {
        auto now = std::chrono::high_resolution_clock::now();
        auto deltaTime = std::chrono::duration<float>(now - lastUpdateTime_).count();
        lastUpdateTime_ = now;
        
        // Scroll based on time window
        float scrollSpeed = 1.0f / config_.timeWindowSeconds;
        scrollPosition_ += scrollSpeed * deltaTime;
        
        // Recycle every 50ms as requested
        if (scrollPosition_ >= config_.updateIntervalMs / 1000.0f) {
            scrollPosition_ = 0.0f;
        }
    }
    
    void generateWaveformVertices(const std::vector<WaveformSample>& samples,
                                 std::vector<WaveformVertex>& vertices) {
        if (samples.empty()) {
            return;
        }
        
        vertices.reserve(vertices.size() + samples.size() * 2); // Line strip needs 2 vertices per sample
        
        float timeStep = config_.timeWindowSeconds / config_.maxSamplesPerLine;
        
        for (size_t i = 0; i < samples.size(); ++i) {
            const auto& sample = samples[i];
            float x = static_cast<float>(i) * timeStep;
            float y = sample.amplitude;
            
            // Create vertex with appropriate color and flags
            WaveformVertex vertex(x, y, 
                                config_.colors.waveform[0], 
                                config_.colors.waveform[1], 
                                config_.colors.waveform[2], 
                                config_.colors.waveform[3]);
            
            // Set hit flag if this sample is a hit
            if (sample.isHit) {
                vertex.flags += WaveformVertex::HIT_FLAG;
            }
            
            vertices.push_back(vertex);
        }
    }
    
    void generateSpectralVertices(const std::vector<SpectralFrame>& spectralFrames,
                                 std::vector<WaveformVertex>& vertices) {
        if (spectralFrames.empty()) {
            return;
        }
        
        float timeStep = config_.timeWindowSeconds / spectralFrames.size();
        
        for (size_t frameIdx = 0; frameIdx < spectralFrames.size(); ++frameIdx) {
            const auto& frame = spectralFrames[frameIdx];
            float x = static_cast<float>(frameIdx) * timeStep;
            
            // Create spectral overlay vertices
            for (int bin = 0; bin < SpectralFrame::kNumBins; ++bin) {
                float magnitude = frame.magnitudes[bin];
                if (magnitude < config_.spectralThreshold) {
                    continue; // Skip low-magnitude bins
                }
                
                float binFreq = static_cast<float>(bin) / SpectralFrame::kNumBins;
                float y = magnitude * 0.5f; // Scale spectral data
                
                WaveformVertex vertex(x, y,
                                    config_.colors.spectral[0],
                                    config_.colors.spectral[1],
                                    config_.colors.spectral[2],
                                    config_.colors.spectral[3]);
                
                vertex.flags += WaveformVertex::SPECTRAL_FLAG;
                vertex.texCoord[0] = binFreq;
                vertex.texCoord[1] = magnitude;
                
                vertices.push_back(vertex);
            }
        }
    }
    
    void generateGridVertices(std::vector<WaveformVertex>& vertices) {
        // Add time grid lines (vertical)
        const int numTimeLines = 10;
        for (int i = 0; i <= numTimeLines; ++i) {
            float x = static_cast<float>(i) / numTimeLines * config_.timeWindowSeconds;
            
            // Top point
            WaveformVertex topVertex(x, 1.0f,
                                   config_.colors.grid[0],
                                   config_.colors.grid[1],
                                   config_.colors.grid[2],
                                   config_.colors.grid[3]);
            topVertex.flags += WaveformVertex::GRID_FLAG;
            
            // Bottom point
            WaveformVertex bottomVertex(x, -1.0f,
                                      config_.colors.grid[0],
                                      config_.colors.grid[1],
                                      config_.colors.grid[2],
                                      config_.colors.grid[3]);
            bottomVertex.flags += WaveformVertex::GRID_FLAG;
            
            vertices.push_back(topVertex);
            vertices.push_back(bottomVertex);
        }
        
        // Add amplitude grid lines (horizontal)
        const int numAmpLines = 8;
        for (int i = 0; i <= numAmpLines; ++i) {
            float y = (static_cast<float>(i) / numAmpLines - 0.5f) * 2.0f; // -1 to 1
            
            // Left point
            WaveformVertex leftVertex(0.0f, y,
                                    config_.colors.grid[0],
                                    config_.colors.grid[1],
                                    config_.colors.grid[2],
                                    config_.colors.grid[3]);
            leftVertex.flags += WaveformVertex::GRID_FLAG;
            
            // Right point
            WaveformVertex rightVertex(config_.timeWindowSeconds, y,
                                     config_.colors.grid[0],
                                     config_.colors.grid[1],
                                     config_.colors.grid[2],
                                     config_.colors.grid[3]);
            rightVertex.flags += WaveformVertex::GRID_FLAG;
            
            vertices.push_back(leftVertex);
            vertices.push_back(rightVertex);
        }
    }
    
    void setupProjectionMatrix(float* matrix) {
        // Simple orthographic projection
        float left = 0.0f;
        float right = config_.timeWindowSeconds;
        float bottom = -1.0f;
        float top = 1.0f;
        float near = -1.0f;
        float far = 1.0f;
        
        // Clear matrix
        memset(matrix, 0, 16 * sizeof(float));
        
        // Orthographic projection matrix
        matrix[0] = 2.0f / (right - left);
        matrix[5] = 2.0f / (top - bottom);
        matrix[10] = -2.0f / (far - near);
        matrix[12] = -(right + left) / (right - left);
        matrix[13] = -(top + bottom) / (top - bottom);
        matrix[14] = -(far + near) / (far - near);
        matrix[15] = 1.0f;
    }
    
    void updatePerformanceStats(std::chrono::high_resolution_clock::time_point frameStart) {
        auto frameEnd = std::chrono::high_resolution_clock::now();
        stats_.frameDuration = frameEnd - frameStart;
        stats_.renderTimeMs = stats_.frameDuration.count() * 1000.0f;
        stats_.lastFrameTime = frameEnd;
        stats_.vertexCount = static_cast<int>(vboManager_.getCurrentVertexCount());
        stats_.drawCalls = 1; // Simple for now
        stats_.vboSwapped = true;
        
        frameCount_++;
        
        // Update FPS every second
        auto now = std::chrono::high_resolution_clock::now();
        auto fpsElapsed = std::chrono::duration<float>(now - fpsUpdateTimer_).count();
        if (fpsElapsed >= 1.0f) {
            stats_.currentFPS = frameCount_ / fpsElapsed;
            
            // Update running average
            static float totalFPS = 0.0f;
            static int fpsCount = 0;
            totalFPS += stats_.currentFPS;
            fpsCount++;
            stats_.averageFPS = totalFPS / fpsCount;
            
            frameCount_ = 0;
            fpsUpdateTimer_ = now;
        }
    }
};

// WaveformRenderer implementation
WaveformRenderer::WaveformRenderer(const WaveformConfig& config) 
    : pImpl_(std::make_unique<Impl>(config)) {}

WaveformRenderer::~WaveformRenderer() = default;

bool WaveformRenderer::initialize(int viewportWidth, int viewportHeight) {
    return pImpl_->initialize(viewportWidth, viewportHeight);
}

void WaveformRenderer::cleanup() {
    pImpl_->cleanup();
}

void WaveformRenderer::render(const std::vector<WaveformSample>& samples,
                             const std::vector<SpectralFrame>& spectralFrames) {
    pImpl_->render(samples, spectralFrames);
}

void WaveformRenderer::setViewport(int width, int height) {
    pImpl_->setViewport(width, height);
}

void WaveformRenderer::setConfig(const WaveformConfig& config) {
    pImpl_->setConfig(config);
}

const WaveformConfig& WaveformRenderer::getConfig() const {
    return pImpl_->getConfig();
}

const WaveformRenderer::PerformanceStats& WaveformRenderer::getPerformanceStats() const {
    return pImpl_->getPerformanceStats();
}

bool WaveformRenderer::isInitialized() const {
    return pImpl_->isInitialized();
}

// SMAAHelper implementation (simplified version)
class SMAAHelper::Impl {
public:
    Impl() : initialized_(false), width_(0), height_(0), framebuffer_(0), colorTexture_(0) {}
    
    ~Impl() {
        cleanup();
    }
    
    bool initialize(int width, int height) {
        width_ = width;
        height_ = height;
        
        // Create framebuffer for SMAA
        glGenFramebuffers(1, &framebuffer_);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        
        // Create color texture
        glGenTextures(1, &colorTexture_);
        glBindTexture(GL_TEXTURE_2D, colorTexture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture_, 0);
        
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "SMAA framebuffer not complete" << std::endl;
            cleanup();
            return false;
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        initialized_ = true;
        return true;
    }
    
    void cleanup() {
        if (framebuffer_) {
            glDeleteFramebuffers(1, &framebuffer_);
            framebuffer_ = 0;
        }
        if (colorTexture_) {
            glDeleteTextures(1, &colorTexture_);
            colorTexture_ = 0;
        }
        initialized_ = false;
    }
    
    void resize(int width, int height) {
        if (width_ == width && height_ == height) {
            return;
        }
        
        cleanup();
        initialize(width, height);
    }
    
    GLuint beginSMAAPass() {
        if (initialized_) {
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            glViewport(0, 0, width_, height_);
            return framebuffer_;
        }
        return 0;
    }
    
    void endSMAAPass() {
        if (initialized_) {
            // Simple blit for now - real SMAA would do edge detection + blending
            glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, 
                             GL_COLOR_BUFFER_BIT, GL_LINEAR);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }
    
    bool isInitialized() const {
        return initialized_;
    }

private:
    bool initialized_;
    int width_, height_;
    GLuint framebuffer_;
    GLuint colorTexture_;
};

SMAAHelper::SMAAHelper() : pImpl_(std::make_unique<Impl>()) {}
SMAAHelper::~SMAAHelper() = default;

bool SMAAHelper::initialize(int width, int height) {
    return pImpl_->initialize(width, height);
}

void SMAAHelper::cleanup() {
    pImpl_->cleanup();
}

void SMAAHelper::resize(int width, int height) {
    pImpl_->resize(width, height);
}

GLuint SMAAHelper::beginSMAAPass() {
    return pImpl_->beginSMAAPass();
}

void SMAAHelper::endSMAAPass() {
    pImpl_->endSMAAPass();
}

bool SMAAHelper::isInitialized() const {
    return pImpl_->isInitialized();
}

} // namespace KhDetector 