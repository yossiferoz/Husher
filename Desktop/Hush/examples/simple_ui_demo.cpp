/**
 * @file simple_ui_demo.cpp
 * @brief Simplified demo for KhDetector resizable UI
 * 
 * This demo showcases the three UI sizes (Small, Medium, Large) with:
 * - CPU meter showing simulated process() timing
 * - Sensitivity slider (0-1) 
 * - Write Markers button
 * - Auto-layout system for responsive design
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>

/**
 * @brief Simple mock controller for demo purposes
 */
class SimpleController
{
public:
    void setSensitivity(float value)
    {
        std::cout << "Controller: Sensitivity changed to " << value << std::endl;
        mSensitivity = value;
    }
    
    void writeMarkers()
    {
        std::cout << "Controller: Write Markers button clicked!" << std::endl;
        mMarkerWriteCount++;
    }
    
    float getSensitivity() const { return mSensitivity; }
    int getMarkerWriteCount() const { return mMarkerWriteCount; }
    
private:
    float mSensitivity = 0.6f;
    int mMarkerWriteCount = 0;
};

/**
 * @brief Simplified UI demo without VSTGUI dependencies
 */
class SimpleUIDemo
{
public:
    SimpleUIDemo()
        : mController(std::make_unique<SimpleController>())
        , mHitState(false)
        , mRandom(std::random_device{}())
        , mDistribution(0.0f, 1.0f)
        , mCurrentSize(UISize::Medium)
    {
        std::cout << "=== KhDetector Resizable UI Demo (Simplified) ===" << std::endl;
        std::cout << "Features:" << std::endl;
        std::cout << "- Three UI sizes: Small (760×480), Medium (1100×680), Large (1600×960)" << std::endl;
        std::cout << "- CPU meter with real-time process() timing simulation" << std::endl;
        std::cout << "- Sensitivity slider (0-1) mapped to detection threshold" << std::endl;
        std::cout << "- Write Markers button calling controller" << std::endl;
        std::cout << "- Auto-layout system for responsive design" << std::endl;
        std::cout << std::endl;
    }
    
    void run()
    {
        initializeUI();
        startSimulation();
        
        std::cout << "Demo running... Commands:" << std::endl;
        std::cout << "  s - Change UI size (Small -> Medium -> Large -> Small)" << std::endl;
        std::cout << "  + - Increase sensitivity" << std::endl;
        std::cout << "  - - Decrease sensitivity" << std::endl;
        std::cout << "  m - Write markers" << std::endl;
        std::cout << "  q - Quit" << std::endl;
        std::cout << std::endl;
        
        char input;
        while (std::cin.get(input)) {
            switch (input) {
                case 'q':
                case 'Q':
                    goto exit_loop;
                case 's':
                case 'S':
                    cycleSizes();
                    break;
                case '+':
                    adjustSensitivity(0.1f);
                    break;
                case '-':
                    adjustSensitivity(-0.1f);
                    break;
                case 'm':
                case 'M':
                    mController->writeMarkers();
                    break;
                case '\n':
                    // Ignore newlines
                    break;
                default:
                    std::cout << "Unknown command: " << input << std::endl;
                    break;
            }
        }
        
    exit_loop:
        stopSimulation();
    }
    
private:
    enum class UISize {
        Small,   // 760×480
        Medium,  // 1100×680
        Large    // 1600×960
    };
    
    void initializeUI()
    {
        std::cout << "Initialized UI with Medium size (1100×680)" << std::endl;
        printCurrentState();
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
    
    void cycleSizes()
    {
        std::string sizeText;
        
        switch (mCurrentSize) {
            case UISize::Small:
                mCurrentSize = UISize::Medium;
                sizeText = "Medium (1100×680)";
                break;
            case UISize::Medium:
                mCurrentSize = UISize::Large;
                sizeText = "Large (1600×960)";
                break;
            case UISize::Large:
                mCurrentSize = UISize::Small;
                sizeText = "Small (760×480)";
                break;
        }
        
        std::cout << "UI size changed to: " << sizeText << std::endl;
        printCurrentState();
    }
    
    void adjustSensitivity(float delta)
    {
        float newValue = std::max(0.0f, std::min(1.0f, mController->getSensitivity() + delta));
        mController->setSensitivity(newValue);
        printCurrentState();
    }
    
    void printCurrentState()
    {
        std::string sizeText;
        switch (mCurrentSize) {
            case UISize::Small: sizeText = "Small (760×480)"; break;
            case UISize::Medium: sizeText = "Medium (1100×680)"; break;
            case UISize::Large: sizeText = "Large (1600×960)"; break;
        }
        
        std::cout << "Current State:" << std::endl;
        std::cout << "  UI Size: " << sizeText << std::endl;
        std::cout << "  Sensitivity: " << mController->getSensitivity() << std::endl;
        std::cout << "  CPU Usage: " << mCpuUsagePercent.load() << "%" << std::endl;
        std::cout << "  Hit State: " << (mHitState.load() ? "HIT!" : "---") << std::endl;
        std::cout << "  Markers Written: " << mController->getMarkerWriteCount() << std::endl;
        std::cout << std::endl;
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
            
            // Calculate CPU usage percentage
            float processTimeMs = duration.count() / 1000.0f;
            float bufferTimeMs = 11.6f;  // Approximate buffer time for 44.1kHz/512 samples
            float cpuPercent = (processTimeMs / bufferTimeMs) * 100.0f;
            
            // Update with exponential moving average
            float currentCpu = mCpuUsagePercent.load();
            float newCpu = currentCpu * 0.95f + cpuPercent * 0.05f;
            mCpuUsagePercent.store(std::min(newCpu, 100.0f));
            
            mCpuSimulationTime += 0.01f;
            
            // Simulate ~44.1kHz sample rate with 512 samples per buffer
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
    std::unique_ptr<SimpleController> mController;
    std::atomic<bool> mHitState;
    UISize mCurrentSize;
    
    // Simulation state
    std::atomic<bool> mSimulationRunning{false};
    std::atomic<float> mCpuUsagePercent{0.0f};
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
        SimpleUIDemo demo;
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