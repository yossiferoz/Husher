/**
 * @file resizable_ui_demo.cpp
 * @brief Demo application for KhDetector resizable UI
 * 
 * This demo showcases the three UI sizes (Small, Medium, Large) with:
 * - CPU meter showing real-time process() timing
 * - Sensitivity slider (0-1) mapped to detection threshold
 * - Write Markers button calling controller
 * - Auto-layout system for responsive design
 */

#include "../src/KhDetectorEditor.h"
#include "../src/KhDetectorController.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/platform/platformfactory.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>

using namespace KhDetector;

/**
 * @brief Mock controller for demo purposes
 */
class MockController
{
public:
    MockController() = default;
    
    void setSensitivity(float value)
    {
        std::cout << "MockController: Sensitivity changed to " << value << std::endl;
        mSensitivity = value;
    }
    
    void writeMarkers()
    {
        std::cout << "MockController: Write Markers button clicked!" << std::endl;
        mMarkerWriteCount++;
    }
    
    float getSensitivity() const { return mSensitivity; }
    int getMarkerWriteCount() const { return mMarkerWriteCount; }
    
private:
    float mSensitivity = 0.6f;
    int mMarkerWriteCount = 0;
};

/**
 * @brief Demo application class
 */
class ResizableUIDemo
{
public:
    ResizableUIDemo()
        : mController(std::make_unique<MockController>())
        , mHitState(false)
        , mRandom(std::random_device{}())
        , mDistribution(0.0f, 1.0f)
    {
        
        std::cout << "=== KhDetector Resizable UI Demo ===" << std::endl;
        std::cout << "Features:" << std::endl;
        std::cout << "- Three UI sizes: Small (760×480), Medium (1100×680), Large (1600×960)" << std::endl;
        std::cout << "- CPU meter with real-time process() timing simulation" << std::endl;
        std::cout << "- Sensitivity slider (0-1) mapped to detection threshold" << std::endl;
        std::cout << "- Write Markers button calling controller" << std::endl;
        std::cout << "- Auto-layout system for responsive design" << std::endl;
        std::cout << std::endl;
    }
    
    ~ResizableUIDemo()
    {
        cleanup();
    }
    
    void run()
    {
        createEditor();
        startSimulation();
        
        std::cout << "Demo running... Press Enter to cycle through UI sizes, 'q' to quit." << std::endl;
        
        char input;
        while (std::cin.get(input)) {
            if (input == 'q' || input == 'Q') {
                break;
            } else if (input == '\n') {
                cycleSizes();
            }
        }
        
        stopSimulation();
    }
    
private:
    void createEditor()
    {
        // Create editor with medium size by default
        VSTGUI::CRect editorSize(0, 0, 1100, 680);
        mEditor = std::make_unique<KhDetectorEditor>(editorSize, mController.get(), mHitState);
        
        std::cout << "Created editor with Medium size (1100×680)" << std::endl;
    }
    
    void startSimulation()
    {
        mSimulationRunning = true;
        
        // Start CPU monitoring thread
        mCpuThread = std::thread([this]() {
            simulateCPUUsage();
        });
        
        // Start hit detection simulation thread
        mHitThread = std::thread([this]() {
            simulateHitDetection();
        });
        
        std::cout << "Started simulation threads" << std::endl;
    }
    
    void stopSimulation()
    {
        mSimulationRunning = false;
        
        if (mCpuThread.joinable()) {
            mCpuThread.join();
        }
        
        if (mHitThread.joinable()) {
            mHitThread.join();
        }
        
        std::cout << "Stopped simulation threads" << std::endl;
    }
    
    void cleanup()
    {
        stopSimulation();
        mEditor.reset();
    }
    
    void cycleSizes()
    {
        if (!mEditor) return;
        
        auto currentSize = mEditor->getCurrentUISize();
        KhDetectorEditor::UISize newSize;
        std::string sizeText;
        
        switch (currentSize) {
            case KhDetectorEditor::UISize::Small:
                newSize = KhDetectorEditor::UISize::Medium;
                sizeText = "Medium (1100×680)";
                break;
            case KhDetectorEditor::UISize::Medium:
                newSize = KhDetectorEditor::UISize::Large;
                sizeText = "Large (1600×960)";
                break;
            case KhDetectorEditor::UISize::Large:
                newSize = KhDetectorEditor::UISize::Small;
                sizeText = "Small (760×480)";
                break;
        }
        
        mEditor->setUISize(newSize);
        std::cout << "UI size changed to: " << sizeText << std::endl;
    }
    
    void simulateCPUUsage()
    {
        
        while (mSimulationRunning) {
            // Simulate process() call timing
            auto startTime = std::chrono::high_resolution_clock::now();
            
            // Simulate variable processing load
            float loadFactor = 0.5f + 0.5f * std::sin(mCpuSimulationTime * 0.1f);
            auto processingTime = std::chrono::microseconds(
                static_cast<int>(100 + loadFactor * 200)  // 100-300 microseconds
            );
            
            std::this_thread::sleep_for(processingTime);
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            
            // Update CPU stats in editor
            if (mEditor) {
                float processTimeMs = duration.count() / 1000.0f;
                mEditor->updateCPUStats(processTimeMs);
            }
            
            mCpuSimulationTime += 0.01f;
            
            // Simulate ~44.1kHz sample rate with 512 samples per buffer
            // = ~86 buffers per second = ~11.6ms per buffer
            std::this_thread::sleep_for(std::chrono::milliseconds(11));
        }
    }
    
    void simulateHitDetection()
    {
        while (mSimulationRunning) {
            // Generate random confidence values
            float confidence = mDistribution(mRandom);
            float threshold = mController->getSensitivity();
            
            // Simulate hit detection
            bool hit = confidence > threshold;
            
            if (hit != mHitState.load()) {
                mHitState.store(hit);
                
                if (hit) {
                    std::cout << "HIT detected! (confidence: " << confidence 
                              << ", threshold: " << threshold << ")" << std::endl;
                }
            }
            
            // Check for hits every 50ms (20 Hz)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
private:
    std::unique_ptr<KhDetectorEditor> mEditor;
    std::unique_ptr<MockController> mController;
    std::atomic<bool> mHitState;
    
    // Simulation state
    std::atomic<bool> mSimulationRunning{false};
    std::thread mCpuThread;
    std::thread mHitThread;
    float mCpuSimulationTime = 0.0f;
    
    // Random number generation
    std::mt19937 mRandom;
    std::uniform_real_distribution<float> mDistribution;
};

int main()
{
    try {
        ResizableUIDemo demo;
        demo.run();
        
        std::cout << "Demo completed successfully!" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Demo failed with exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Demo failed with unknown exception" << std::endl;
        return 1;
    }
} 