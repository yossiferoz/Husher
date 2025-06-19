#include "KhDetectorEditor.h"
#include "KhDetectorController.h"
#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/controls/cslider.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/cparamdisplay.h"
#include "vstgui/lib/cgradientview.h"
#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/lib/controls/icontrollistener.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace KhDetector {

//==============================================================================
// Control Listener Implementation
//==============================================================================
class KhDetectorEditor::ControlListener : public VSTGUI::IControlListener
{
public:
    explicit ControlListener(KhDetectorEditor* editor) : mEditor(editor) {}
    
    void valueChanged(VSTGUI::CControl* control) override
    {
        if (!mEditor || !control) return;
        
        switch (control->getTag()) {
            case kSensitivitySliderTag:
                mEditor->onSensitivityChanged(control->getValueNormalized());
                break;
            case kWriteMarkersButtonTag:
                mEditor->onWriteMarkersClicked();
                break;
            case kSmallSizeButtonTag:
                mEditor->setUISize(UISize::Small);
                break;
            case kMediumSizeButtonTag:
                mEditor->setUISize(UISize::Medium);
                break;
            case kLargeSizeButtonTag:
                mEditor->setUISize(UISize::Large);
                break;
        }
    }
    
    void controlBeginEdit(VSTGUI::CControl* control) override {}
    void controlEndEdit(VSTGUI::CControl* control) override {}
    
private:
    KhDetectorEditor* mEditor;
};

//==============================================================================
// KhDetectorEditor Implementation
//==============================================================================
KhDetectorEditor::KhDetectorEditor(const VSTGUI::CRect& size, 
                                 KhDetectorController* controller,
                                 std::atomic<bool>& hitState)
    : VSTGUI::CViewContainer(size)
    , mController(controller)
    , mHitState(hitState)
    , mControlListener(std::make_unique<ControlListener>(this))
    , mLastUpdateTime(std::chrono::high_resolution_clock::now())
{
    // Set frame properties
    setBackgroundColor(VSTGUI::CColor(40, 40, 40, 255));
    
    // Create UI components
    createUI();
    
    // Start update timer (30 FPS)
    mUpdateTimer = VSTGUI::makeOwned<VSTGUI::CVSTGUITimer>([this](VSTGUI::CVSTGUITimer*) {
        onTimer();
    }, 33, true);
    
    std::cout << "KhDetectorEditor: Created with size " 
              << size.getWidth() << "x" << size.getHeight() << std::endl;
}

KhDetectorEditor::~KhDetectorEditor()
{
    if (mUpdateTimer) {
        mUpdateTimer->stop();
        mUpdateTimer = nullptr;
    }
    
    std::cout << "KhDetectorEditor: Destroyed" << std::endl;
}

void KhDetectorEditor::setViewSize(const VSTGUI::CRect& rect, bool invalid)
{
    CViewContainer::setViewSize(rect, invalid);
    updateLayout();
}

void KhDetectorEditor::updateDisplay()
{
    
    // Update CPU stats display
    if (mCPUMeter && mCPUBar) {
        float cpuPercent = mCPUStats.cpuUsagePercent.load();
        
        // Update CPU meter text
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << cpuPercent << "%";
        mCPUMeter->setText(oss.str().c_str());
        
        // Update CPU bar gradient
        float normalizedCPU = std::min(cpuPercent / 100.0f, 1.0f);
        VSTGUI::CColor lowColor(0, 255, 0, 255);    // Green
        VSTGUI::CColor midColor(255, 255, 0, 255);  // Yellow
        VSTGUI::CColor highColor(255, 0, 0, 255);   // Red
        
        VSTGUI::CColor barColor;
        if (normalizedCPU < 0.5f) {
            // Green to Yellow
            float t = normalizedCPU * 2.0f;
            barColor.red = static_cast<uint8_t>(lowColor.red * (1-t) + midColor.red * t);
            barColor.green = static_cast<uint8_t>(lowColor.green * (1-t) + midColor.green * t);
            barColor.blue = static_cast<uint8_t>(lowColor.blue * (1-t) + midColor.blue * t);
        } else {
            // Yellow to Red
            float t = (normalizedCPU - 0.5f) * 2.0f;
            barColor.red = static_cast<uint8_t>(midColor.red * (1-t) + highColor.red * t);
            barColor.green = static_cast<uint8_t>(midColor.green * (1-t) + highColor.green * t);
            barColor.blue = static_cast<uint8_t>(midColor.blue * (1-t) + highColor.blue * t);
        }
        barColor.alpha = 255;
        
        // Update gradient
        VSTGUI::CGradient* gradient = VSTGUI::CGradient::create(0.0, 1.0, VSTGUI::CColor(60, 60, 60, 255), barColor);
        mCPUBar->setGradient(gradient);
        gradient->forget();
    }
}

