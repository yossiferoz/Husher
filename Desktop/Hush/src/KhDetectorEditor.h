#pragma once

#include "vstgui/lib/vstguifwd.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/controls/cslider.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/cparamdisplay.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/lib/cgradientview.h"
#include <atomic>
#include <chrono>

namespace KhDetector {

class KhDetectorController;

/**
 * @brief Main editor window for KhDetector plugin
 * 
 * Features:
 * - Three resizable presets: Small (760×480), Medium (1100×680), Large (1600×960)
 * - CPU meter showing average process() time
 * - Sensitivity slider (0-1) mapped to detection threshold
 * - Write Markers button for controller integration
 * - Auto-layout system for responsive design
 */
class KhDetectorEditor : public VSTGUI::CViewContainer
{
public:
    enum class UISize {
        Small,   // 760×480
        Medium,  // 1100×680
        Large    // 1600×960
    };

    struct CPUStats {
        std::atomic<float> averageProcessTime{0.0f};  // in milliseconds
        std::atomic<float> cpuUsagePercent{0.0f};     // 0-100%
        std::atomic<uint64_t> processCallCount{0};
    };

    template<typename ControllerType>
    explicit KhDetectorEditor(const VSTGUI::CRect& size, 
                             ControllerType* controller,
                             std::atomic<bool>& hitState);
    ~KhDetectorEditor();

    // CViewContainer overrides
    void setViewSize(const VSTGUI::CRect& rect, bool invalid = true) override;

    // UI size management
    void setUISize(UISize size);
    UISize getCurrentUISize() const { return mCurrentSize; }
    
    // CPU monitoring
    void updateCPUStats(float processTimeMs);
    const CPUStats& getCPUStats() const { return mCPUStats; }

    // Parameter updates
    void updateSensitivity(float value);
    void updateHitState(bool hit);
    void updateDisplay();

    // Factory function
    static std::unique_ptr<KhDetectorEditor> create(
        const VSTGUI::CRect& size,
        KhDetectorController* controller,
        std::atomic<bool>& hitState
    );

private:
    // UI creation and layout
    void createUI();
    void createHeader();
    void createMainPanel();
    void createFooter();
    void layoutComponents();
    void updateLayout();

    // Component creation helpers
    VSTGUI::CTextLabel* createLabel(const std::string& text, 
                                   const VSTGUI::CRect& rect,
                                   VSTGUI::CFontRef font = nullptr);
    VSTGUI::CSlider* createSlider(const VSTGUI::CRect& rect,
                                 VSTGUI::IControlListener* listener,
                                 int32_t tag);
    VSTGUI::CTextButton* createButton(const std::string& title,
                                     const VSTGUI::CRect& rect,
                                     VSTGUI::IControlListener* listener,
                                     int32_t tag);

    // Timer callback
    void onTimer();

    // Control event handlers
    void onSensitivityChanged(float value);
    void onWriteMarkersClicked();

    // Control listener
    class ControlListener;
    std::unique_ptr<ControlListener> mControlListener;

    // UI components
    VSTGUI::CViewContainer* mMainContainer = nullptr;
    VSTGUI::CViewContainer* mHeaderContainer = nullptr;
    VSTGUI::CViewContainer* mContentContainer = nullptr;
    VSTGUI::CViewContainer* mFooterContainer = nullptr;

    // Header components
    VSTGUI::CTextLabel* mTitleLabel = nullptr;
    VSTGUI::CTextLabel* mSizeLabel = nullptr;

    // Main panel components
    VSTGUI::CTextLabel* mCPULabel = nullptr;
    VSTGUI::CParamDisplay* mCPUMeter = nullptr;
    VSTGUI::CGradientView* mCPUBar = nullptr;
    
    VSTGUI::CTextLabel* mSensitivityLabel = nullptr;
    VSTGUI::CSlider* mSensitivitySlider = nullptr;
    VSTGUI::CParamDisplay* mSensitivityDisplay = nullptr;
    
    VSTGUI::CTextLabel* mHitLabel = nullptr;
    VSTGUI::CTextLabel* mHitIndicator = nullptr;

    // Footer components
    VSTGUI::CTextButton* mWriteMarkersButton = nullptr;
    VSTGUI::CTextButton* mSmallSizeButton = nullptr;
    VSTGUI::CTextButton* mMediumSizeButton = nullptr;
    VSTGUI::CTextButton* mLargeSizeButton = nullptr;

    // State
    KhDetectorController* mController;
    std::atomic<bool>& mHitState;
    UISize mCurrentSize = UISize::Medium;
    CPUStats mCPUStats;

    // Timing and animation
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> mUpdateTimer;
    std::chrono::high_resolution_clock::time_point mLastUpdateTime;
    
    // Hit animation
    float mHitFlashAlpha = 0.0f;
    bool mLastHitState = false;

    // Layout constants
    static constexpr int kMargin = 10;
    static constexpr int kComponentHeight = 25;
    static constexpr int kHeaderHeight = 40;
    static constexpr int kFooterHeight = 50;
    static constexpr int kSliderWidth = 200;
    static constexpr int kButtonWidth = 80;
    static constexpr int kCPUBarWidth = 150;

    // UI size dimensions
    static constexpr VSTGUI::CRect kSmallSize{0, 0, 760, 480};
    static constexpr VSTGUI::CRect kMediumSize{0, 0, 1100, 680};
    static constexpr VSTGUI::CRect kLargeSize{0, 0, 1600, 960};

    // Control tags
    enum ControlTags {
        kSensitivitySliderTag = 1000,
        kWriteMarkersButtonTag,
        kSmallSizeButtonTag,
        kMediumSizeButtonTag,
        kLargeSizeButtonTag
    };
};

} // namespace KhDetector 