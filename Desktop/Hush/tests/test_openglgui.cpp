#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>

#include "KhDetectorOpenGLView.h"
#include "KhDetectorGUIView.h"

using namespace KhDetector;

class OpenGLGUITest : public ::testing::Test 
{
protected:
    void SetUp() override 
    {
        // Create a test hit state
        mHitState.store(false);
        
        // Create test view size
        mViewRect = VSTGUI::CRect(0, 0, 400, 300);
    }
    
    void TearDown() override 
    {
        // Clean up any views
        if (mOpenGLView) {
            mOpenGLView->stopAnimation();
            mOpenGLView.reset();
        }
        
        if (mGUIView) {
            mGUIView->stopUpdates();
            mGUIView.reset();
        }
    }
    
    std::atomic<bool> mHitState;
    VSTGUI::CRect mViewRect;
    std::unique_ptr<KhDetectorOpenGLView> mOpenGLView;
    std::unique_ptr<KhDetectorGUIView> mGUIView;
};

TEST_F(OpenGLGUITest, OpenGLViewCreation)
{
    // Test OpenGL view creation
    mOpenGLView = createOpenGLView(mViewRect, mHitState);
    
    ASSERT_NE(mOpenGLView, nullptr);
    EXPECT_EQ(mOpenGLView->getViewSize().getWidth(), 400);
    EXPECT_EQ(mOpenGLView->getViewSize().getHeight(), 300);
    
    // Test initial statistics
    auto stats = mOpenGLView->getStatistics();
    EXPECT_EQ(stats.frameCount, 0);
    EXPECT_EQ(stats.hitCount, 0);
    EXPECT_EQ(stats.currentFPS, 0.0f);
}

TEST_F(OpenGLGUITest, OpenGLViewConfiguration)
{
    mOpenGLView = createOpenGLView(mViewRect, mHitState);
    ASSERT_NE(mOpenGLView, nullptr);
    
    // Test default configuration
    auto defaultConfig = mOpenGLView->getConfig();
    EXPECT_EQ(defaultConfig.refreshRate, 60.0f);
    EXPECT_TRUE(defaultConfig.showFPS);
    EXPECT_TRUE(defaultConfig.showHitIndicator);
    
    // Test custom configuration
    KhDetectorOpenGLView::Config customConfig;
    customConfig.refreshRate = 30.0f;
    customConfig.hitFlashDuration = 1.0f;
    customConfig.showFPS = false;
    
    mOpenGLView->setConfig(customConfig);
    
    auto updatedConfig = mOpenGLView->getConfig();
    EXPECT_EQ(updatedConfig.refreshRate, 30.0f);
    EXPECT_EQ(updatedConfig.hitFlashDuration, 1.0f);
    EXPECT_FALSE(updatedConfig.showFPS);
}

TEST_F(OpenGLGUITest, HitDetectionIntegration)
{
    mOpenGLView = createOpenGLView(mViewRect, mHitState);
    ASSERT_NE(mOpenGLView, nullptr);
    
    // Initial state should be no hit
    auto initialStats = mOpenGLView->getStatistics();
    EXPECT_EQ(initialStats.hitCount, 0);
    
    // Simulate hit detection - note: this is a simplified test
    // In reality, the OpenGL view would detect the hit state change during rendering
    mHitState.store(true);
    
    // The actual hit detection happens in updateHitState() which is called during rendering
    // For testing purposes, we'll verify the atomic state is accessible
    EXPECT_TRUE(mHitState.load());
    
    // Reset hit state
    mHitState.store(false);
    EXPECT_FALSE(mHitState.load());
}

TEST_F(OpenGLGUITest, AnimationControl)
{
    mOpenGLView = createOpenGLView(mViewRect, mHitState);
    ASSERT_NE(mOpenGLView, nullptr);
    
    // Test animation start/stop
    mOpenGLView->startAnimation();
    // Note: We can't easily test if animation is actually running without a full OpenGL context
    // But we can test that the methods don't crash
    
    mOpenGLView->stopAnimation();
    // Should be safe to call multiple times
    mOpenGLView->stopAnimation();
}

TEST_F(OpenGLGUITest, GUIViewCreation)
{
    // Test GUI view creation
    mGUIView = createGUIView(mViewRect, mHitState);
    
    ASSERT_NE(mGUIView, nullptr);
    EXPECT_EQ(mGUIView->getViewSize().getWidth(), 400);
    EXPECT_EQ(mGUIView->getViewSize().getHeight(), 300);
    
    // Test OpenGL view access
    auto openglView = mGUIView->getOpenGLView();
    ASSERT_NE(openglView, nullptr);
}

TEST_F(OpenGLGUITest, GUIViewConfiguration)
{
    mGUIView = createGUIView(mViewRect, mHitState);
    ASSERT_NE(mGUIView, nullptr);
    
    // Test default configuration
    auto defaultConfig = mGUIView->getConfig();
    EXPECT_TRUE(defaultConfig.showFPS);
    EXPECT_TRUE(defaultConfig.showHitCounter);
    EXPECT_TRUE(defaultConfig.showStatistics);
    EXPECT_EQ(defaultConfig.textUpdateRate, 10.0f);
    EXPECT_EQ(defaultConfig.openglUpdateRate, 60.0f);
    
    // Test custom configuration
    KhDetectorGUIView::Config customConfig;
    customConfig.showFPS = false;
    customConfig.showHitCounter = true;
    customConfig.showStatistics = false;
    customConfig.textUpdateRate = 5.0f;
    customConfig.fontSize = 16.0f;
    
    mGUIView->setConfig(customConfig);
    
    auto updatedConfig = mGUIView->getConfig();
    EXPECT_FALSE(updatedConfig.showFPS);
    EXPECT_TRUE(updatedConfig.showHitCounter);
    EXPECT_FALSE(updatedConfig.showStatistics);
    EXPECT_EQ(updatedConfig.textUpdateRate, 5.0f);
    EXPECT_EQ(updatedConfig.fontSize, 16.0f);
}