void KhDetectorEditor::setUISize(UISize size)
{
    if (mCurrentSize == size) return;
    
    mCurrentSize = size;
    
    VSTGUI::CRect newSize;
    std::string sizeText;
    
    switch (size) {
        case UISize::Small:
            newSize = kSmallSize;
            sizeText = "Small (760×480)";
            break;
        case UISize::Medium:
            newSize = kMediumSize;
            sizeText = "Medium (1100×680)";
            break;
        case UISize::Large:
            newSize = kLargeSize;
            sizeText = "Large (1600×960)";
            break;
    }
    
    // Update size label
    if (mSizeLabel) {
        mSizeLabel->setText(sizeText.c_str());
    }
    
    // Resize frame
    setViewSize(newSize, true);
    
    std::cout << "KhDetectorEditor: UI size changed to " << sizeText << std::endl;
}

void KhDetectorEditor::updateCPUStats(float processTimeMs)
{
    // Update process call count
    mCPUStats.processCallCount.fetch_add(1);
    
    // Calculate moving average of process time
    float currentAvg = mCPUStats.averageProcessTime.load();
    float newAvg = currentAvg * 0.95f + processTimeMs * 0.05f;  // Exponential moving average
    mCPUStats.averageProcessTime.store(newAvg);
    
    // Estimate CPU usage based on process time and sample rate
    // Assuming 44.1kHz, 512 samples per buffer = ~11.6ms per buffer
    float bufferTimeMs = 11.6f;  // Approximate buffer time
    float cpuPercent = (newAvg / bufferTimeMs) * 100.0f;
    mCPUStats.cpuUsagePercent.store(std::min(cpuPercent, 100.0f));
}

void KhDetectorEditor::updateSensitivity(float value)
{
    if (mSensitivitySlider) {
        mSensitivitySlider->setValue(value);
    }
    
    if (mSensitivityDisplay) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << value;
        mSensitivityDisplay->setText(oss.str().c_str());
    }
}

void KhDetectorEditor::updateHitState(bool hit)
{
    if (hit != mLastHitState) {
        mLastHitState = hit;
        
        if (hit) {
            mHitFlashAlpha = 1.0f;  // Start flash animation
        }
    }
    
    // Update hit indicator
    if (mHitIndicator) {
        if (hit) {
            mHitIndicator->setText("HIT!");
            mHitIndicator->setFontColor(VSTGUI::CColor(255, 0, 0, 255));  // Red
        } else {
            mHitIndicator->setText("---");
            mHitIndicator->setFontColor(VSTGUI::CColor(128, 128, 128, 255));  // Gray
        }
    }
}

std::unique_ptr<KhDetectorEditor> KhDetectorEditor::create(
    const VSTGUI::CRect& size,
    KhDetectorController* controller,
    std::atomic<bool>& hitState)
{
    return std::make_unique<KhDetectorEditor>(size, controller, hitState);
}

//==============================================================================
// UI Creation Methods
//==============================================================================
void KhDetectorEditor::createUI()
{
    // Create main container
    VSTGUI::CRect containerRect = getViewSize();
    containerRect.offset(-containerRect.left, -containerRect.top);
    
    mMainContainer = new VSTGUI::CViewContainer(containerRect);
    mMainContainer->setBackgroundColor(VSTGUI::CColor(50, 50, 50, 255));
    addView(mMainContainer);
    
    createHeader();
    createMainPanel();
    createFooter();
    
    layoutComponents();
}

