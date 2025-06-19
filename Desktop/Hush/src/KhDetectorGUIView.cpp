#include "KhDetectorGUIView.h"
#include "vstgui/lib/cstring.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace KhDetector {

KhDetectorGUIView::KhDetectorGUIView(const VSTGUI::CRect& size, std::atomic<bool>& hitState)
    : CViewContainer(size)
    , mHitState(hitState)
{
    // Set transparent background
    setBackgroundColor(VSTGUI::CColor(0, 0, 0, 0));
    
    // Create font
    mFont = VSTGUI::makeOwned<VSTGUI::CFontDesc>(mConfig.fontName.c_str(), mConfig.fontSize);
    
    // Create child views
    createChildViews();
    
    std::cout << "KhDetectorGUIView: Created with size " 
              << size.getWidth() << "x" << size.getHeight() << std::endl;
}

KhDetectorGUIView::~KhDetectorGUIView()
{
    stopUpdates();
    
    std::cout << "KhDetectorGUIView: Destroyed" << std::endl;
}

void KhDetectorGUIView::setViewSize(const VSTGUI::CRect& rect, bool invalid)
{
    CViewContainer::setViewSize(rect, invalid);
    layoutChildViews();
    
    std::cout << "KhDetectorGUIView: Size changed to " 
              << rect.getWidth() << "x" << rect.getHeight() << std::endl;
}

void KhDetectorGUIView::onTimer()
{
    updateTextDisplays();
}

void KhDetectorGUIView::startUpdates()
{
    if (!mUpdatesActive) {
        mUpdatesActive = true;
        
        // Start text update timer
        uint32_t intervalMs = static_cast<uint32_t>(1000.0f / mConfig.textUpdateRate);
        mUpdateTimer = VSTGUI::makeOwned<VSTGUI::CVSTGUITimer>([this](VSTGUI::CVSTGUITimer*) {
            onTimer();
        }, intervalMs, true);
        
        // Start OpenGL animation
        if (mOpenGLView) {
            mOpenGLView->startAnimation();
        }
        
        std::cout << "KhDetectorGUIView: Updates started (text: " 
                  << mConfig.textUpdateRate << " Hz, OpenGL: " 
                  << mConfig.openglUpdateRate << " Hz)" << std::endl;
    }
}

void KhDetectorGUIView::stopUpdates()
{
    if (mUpdatesActive) {
        mUpdatesActive = false;
        
        // Stop text update timer
        if (mUpdateTimer) {
            mUpdateTimer->stop();
            mUpdateTimer = nullptr;
        }
        
        // Stop OpenGL animation
        if (mOpenGLView) {
            mOpenGLView->stopAnimation();
        }
        
        std::cout << "KhDetectorGUIView: Updates stopped" << std::endl;
    }
}

void KhDetectorGUIView::setConfig(const Config& config)
{
    mConfig = config;
    
    // Update font
    mFont = VSTGUI::makeOwned<VSTGUI::CFontDesc>(mConfig.fontName.c_str(), mConfig.fontSize);
    
    // Update text label styling
    if (mFPSLabel) {
        mFPSLabel->setFontColor(mConfig.textColor);
        mFPSLabel->setFont(mFont);
    }
    if (mHitCounterLabel) {
        mHitCounterLabel->setFontColor(mConfig.textColor);
        mHitCounterLabel->setFont(mFont);
    }
    if (mStatisticsLabel) {
        mStatisticsLabel->setFontColor(mConfig.textColor);
        mStatisticsLabel->setFont(mFont);
    }
    
    // Update OpenGL view configuration
    if (mOpenGLView) {
        KhDetectorOpenGLView::Config glConfig = mOpenGLView->getConfig();
        glConfig.refreshRate = mConfig.openglUpdateRate;
        glConfig.showFPS = mConfig.showFPS;
        glConfig.textColor = mConfig.textColor;
        mOpenGLView->setConfig(glConfig);
    }
    
    // Restart updates with new rates if active
    if (mUpdatesActive) {
        stopUpdates();
        startUpdates();
    }
    
    // Re-layout views
    layoutChildViews();
    
    std::cout << "KhDetectorGUIView: Configuration updated" << std::endl;
}

