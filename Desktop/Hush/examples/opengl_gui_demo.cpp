#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>

#include "../src/KhDetectorGUIView.h"

using namespace KhDetector;

/**
 * @brief Demonstration of OpenGL GUI with hit detection visualization
 * 
 * This demo simulates hit detection events and shows how the OpenGL GUI
 * responds with real-time FPS counter and hit flash effects.
 */

class OpenGLGUIDemo
{
public:
    void runDemo()
    {
        std::cout << "=== OpenGL GUI Demonstration ===" << std::endl;
        std::cout << std::endl;
        
        demoBasicGUICreation();
        std::cout << std::endl;
        
        demoHitDetectionVisualization();
        std::cout << std::endl;
        
        demoConfigurationOptions();
        std::cout << std::endl;
        
        demoPerformanceMonitoring();
        std::cout << std::endl;
        
        std::cout << "=== OpenGL GUI Demo Complete ===" << std::endl;
    }

private:
    void demoBasicGUICreation()
    {
        std::cout << "--- Basic GUI Creation ---" << std::endl;
        
        // Create atomic hit state for GUI monitoring
        std::atomic<bool> hitState{false};
        
        // Create GUI view
        VSTGUI::CRect viewSize(0, 0, 600, 400);
        auto guiView = createGUIView(viewSize, hitState);
        
        if (guiView) {
            std::cout << "✓ GUI view created successfully" << std::endl;
            std::cout << "  Size: " << viewSize.getWidth() << "x" << viewSize.getHeight() << std::endl;
            
            // Test OpenGL view access
            auto openglView = guiView->getOpenGLView();
            if (openglView) {
                std::cout << "✓ OpenGL view accessible" << std::endl;
                
                auto stats = openglView->getStatistics();
                std::cout << "  Initial stats:" << std::endl;
                std::cout << "    Frame count: " << stats.frameCount << std::endl;
                std::cout << "    Hit count: " << stats.hitCount << std::endl;
                std::cout << "    Current FPS: " << stats.currentFPS << std::endl;
            }
            
            // Test configuration
            auto config = guiView->getConfig();
            std::cout << "  Configuration:" << std::endl;
            std::cout << "    Text update rate: " << config.textUpdateRate << " Hz" << std::endl;
            std::cout << "    OpenGL update rate: " << config.openglUpdateRate << " Hz" << std::endl;
            std::cout << "    Show FPS: " << (config.showFPS ? "Yes" : "No") << std::endl;
            std::cout << "    Show hit counter: " << (config.showHitCounter ? "Yes" : "No") << std::endl;
        } else {
            std::cerr << "✗ Failed to create GUI view" << std::endl;
        }
    }
    