void KhDetectorEditor::createHeader()
{
    VSTGUI::CRect headerRect(0, 0, getViewSize().getWidth(), kHeaderHeight);
    
    mHeaderContainer = new VSTGUI::CViewContainer(headerRect);
    mHeaderContainer->setBackgroundColor(VSTGUI::CColor(60, 60, 60, 255));
    mMainContainer->addView(mHeaderContainer);
    
    // Title label
    VSTGUI::CRect titleRect(kMargin, 5, 300, kHeaderHeight - 5);
    mTitleLabel = createLabel("KhDetector - Audio Hit Detection", titleRect);
    mTitleLabel->setFontColor(VSTGUI::CColor(255, 255, 255, 255));
    mTitleLabel->setFont(VSTGUI::kSystemFont, 16);
    mHeaderContainer->addView(mTitleLabel);
    
    // Size label (right-aligned)
    VSTGUI::CRect sizeRect(headerRect.getWidth() - 200 - kMargin, 5, 200, kHeaderHeight - 5);
    mSizeLabel = createLabel("Medium (1100×680)", sizeRect);
    mSizeLabel->setFontColor(VSTGUI::CColor(200, 200, 200, 255));
    mSizeLabel->setHoriAlign(VSTGUI::kRightText);
    mHeaderContainer->addView(mSizeLabel);
}

void KhDetectorEditor::createMainPanel()
{
    VSTGUI::CRect contentRect(0, kHeaderHeight, getViewSize().getWidth(), 
                             getViewSize().getHeight() - kHeaderHeight - kFooterHeight);
    
    mContentContainer = new VSTGUI::CViewContainer(contentRect);
    mContentContainer->setBackgroundColor(VSTGUI::CColor(45, 45, 45, 255));
    mMainContainer->addView(mContentContainer);
    
    int yPos = kMargin;
    
    // CPU Meter Section
    mCPULabel = createLabel("CPU Usage:", VSTGUI::CRect(kMargin, yPos, 100, kComponentHeight));
    mCPULabel->setFontColor(VSTGUI::CColor(255, 255, 255, 255));
    mContentContainer->addView(mCPULabel);
    
    // CPU bar (visual indicator)
    VSTGUI::CRect cpuBarRect(120, yPos, 120 + kCPUBarWidth, yPos + kComponentHeight);
    mCPUBar = new VSTGUI::CGradientView(cpuBarRect);
    VSTGUI::CGradient* gradient = VSTGUI::CGradient::create(0.0, 1.0, 
                                                           VSTGUI::CColor(60, 60, 60, 255), 
                                                           VSTGUI::CColor(0, 255, 0, 255));
    mCPUBar->setGradient(gradient);
    gradient->forget();
    mContentContainer->addView(mCPUBar);
    
    // CPU percentage display
    VSTGUI::CRect cpuMeterRect(120 + kCPUBarWidth + 10, yPos, 120 + kCPUBarWidth + 70, yPos + kComponentHeight);
    mCPUMeter = new VSTGUI::CParamDisplay(cpuMeterRect);
    mCPUMeter->setBackColor(VSTGUI::CColor(30, 30, 30, 255));
    mCPUMeter->setFontColor(VSTGUI::CColor(255, 255, 255, 255));
    mCPUMeter->setText("0.0%");
    mContentContainer->addView(mCPUMeter);
    
    yPos += kComponentHeight + kMargin * 2;
    
    // Sensitivity Slider Section
    mSensitivityLabel = createLabel("Sensitivity:", VSTGUI::CRect(kMargin, yPos, 100, kComponentHeight));
    mSensitivityLabel->setFontColor(VSTGUI::CColor(255, 255, 255, 255));
    mContentContainer->addView(mSensitivityLabel);
    
    VSTGUI::CRect sliderRect(120, yPos, 120 + kSliderWidth, yPos + kComponentHeight);
    mSensitivitySlider = createSlider(sliderRect, mControlListener.get(), kSensitivitySliderTag);
    mSensitivitySlider->setValue(0.6f);  // Default threshold
    mContentContainer->addView(mSensitivitySlider);
    
    VSTGUI::CRect sensitivityDisplayRect(120 + kSliderWidth + 10, yPos, 120 + kSliderWidth + 70, yPos + kComponentHeight);
    mSensitivityDisplay = new VSTGUI::CParamDisplay(sensitivityDisplayRect);
    mSensitivityDisplay->setBackColor(VSTGUI::CColor(30, 30, 30, 255));
    mSensitivityDisplay->setFontColor(VSTGUI::CColor(255, 255, 255, 255));
    mSensitivityDisplay->setText("0.60");
    mContentContainer->addView(mSensitivityDisplay);
    
    yPos += kComponentHeight + kMargin * 2;
    
    // Hit Indicator Section
    mHitLabel = createLabel("Hit Status:", VSTGUI::CRect(kMargin, yPos, 100, kComponentHeight));
    mHitLabel->setFontColor(VSTGUI::CColor(255, 255, 255, 255));
    mContentContainer->addView(mHitLabel);
    
    VSTGUI::CRect hitIndicatorRect(120, yPos, 200, yPos + kComponentHeight);
    mHitIndicator = createLabel("---", hitIndicatorRect);
    mHitIndicator->setFontColor(VSTGUI::CColor(128, 128, 128, 255));
    mHitIndicator->setFont(VSTGUI::kSystemFont, 14, VSTGUI::kBoldFace);
    mContentContainer->addView(mHitIndicator);
}