void KhDetectorGUIView::createChildViews()
{
    auto viewSize = getViewSize();
    
    // Create OpenGL view (full size background)
    mOpenGLView = createOpenGLView(VSTGUI::CRect(0, 0, viewSize.getWidth(), viewSize.getHeight()), mHitState);
    addView(mOpenGLView.get());
    
    // Create text labels
    VSTGUI::CRect labelRect(0, 0, 200, mConfig.lineHeight);
    
    if (mConfig.showFPS) {
        mFPSLabel = createStyledLabel(labelRect, "FPS: --");
        addView(mFPSLabel);
    }
    
    if (mConfig.showHitCounter) {
        mHitCounterLabel = createStyledLabel(labelRect, "Hits: 0");
        addView(mHitCounterLabel);
    }
    
    if (mConfig.showStatistics) {
        mStatisticsLabel = createStyledLabel(labelRect, "Stats: --");
        addView(mStatisticsLabel);
    }
    
    // Layout the views
    layoutChildViews();
    
    std::cout << "KhDetectorGUIView: Child views created" << std::endl;
}

void KhDetectorGUIView::updateTextDisplays()
{
    updateFPSDisplay();
    updateHitCounterDisplay();
    updateStatisticsDisplay();
}

void KhDetectorGUIView::updateFPSDisplay()
{
    if (!mFPSLabel || !mConfig.showFPS || !mOpenGLView) {
        return;
    }
    
    auto stats = mOpenGLView->getStatistics();
    
    std::ostringstream oss;
    oss << "FPS: " << std::fixed << std::setprecision(1) << stats.currentFPS 
        << " (avg: " << std::setprecision(1) << stats.averageFPS << ")";
    
    mFPSLabel->setText(oss.str().c_str());
}

void KhDetectorGUIView::updateHitCounterDisplay()
{
    if (!mHitCounterLabel || !mConfig.showHitCounter || !mOpenGLView) {
        return;
    }
    
    auto stats = mOpenGLView->getStatistics();
    
    std::ostringstream oss;
    oss << "Hits: " << stats.hitCount;
    
    if (stats.lastHitTime > 0.0f) {
        oss << " (last: " << std::fixed << std::setprecision(1) << stats.lastHitTime << "s)";
    }
    
    mHitCounterLabel->setText(oss.str().c_str());
}

void KhDetectorGUIView::updateStatisticsDisplay()
{
    if (!mStatisticsLabel || !mConfig.showStatistics || !mOpenGLView) {
        return;
    }
    
    auto stats = mOpenGLView->getStatistics();
    
    std::ostringstream oss;
    oss << "Frames: " << stats.frameCount;
    
    // Add hit state indicator
    bool currentHit = mHitState.load();
    oss << (currentHit ? " [HIT]" : " [---]");
    
    mStatisticsLabel->setText(oss.str().c_str());
}

void KhDetectorGUIView::layoutChildViews()
{
    auto viewSize = getViewSize();
    
    // OpenGL view fills the entire background
    if (mOpenGLView) {
        mOpenGLView->setViewSize(VSTGUI::CRect(0, 0, viewSize.getWidth(), viewSize.getHeight()));
    }
    
    // Position text labels in top-left corner
    float x = mConfig.margin;
    float y = mConfig.margin;
    float labelWidth = 250;
    float labelHeight = mConfig.lineHeight;
    
    if (mFPSLabel && mConfig.showFPS) {
        mFPSLabel->setViewSize(VSTGUI::CRect(x, y, x + labelWidth, y + labelHeight));
        y += labelHeight + 2;
    }
    
    if (mHitCounterLabel && mConfig.showHitCounter) {
        mHitCounterLabel->setViewSize(VSTGUI::CRect(x, y, x + labelWidth, y + labelHeight));
        y += labelHeight + 2;
    }
    
    if (mStatisticsLabel && mConfig.showStatistics) {
        mStatisticsLabel->setViewSize(VSTGUI::CRect(x, y, x + labelWidth, y + labelHeight));
        y += labelHeight + 2;
    }
}

VSTGUI::CTextLabel* KhDetectorGUIView::createStyledLabel(const VSTGUI::CRect& rect, const std::string& text)
{
    auto label = new VSTGUI::CTextLabel(rect);
    
    // Set text properties
    label->setText(text.c_str());
    label->setFont(mFont);
    label->setFontColor(mConfig.textColor);
    label->setBackColor(mConfig.backgroundColor);
    label->setTransparency(true);
    label->setHoriAlign(VSTGUI::CHoriTxtAlign::kLeftText);
    label->setStyle(VSTGUI::CTextLabel::kNoFrame);
    
    return label;
}

// Factory function
std::unique_ptr<KhDetectorGUIView> createGUIView(
    const VSTGUI::CRect& size,
    std::atomic<bool>& hitState)
{
    return std::make_unique<KhDetectorGUIView>(size, hitState);
}

} // namespace KhDetector 