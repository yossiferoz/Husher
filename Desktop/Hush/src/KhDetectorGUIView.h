#pragma once

#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/cvstguitimer.h"
#include "KhDetectorOpenGLView.h"
#include <atomic>
#include <memory>

namespace KhDetector {

/**
 * @brief Main GUI view combining OpenGL rendering with text overlays
 * 
 * This view contains:
 * - OpenGL view for hit flash effects and background
 * - Text labels for FPS counter and statistics
 * - Real-time updates and animations
 */
class KhDetectorGUIView : public VSTGUI::CViewContainer
{
public:
    /**
     * @brief Constructor
     * 
     * @param size View size
     * @param hitState Reference to atomic hit state from processor
     */
    KhDetectorGUIView(const VSTGUI::CRect& size, std::atomic<bool>& hitState);
    
    /**
     * @brief Destructor
     */
    ~KhDetectorGUIView() override;
    
    // CView overrides
    void setViewSize(const VSTGUI::CRect& rect, bool invalid = true) override;
    
    // Timer callback
    void onTimer();
    
    /**
     * @brief Start/stop GUI updates
     */
    void startUpdates();
    void stopUpdates();
    
    /**
     * @brief Configuration for GUI appearance
     */
    struct Config
    {
        // Text appearance
        VSTGUI::CColor textColor = VSTGUI::CColor(255, 255, 255, 255);
        VSTGUI::CColor backgroundColor = VSTGUI::CColor(0, 0, 0, 0); // Transparent
        float fontSize = 12.0f;
        std::string fontName = "Arial";
        
        // Update rates
        float textUpdateRate = 10.0f;  // Hz for text updates
        float openglUpdateRate = 60.0f; // Hz for OpenGL updates
        
        // Display options
        bool showFPS = true;
        bool showHitCounter = true;
        bool showStatistics = true;
        
        // Layout
        float margin = 10.0f;
        float lineHeight = 16.0f;
    };
    
    /**
     * @brief Update configuration
     */
    void setConfig(const Config& config);
    const Config& getConfig() const { return mConfig; }
    
    /**
     * @brief Get the OpenGL view for direct access
     */
    KhDetectorOpenGLView* getOpenGLView() const { return mOpenGLView.get(); }

private:
    // Configuration
    Config mConfig;
    
    // Hit state reference
    std::atomic<bool>& mHitState;
    
    // Child views
    std::unique_ptr<KhDetectorOpenGLView> mOpenGLView;
    VSTGUI::CTextLabel* mFPSLabel = nullptr;
    VSTGUI::CTextLabel* mHitCounterLabel = nullptr;
    VSTGUI::CTextLabel* mStatisticsLabel = nullptr;
    
    // Update timer
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> mUpdateTimer;
    bool mUpdatesActive = false;
    
    // Fonts
    VSTGUI::SharedPointer<VSTGUI::CFontDesc> mFont;
    
    /**
     * @brief Create and setup child views
     */
    void createChildViews();
    
    /**
     * @brief Update text displays
     */
    void updateTextDisplays();
    
    /**
     * @brief Update FPS display
     */
    void updateFPSDisplay();
    
    /**
     * @brief Update hit counter display
     */
    void updateHitCounterDisplay();
    
    /**
     * @brief Update statistics display
     */
    void updateStatisticsDisplay();
    
    /**
     * @brief Layout child views
     */
    void layoutChildViews();
    
    /**
     * @brief Create text label with standard styling
     */
    VSTGUI::CTextLabel* createStyledLabel(const VSTGUI::CRect& rect, const std::string& text);
};

/**
 * @brief Factory function to create the main GUI view
 */
std::unique_ptr<KhDetectorGUIView> createGUIView(
    const VSTGUI::CRect& size,
    std::atomic<bool>& hitState
);

} // namespace KhDetector 