void KhDetectorEditor::createFooter()
{
    VSTGUI::CRect footerRect(0, getViewSize().getHeight() - kFooterHeight, 
                            getViewSize().getWidth(), getViewSize().getHeight());
    
    mFooterContainer = new VSTGUI::CViewContainer(footerRect);
    mFooterContainer->setBackgroundColor(VSTGUI::CColor(35, 35, 35, 255));
    mMainContainer->addView(mFooterContainer);
    
    // Write Markers button
    VSTGUI::CRect writeButtonRect(kMargin, 10, kMargin + 120, 10 + 30);
    mWriteMarkersButton = createButton("Write Markers", writeButtonRect, 
                                      mControlListener.get(), kWriteMarkersButtonTag);
    mFooterContainer->addView(mWriteMarkersButton);
    
    // Size buttons (right-aligned)
    int buttonY = 10;
    int buttonWidth = kButtonWidth;
    int buttonSpacing = 5;
    int totalButtonWidth = (buttonWidth + buttonSpacing) * 3 - buttonSpacing;
    int startX = footerRect.getWidth() - totalButtonWidth - kMargin;
    
    VSTGUI::CRect smallButtonRect(startX, buttonY, startX + buttonWidth, buttonY + 30);
    mSmallSizeButton = createButton("Small", smallButtonRect, 
                                   mControlListener.get(), kSmallSizeButtonTag);
    mFooterContainer->addView(mSmallSizeButton);
    
    startX += buttonWidth + buttonSpacing;
    VSTGUI::CRect mediumButtonRect(startX, buttonY, startX + buttonWidth, buttonY + 30);
    mMediumSizeButton = createButton("Medium", mediumButtonRect, 
                                    mControlListener.get(), kMediumSizeButtonTag);
    mFooterContainer->addView(mMediumSizeButton);
    
    startX += buttonWidth + buttonSpacing;
    VSTGUI::CRect largeButtonRect(startX, buttonY, startX + buttonWidth, buttonY + 30);
    mLargeSizeButton = createButton("Large", largeButtonRect, 
                                   mControlListener.get(), kLargeSizeButtonTag);
    mFooterContainer->addView(mLargeSizeButton);
}

void KhDetectorEditor::layoutComponents()
{
    // This method handles responsive layout based on current size
    updateLayout();
}

void KhDetectorEditor::updateLayout()
{
    if (!mMainContainer) return;
    
    VSTGUI::CRect viewSize = getViewSize();
    VSTGUI::CRect containerRect = viewSize;
    containerRect.offset(-containerRect.left, -containerRect.top);
    
    mMainContainer->setViewSize(containerRect);
    
    // Update header
    if (mHeaderContainer) {
        VSTGUI::CRect headerRect(0, 0, viewSize.getWidth(), kHeaderHeight);
        mHeaderContainer->setViewSize(headerRect);
        
        if (mSizeLabel) {
            VSTGUI::CRect sizeRect(headerRect.getWidth() - 200 - kMargin, 5, 200, kHeaderHeight - 5);
            mSizeLabel->setViewSize(sizeRect);
        }
    }
    
    // Update content
    if (mContentContainer) {
        VSTGUI::CRect contentRect(0, kHeaderHeight, viewSize.getWidth(), 
                                 viewSize.getHeight() - kHeaderHeight - kFooterHeight);
        mContentContainer->setViewSize(contentRect);
    }
    
    // Update footer
    if (mFooterContainer) {
        VSTGUI::CRect footerRect(0, viewSize.getHeight() - kFooterHeight, 
                                viewSize.getWidth(), viewSize.getHeight());
        mFooterContainer->setViewSize(footerRect);
        
        // Update size buttons position
        int buttonY = 10;
        int buttonWidth = kButtonWidth;
        int buttonSpacing = 5;
        int totalButtonWidth = (buttonWidth + buttonSpacing) * 3 - buttonSpacing;
        int startX = footerRect.getWidth() - totalButtonWidth - kMargin;
        
        if (mSmallSizeButton) {
            VSTGUI::CRect rect(startX, buttonY, startX + buttonWidth, buttonY + 30);
            mSmallSizeButton->setViewSize(rect);
        }
        
        startX += buttonWidth + buttonSpacing;
        if (mMediumSizeButton) {
            VSTGUI::CRect rect(startX, buttonY, startX + buttonWidth, buttonY + 30);
            mMediumSizeButton->setViewSize(rect);
        }
        
        startX += buttonWidth + buttonSpacing;
        if (mLargeSizeButton) {
            VSTGUI::CRect rect(startX, buttonY, startX + buttonWidth, buttonY + 30);
            mLargeSizeButton->setViewSize(rect);
        }
    }
}