    void demoHitDetectionVisualization()
    {
        std::cout << "--- Hit Detection Visualization ---" << std::endl;
        
        std::atomic<bool> hitState{false};
        VSTGUI::CRect viewSize(0, 0, 600, 400);
        auto guiView = createGUIView(viewSize, hitState);
        
        if (!guiView) {
            std::cerr << "Failed to create GUI view for hit detection demo" << std::endl;
            return;
        }
        
        auto openglView = guiView->getOpenGLView();
        if (!openglView) {
            std::cerr << "Failed to access OpenGL view" << std::endl;
            return;
        }
        
        std::cout << "Simulating hit detection events..." << std::endl;
        
        // Simulate various hit patterns
        std::vector<std::pair<std::string, std::vector<bool>>> patterns = {
            {"Single hit", {true, false, false, false}},
            {"Double hit", {true, false, true, false}},
            {"Sustained hit", {true, true, true, false}},
            {"Rapid hits", {true, false, true, false, true, false}}
        };
        
        for (const auto& pattern : patterns) {
            std::cout << "\nTesting pattern: " << pattern.first << std::endl;
            
            auto initialStats = openglView->getStatistics();
            
            for (size_t i = 0; i < pattern.second.size(); ++i) {
                hitState.store(pattern.second[i]);
                std::cout << "  Frame " << i << ": " << (pattern.second[i] ? "HIT" : "---") << std::endl;
                
                // Simulate frame processing time
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Reset to no hit
            hitState.store(false);
            
            auto finalStats = openglView->getStatistics();
            std::cout << "  Stats change: " 
                      << (finalStats.hitCount - initialStats.hitCount) << " hits detected" << std::endl;
        }
    }
    
    void demoConfigurationOptions()
    {
        std::cout << "--- Configuration Options ---" << std::endl;
        
        std::atomic<bool> hitState{false};
        VSTGUI::CRect viewSize(0, 0, 600, 400);
        auto guiView = createGUIView(viewSize, hitState);
        
        if (!guiView) {
            std::cerr << "Failed to create GUI view for configuration demo" << std::endl;
            return;
        }
        
        // Test different configurations
        struct ConfigTest {
            std::string name;
            KhDetectorGUIView::Config config;
        };
        
        std::vector<ConfigTest> configTests = {
            {
                "High performance",
                {
                    .textUpdateRate = 30.0f,
                    .openglUpdateRate = 120.0f,
                    .showFPS = true,
                    .showHitCounter = true,
                    .showStatistics = true,
                    .fontSize = 12.0f
                }
            },
            {
                "Low performance",
                {
                    .textUpdateRate = 5.0f,
                    .openglUpdateRate = 30.0f,
                    .showFPS = true,
                    .showHitCounter = false,
                    .showStatistics = false,
                    .fontSize = 10.0f
                }
            },
            {
                "Minimal display",
                {
                    .textUpdateRate = 10.0f,
                    .openglUpdateRate = 60.0f,
                    .showFPS = false,
                    .showHitCounter = true,
                    .showStatistics = false,
                    .fontSize = 14.0f
                }
            }
        };
        
        for (const auto& test : configTests) {
            std::cout << "\nTesting configuration: " << test.name << std::endl;
            
            guiView->setConfig(test.config);
            auto appliedConfig = guiView->getConfig();
            
            std::cout << "  Text update rate: " << appliedConfig.textUpdateRate << " Hz" << std::endl;
            std::cout << "  OpenGL update rate: " << appliedConfig.openglUpdateRate << " Hz" << std::endl;
            std::cout << "  Show FPS: " << (appliedConfig.showFPS ? "Yes" : "No") << std::endl;
            std::cout << "  Show hit counter: " << (appliedConfig.showHitCounter ? "Yes" : "No") << std::endl;
            std::cout << "  Show statistics: " << (appliedConfig.showStatistics ? "Yes" : "No") << std::endl;
            std::cout << "  Font size: " << appliedConfig.fontSize << "pt" << std::endl;
            
            // Test configuration persistence
            auto openglConfig = guiView->getOpenGLView()->getConfig();
            std::cout << "  OpenGL refresh rate: " << openglConfig.refreshRate << " Hz" << std::endl;
        }
    }
    
    void demoPerformanceMonitoring()
    {
        std::cout << "--- Performance Monitoring ---" << std::endl;
        
        std::atomic<bool> hitState{false};
        VSTGUI::CRect viewSize(0, 0, 600, 400);
        auto guiView = createGUIView(viewSize, hitState);
        
        if (!guiView) {
            std::cerr << "Failed to create GUI view for performance demo" << std::endl;
            return;
        }
        
        auto openglView = guiView->getOpenGLView();
        if (!openglView) {
            std::cerr << "Failed to access OpenGL view" << std::endl;
            return;
        }
        
        std::cout << "Running performance monitoring simulation..." << std::endl;
        
        // Start updates (simulating GUI activation)
        guiView->startUpdates();
        
        // Simulate random hit detection with performance monitoring
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> hitProb(0.0, 1.0);
        std::uniform_int_distribution<> frameDelay(16, 33); // 30-60 FPS simulation
        
        auto startTime = std::chrono::steady_clock::now();
        int frameCount = 0;
        int hitCount = 0;
        
        std::cout << "\nSimulating real-time processing..." << std::endl;
        std::cout << "Frame | Hit | FPS  | Total Hits | Frame Count" << std::endl;
        std::cout << "------|-----|------|------------|------------" << std::endl;
        
        for (int i = 0; i < 50; ++i) {
            // Random hit detection
            bool hit = hitProb(gen) < 0.1; // 10% hit probability
            hitState.store(hit);
            
            if (hit) {
                hitCount++;
            }
            
            // Simulate frame processing
            std::this_thread::sleep_for(std::chrono::milliseconds(frameDelay(gen)));
            
            frameCount++;
            
            // Get current statistics
            auto stats = openglView->getStatistics();
            
            // Print status every 10 frames
            if (i % 10 == 0) {
                std::cout << std::setw(5) << i << " | "
                          << std::setw(3) << (hit ? "HIT" : "---") << " | "
                          << std::fixed << std::setprecision(1) << std::setw(4) << stats.currentFPS << " | "
                          << std::setw(10) << stats.hitCount << " | "
                          << std::setw(11) << stats.frameCount << std::endl;
            }
        }
        
        // Final statistics
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        auto finalStats = openglView->getStatistics();
        
        std::cout << "\nPerformance Summary:" << std::endl;
        std::cout << "  Simulation duration: " << duration.count() << " ms" << std::endl;
        std::cout << "  Simulated frames: " << frameCount << std::endl;
        std::cout << "  Simulated hits: " << hitCount << std::endl;
        std::cout << "  GUI frame count: " << finalStats.frameCount << std::endl;
        std::cout << "  GUI hit count: " << finalStats.hitCount << std::endl;
        std::cout << "  Current FPS: " << std::fixed << std::setprecision(1) << finalStats.currentFPS << std::endl;
        std::cout << "  Average FPS: " << std::fixed << std::setprecision(1) << finalStats.averageFPS << std::endl;
        
        if (finalStats.lastHitTime > 0.0f) {
            std::cout << "  Last hit time: " << std::fixed << std::setprecision(2) << finalStats.lastHitTime << "s" << std::endl;
        }
        
        // Stop updates
        guiView->stopUpdates();
        
        std::cout << "✓ Performance monitoring complete" << std::endl;
    }
};

int main()
{
    try {
        OpenGLGUIDemo demo;
        demo.runDemo();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "OpenGL GUI demo failed with exception: " << e.what() << std::endl;
        return 1;
    }
} 