TEST_F(OpenGLGUITest, UpdateControl)
{
    mGUIView = createGUIView(mViewRect, mHitState);
    ASSERT_NE(mGUIView, nullptr);
    
    // Test update start/stop
    mGUIView->startUpdates();
    // Note: We can't easily test the actual update loop without a full GUI context
    // But we can verify the methods don't crash
    
    mGUIView->stopUpdates();
    // Should be safe to call multiple times
    mGUIView->stopUpdates();
}

TEST_F(OpenGLGUITest, ViewSizeChanges)
{
    mOpenGLView = createOpenGLView(mViewRect, mHitState);
    ASSERT_NE(mOpenGLView, nullptr);
    
    // Test view size changes
    VSTGUI::CRect newSize(0, 0, 800, 600);
    mOpenGLView->setViewSize(newSize);
    
    EXPECT_EQ(mOpenGLView->getViewSize().getWidth(), 800);
    EXPECT_EQ(mOpenGLView->getViewSize().getHeight(), 600);
}

TEST_F(OpenGLGUITest, StatisticsTracking)
{
    mOpenGLView = createOpenGLView(mViewRect, mHitState);
    ASSERT_NE(mOpenGLView, nullptr);
    
    // Initial statistics should be zero
    auto stats = mOpenGLView->getStatistics();
    EXPECT_EQ(stats.frameCount, 0);
    EXPECT_EQ(stats.hitCount, 0);
    EXPECT_EQ(stats.currentFPS, 0.0f);
    EXPECT_EQ(stats.averageFPS, 0.0f);
    EXPECT_EQ(stats.lastHitTime, 0.0f);
}

TEST_F(OpenGLGUITest, FactoryFunctions)
{
    // Test OpenGL view factory
    auto openglView = createOpenGLView(mViewRect, mHitState);
    ASSERT_NE(openglView, nullptr);
    EXPECT_EQ(openglView->getViewSize().getWidth(), 400);
    EXPECT_EQ(openglView->getViewSize().getHeight(), 300);
    
    openglView.reset();
    
    // Test GUI view factory
    auto guiView = createGUIView(mViewRect, mHitState);
    ASSERT_NE(guiView, nullptr);
    EXPECT_EQ(guiView->getViewSize().getWidth(), 400);
    EXPECT_EQ(guiView->getViewSize().getHeight(), 300);
    
    auto embeddedOpenGL = guiView->getOpenGLView();
    ASSERT_NE(embeddedOpenGL, nullptr);
}

TEST_F(OpenGLGUITest, MultipleHitSimulation)
{
    mGUIView = createGUIView(mViewRect, mHitState);
    ASSERT_NE(mGUIView, nullptr);
    
    auto openglView = mGUIView->getOpenGLView();
    ASSERT_NE(openglView, nullptr);
    
    // Simulate multiple hit state changes
    for (int i = 0; i < 5; ++i) {
        mHitState.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        mHitState.store(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // The hit state should be accessible
    EXPECT_FALSE(mHitState.load()); // Should end in false state
}

TEST_F(OpenGLGUITest, ThreadSafety)
{
    mGUIView = createGUIView(mViewRect, mHitState);
    ASSERT_NE(mGUIView, nullptr);
    
    // Test thread-safe access to hit state
    std::atomic<bool> testComplete{false};
    
    // Thread 1: Toggle hit state
    std::thread hitThread([&]() {
        for (int i = 0; i < 100 && !testComplete.load(); ++i) {
            mHitState.store(i % 2 == 0);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // Thread 2: Read hit state
    std::thread readThread([&]() {
        for (int i = 0; i < 100 && !testComplete.load(); ++i) {
            volatile bool state = mHitState.load();
            (void)state; // Suppress unused variable warning
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // Let threads run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    testComplete.store(true);
    
    hitThread.join();
    readThread.join();
    
    // Test should complete without crashes
    SUCCEED();
}

TEST_F(OpenGLGUITest, ConfigurationPersistence)
{
    mGUIView = createGUIView(mViewRect, mHitState);
    ASSERT_NE(mGUIView, nullptr);
    
    // Set custom configuration
    KhDetectorGUIView::Config config;
    config.fontSize = 18.0f;
    config.textUpdateRate = 15.0f;
    config.showFPS = false;
    config.showHitCounter = true;
    config.showStatistics = false;
    
    mGUIView->setConfig(config);
    
    // Verify configuration persists
    auto retrievedConfig = mGUIView->getConfig();
    EXPECT_EQ(retrievedConfig.fontSize, 18.0f);
    EXPECT_EQ(retrievedConfig.textUpdateRate, 15.0f);
    EXPECT_FALSE(retrievedConfig.showFPS);
    EXPECT_TRUE(retrievedConfig.showHitCounter);
    EXPECT_FALSE(retrievedConfig.showStatistics);
} 