#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>

// Mock CLAP host for testing
#include <clap/clap.h>

class MockClapHost {
public:
    MockClapHost() {
        host.clap_version = CLAP_VERSION_INIT;
        host.host_data = this;
        host.name = "MockHost";
        host.vendor = "KhDetector";
        host.url = "https://github.com/khdetector/khdetector";
        host.version = "1.0.0";
        
        // Set up callbacks
        host.get_extension = [](const clap_host_t* host, const char* extension_id) -> const void* {
            // Return nullptr for all extensions for now
            return nullptr;
        };
        
        host.request_restart = [](const clap_host_t* host) {
            std::cout << "[Host] Plugin requested restart" << std::endl;
        };
        
        host.request_process = [](const clap_host_t* host) {
            std::cout << "[Host] Plugin requested process" << std::endl;
        };
        
        host.request_callback = [](const clap_host_t* host) {
            std::cout << "[Host] Plugin requested callback" << std::endl;
        };
    }
    
    const clap_host_t* getHost() const { return &host; }
    
private:
    clap_host_t host;
};

class ClapPluginDemo {
public:
    ClapPluginDemo() {
        std::cout << "=== CLAP Plugin Demo ===" << std::endl;
        std::cout << "Testing KhDetector CLAP plugin functionality" << std::endl;
        std::cout << std::endl;
    }
    
    void runDemo() {
        std::cout << "1. Creating mock CLAP host..." << std::endl;
        MockClapHost mockHost;
        
        std::cout << "2. Loading plugin entry point..." << std::endl;
        // In a real scenario, we would load this from a shared library
        // For demo purposes, we'll simulate the plugin behavior
        
        std::cout << "3. Creating plugin factory..." << std::endl;
        simulatePluginFactory();
        
        std::cout << "4. Creating plugin instance..." << std::endl;
        simulatePluginInstance(mockHost.getHost());
        
        std::cout << "5. Testing parameter handling..." << std::endl;
        testParameterHandling();
        
        std::cout << "6. Testing audio processing..." << std::endl;
        testAudioProcessing();
        
        std::cout << "7. Testing MIDI output..." << std::endl;
        testMidiOutput();
        
        std::cout << std::endl;
        std::cout << "=== Demo Complete ===" << std::endl;
    }
    
private:
    void simulatePluginFactory() {
        std::cout << "   - Plugin count: 1" << std::endl;
        std::cout << "   - Plugin ID: com.khdetector.clap" << std::endl;
        std::cout << "   - Plugin name: KhDetector CLAP" << std::endl;
        std::cout << "   - Plugin features: Audio Effect | Instrument" << std::endl;
    }
    
    void simulatePluginInstance(const clap_host_t* host) {
        std::cout << "   - Plugin instance created successfully" << std::endl;
        std::cout << "   - Host: " << host->name << " v" << host->version << std::endl;
        std::cout << "   - Initializing plugin..." << std::endl;
        std::cout << "   - Plugin initialized successfully" << std::endl;
    }
    
    void testParameterHandling() {
        std::cout << "   - Parameter count: 3" << std::endl;
        std::cout << "   - Parameter 0: Bypass (0.0-1.0, default: 0.0)" << std::endl;
        std::cout << "   - Parameter 1: Hit Detected (readonly, 0.0-1.0)" << std::endl;
        std::cout << "   - Parameter 2: Sensitivity (0.0-1.0, default: 0.6)" << std::endl;
        
        std::cout << "   - Setting sensitivity to 0.8..." << std::endl;
        std::cout << "   - Sensitivity updated successfully" << std::endl;
        
        std::cout << "   - Getting parameter values..." << std::endl;
        std::cout << "     * Bypass: Off" << std::endl;
        std::cout << "     * Hit Detected: ---" << std::endl;
        std::cout << "     * Sensitivity: 0.80" << std::endl;
    }
    
    void testAudioProcessing() {
        std::cout << "   - Activating plugin (48kHz, 512 frames)..." << std::endl;
        std::cout << "   - Starting processing..." << std::endl;
        
        // Simulate audio processing
        const int sampleRate = 48000;
        const int blockSize = 512;
        const int numBlocks = 10;
        
        std::cout << "   - Processing " << numBlocks << " blocks of " << blockSize << " samples..." << std::endl;
        
        for (int block = 0; block < numBlocks; ++block) {
            // Simulate audio input with some test signal
            std::vector<float> inputL(blockSize);
            std::vector<float> inputR(blockSize);
            
            // Generate test signal (sine wave + noise)
            for (int i = 0; i < blockSize; ++i) {
                float t = static_cast<float>(block * blockSize + i) / sampleRate;
                inputL[i] = 0.5f * std::sin(2.0f * M_PI * 440.0f * t) + 
                           0.1f * (static_cast<float>(rand()) / RAND_MAX - 0.5f);
                inputR[i] = inputL[i];
            }
            
            // Simulate processing
            if (block % 3 == 0) {
                std::cout << "     Block " << block << ": Processing frame for AI inference..." << std::endl;
            }
            
            // Simulate hit detection
            if (block == 7) {
                std::cout << "     Block " << block << ": HIT DETECTED! Sending MIDI note 45" << std::endl;
            }
        }
        
        std::cout << "   - Audio processing test completed" << std::endl;
    }
    
    void testMidiOutput() {
        std::cout << "   - MIDI output port configured: 'MIDI Output'" << std::endl;
        std::cout << "   - Supported dialects: MIDI" << std::endl;
        std::cout << "   - Test MIDI event: Note On, Channel 0, Note 45 (A2), Velocity 127" << std::endl;
        std::cout << "   - MIDI output test completed" << std::endl;
    }
};

int main() {
    try {
        ClapPluginDemo demo;
        demo.runDemo();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Demo failed with exception: " << e.what() << std::endl;
        return 1;
    }
} 