//==============================================================================
// Helper Methods
//==============================================================================
VSTGUI::CTextLabel* KhDetectorEditor::createLabel(const std::string& text, 
                                                  const VSTGUI::CRect& rect,
                                                  VSTGUI::CFontRef font)
{
    auto* label = new VSTGUI::CTextLabel(rect, text.c_str());
    label->setBackColor(VSTGUI::CColor(0, 0, 0, 0));  // Transparent
    label->setFrameColor(VSTGUI::CColor(0, 0, 0, 0));  // No frame
    if (font) {
        label->setFont(font);
    }
    return label;
}

VSTGUI::CSlider* KhDetectorEditor::createSlider(const VSTGUI::CRect& rect,
                                               VSTGUI::IControlListener* listener,
                                               int32_t tag)
{
    auto* slider = new VSTGUI::CSlider(rect, listener, tag, 0, rect.getWidth() - 20, nullptr, nullptr);
    slider->setBackColor(VSTGUI::CColor(60, 60, 60, 255));
    slider->setFrameColor(VSTGUI::CColor(100, 100, 100, 255));
    return slider;
}

VSTGUI::CTextButton* KhDetectorEditor::createButton(const std::string& title,
                                                   const VSTGUI::CRect& rect,
                                                   VSTGUI::IControlListener* listener,
                                                   int32_t tag)
{
    auto* button = new VSTGUI::CTextButton(rect, listener, tag, title.c_str());
    button->setTextColor(VSTGUI::CColor(255, 255, 255, 255));
    button->setGradient(VSTGUI::CGradient::create(0.0, 1.0, 
                                                 VSTGUI::CColor(80, 80, 80, 255), 
                                                 VSTGUI::CColor(60, 60, 60, 255)));
    button->setGradientHighlighted(VSTGUI::CGradient::create(0.0, 1.0, 
                                                            VSTGUI::CColor(100, 100, 100, 255), 
                                                            VSTGUI::CColor(80, 80, 80, 255)));
    return button;
}

void KhDetectorEditor::onTimer()
{
    // Update hit state from atomic
    bool currentHit = mHitState.load();
    updateHitState(currentHit);
    
    // Fade hit flash animation
    if (mHitFlashAlpha > 0.0f) {
        mHitFlashAlpha -= 0.05f;  // Fade out over ~0.66 seconds at 30fps
        if (mHitFlashAlpha < 0.0f) {
            mHitFlashAlpha = 0.0f;
        }
    }
    
    // Trigger display update
    updateDisplay();
}

//==============================================================================
// Control Event Handlers
//==============================================================================
void KhDetectorEditor::onSensitivityChanged(float value)
{
    // Update display
    if (mSensitivityDisplay) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << value;
        mSensitivityDisplay->setText(oss.str().c_str());
    }
    
    // Notify controller
    if (mController) {
        mController->setSensitivity(value);
    }
    
    std::cout << "KhDetectorEditor: Sensitivity changed to " << value << std::endl;
}

void KhDetectorEditor::onWriteMarkersClicked()
{
    // Notify controller to write markers
    if (mController) {
        mController->writeMarkers();
    }
    
    std::cout << "KhDetectorEditor: Write Markers button clicked" << std::endl;
}

} // namespace KhDetector 