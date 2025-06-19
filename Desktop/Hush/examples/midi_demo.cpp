#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>

#include "../src/MidiEventHandler.h"

using namespace KhDetector;

/**
 * @brief Demonstration of MIDI event handling for hit detection
 * 
 * This demo shows:
 * - MIDI note on/off generation for hit detection
 * - Different configuration options
 * - Host timestamp handling
 * - Real-time MIDI event processing simulation
 */

class MidiDemo
{
public:
    void runAllDemos()
    {
        std::cout << "=== MIDI Event Handler Demonstration ===" << std::endl;
        std::cout << std::endl;
        
        demoBasicMidiGeneration();
        std::cout << std::endl;
        
        demoTimingAndDelays();
        std::cout << std::endl;
        
        demoConfigurationOptions();
        std::cout << std::endl;
        
        demoRealtimeSimulation();
        std::cout << std::endl;
        
        demoMidiChannelsAndNotes();
        std::cout << std::endl;
        
        demoStatisticsAndMonitoring();
        std::cout << std::endl;
        
        std::cout << "=== MIDI Demo Complete ===" << std::endl;
    }

private:
    void demoBasicMidiGeneration()
    {
        std::cout << "--- Basic MIDI Note Generation ---" << std::endl;
        
        // Create handler with default settings (Note 45 = A2, Velocity 127)
        auto handler = createMidiEventHandler();
        
        uint64_t timestamp = getCurrentTimestamp();
        
        // Simulate hit detection state changes
        std::cout << "Simulating hit detection events:" << std::endl;
        
        // No hit initially
        auto* event = handler->processHitState(false, 0, timestamp);
        std::cout << "Initial state (no hit): " << (event ? "Event generated" : "No event") << std::endl;
        
        // Hit starts
        event = handler->processHitState(true, 100, timestamp + 1000);
        if (event) {
            std::cout << "Hit starts: Generated " << eventTypeToString(event->type) 
                      << " - Note " << static_cast<int>(event->note) 
                      << " (" << midiNoteToString(event->note) << ")"
                      << ", Velocity " << static_cast<int>(event->velocity)
                      << ", Channel " << static_cast<int>(event->channel)
                      << ", Offset " << event->sampleOffset << std::endl;
        }
        
        // Hit continues (should not generate new event)
        event = handler->processHitState(true, 200, timestamp + 2000);
        std::cout << "Hit continues: " << (event ? "Event generated" : "No event") << std::endl;
        
        // Hit ends
        event = handler->processHitState(false, 300, timestamp + 3000);
        if (event) {
            std::cout << "Hit ends: Generated " << eventTypeToString(event->type) 
                      << " - Note " << static_cast<int>(event->note) 
                      << ", Velocity " << static_cast<int>(event->velocity)
                      << ", Offset " << event->sampleOffset << std::endl;
        }
        
        auto stats = handler->getStatistics();
        std::cout << "\nGenerated Events: " << stats.totalEvents 
                  << " (Note On: " << stats.noteOnEvents 
                  << ", Note Off: " << stats.noteOffEvents << ")" << std::endl;
    }
    
    void demoTimingAndDelays()
    {
        std::cout << "--- MIDI Timing and Delays ---" << std::endl;
        
        // Configure with delayed note off
        MidiEventHandler::Config config;
        config.hitNote = 45;
        config.hitVelocity = 127;
        config.sendNoteOff = true;
        config.noteOffDelay = 2400;  // 50ms at 48kHz
        config.useHostTimeStamp = true;
        
        auto handler = std::make_unique<MidiEventHandler>(config);
        
        uint64_t timestamp = getCurrentTimestamp();
        
        std::cout << "Testing delayed note off (50ms delay):" << std::endl;
        
        // Start hit
        auto* event = handler->processHitState(true, 0, timestamp);
        if (event) {
            std::cout << "  Sample 0: NOTE ON generated" << std::endl;
        }
        
        // End hit (should schedule note off)
        event = handler->processHitState(false, 1000, timestamp + 20000);
        std::cout << "  Sample 1000: Hit ended, " << (event ? "immediate note off" : "note off scheduled") << std::endl;
        
        // Check for pending events
        for (int sample = 1100; sample <= 3500; sample += 500) {
            event = handler->processPendingEvents(sample);
            if (event) {
                std::cout << "  Sample " << sample << ": Delayed NOTE OFF triggered" << std::endl;
                break;
            } else {
                std::cout << "  Sample " << sample << ": No pending events yet" << std::endl;
            }
        }
        
        // Test immediate note off
        std::cout << "\nTesting immediate note off:" << std::endl;
        config.noteOffDelay = 0;
        auto immediateHandler = std::make_unique<MidiEventHandler>(config);
        
        immediateHandler->processHitState(true, 2000, timestamp + 40000);
        event = immediateHandler->processHitState(false, 2500, timestamp + 45000);
        if (event && event->type == MidiEventHandler::EventType::NoteOff) {
            std::cout << "  Immediate note off generated at sample " << event->sampleOffset << std::endl;
        }
    }
    
    void demoConfigurationOptions()
    {
        std::cout << "--- Configuration Options ---" << std::endl;
        
        // Test different configurations
        struct TestConfig {
            std::string name;
            uint8_t note;
            uint8_t velocity;
            uint8_t channel;
            bool sendNoteOff;
        };
        
        std::vector<TestConfig> configs = {
            {"Default (A2, Full Velocity)", 45, 127, 0, true},
            {"Middle C, Medium Velocity", 60, 80, 0, true},
            {"High Note, Low Velocity", 84, 40, 1, true},
            {"Note Off Disabled", 45, 127, 0, false}
        };
        
        uint64_t timestamp = getCurrentTimestamp();
        
        for (const auto& testConfig : configs) {
            std::cout << "\nTesting " << testConfig.name << ":" << std::endl;
            
            MidiEventHandler::Config config;
            config.hitNote = testConfig.note;
            config.hitVelocity = testConfig.velocity;
            config.channel = testConfig.channel;
            config.sendNoteOff = testConfig.sendNoteOff;
            
            auto handler = std::make_unique<MidiEventHandler>(config);
            
            // Generate hit
            auto* onEvent = handler->processHitState(true, 0, timestamp);
            if (onEvent) {
                std::cout << "  Note ON:  " << midiNoteToString(onEvent->note) 
                          << " (MIDI " << static_cast<int>(onEvent->note) << ")"
                          << ", Vel " << static_cast<int>(onEvent->velocity)
                          << ", Ch " << static_cast<int>(onEvent->channel) + 1 << std::endl;
            }
            
            auto* offEvent = handler->processHitState(false, 100, timestamp + 1000);
            if (offEvent) {
                std::cout << "  Note OFF: " << midiNoteToString(offEvent->note) 
                          << " (MIDI " << static_cast<int>(offEvent->note) << ")"
                          << ", Ch " << static_cast<int>(offEvent->channel) + 1 << std::endl;
            } else if (testConfig.sendNoteOff) {
                std::cout << "  Note OFF: Expected but not generated (possible delay)" << std::endl;
            } else {
                std::cout << "  Note OFF: Disabled" << std::endl;
            }
        }
    }
    
    void demoRealtimeSimulation()
    {
        std::cout << "--- Real-time MIDI Processing Simulation ---" << std::endl;
        
        auto handler = createMidiEventHandler(45, 127);  // A2, full velocity
        
        // Simulate confidence values from hit detection
        std::vector<float> confidencePattern = {
            0.1f, 0.2f, 0.3f,           // Building up
            0.7f, 0.8f, 0.9f, 0.8f,     // Hit detected
            0.7f, 0.5f, 0.3f,           // Fading
            0.1f, 0.1f,                 // Gap
            0.8f, 0.9f, 0.8f,           // Second hit
            0.2f, 0.1f                  // End
        };
        
        float threshold = 0.6f;
        bool lastHitState = false;
        uint64_t baseTimestamp = getCurrentTimestamp();
        
        std::cout << "Processing confidence pattern with threshold " << threshold << ":" << std::endl;
        std::cout << "Frame | Confidence | Hit State | MIDI Event" << std::endl;
        std::cout << "------|------------|-----------|----------" << std::endl;
        
        for (size_t i = 0; i < confidencePattern.size(); ++i) {
            float confidence = confidencePattern[i];
            bool currentHit = confidence >= threshold;
            
            // Process MIDI events only on state changes
            MidiEventHandler::MidiEvent* event = nullptr;
            if (currentHit != lastHitState) {
                uint64_t timestamp = baseTimestamp + i * 20000;  // 20ms intervals
                event = handler->processHitState(currentHit, static_cast<int32_t>(i * 480), timestamp);
            }
            
            std::cout << std::setw(5) << i << " | "
                      << std::fixed << std::setprecision(3) << std::setw(10) << confidence << " | "
                      << std::setw(9) << (currentHit ? "HIT" : "---") << " | ";
            
            if (event) {
                std::cout << eventTypeToString(event->type);
            } else {
                std::cout << "---";
            }
            std::cout << std::endl;
            
            lastHitState = currentHit;
            
            // Simulate real-time processing interval
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        
        auto stats = handler->getStatistics();
        std::cout << "\nReal-time Processing Summary:" << std::endl;
        std::cout << "  Total MIDI events: " << stats.totalEvents << std::endl;
        std::cout << "  Note On events: " << stats.noteOnEvents << std::endl;
        std::cout << "  Note Off events: " << stats.noteOffEvents << std::endl;
    }
    
    void demoMidiChannelsAndNotes()
    {
        std::cout << "--- MIDI Channels and Note Range ---" << std::endl;
        
        // Test different MIDI notes and channels
        struct NoteTest {
            uint8_t note;
            std::string name;
            std::string description;
        };
        
        std::vector<NoteTest> noteTests = {
            {36, "C2", "Kick drum"},
            {45, "A2", "Default hit note"},
            {60, "C4", "Middle C"},
            {69, "A4", "A440 reference"},
            {84, "C6", "High note"}
        };
        
        std::cout << "Testing different MIDI notes:" << std::endl;
        
        for (const auto& test : noteTests) {
            auto handler = createMidiEventHandler(test.note, 100);
            
            uint64_t timestamp = getCurrentTimestamp();
            auto* event = handler->processHitState(true, 0, timestamp);
            
            if (event) {
                std::cout << "  MIDI " << std::setw(3) << static_cast<int>(test.note) 
                          << " (" << std::setw(2) << test.name << ") - " << test.description << std::endl;
            }
        }
        
        // Test MIDI channels
        std::cout << "\nTesting MIDI channels:" << std::endl;
        
        for (uint8_t channel = 0; channel < 4; ++channel) {
            MidiEventHandler::Config config;
            config.hitNote = 60;  // Middle C
            config.hitVelocity = 100;
            config.channel = channel;
            
            auto handler = std::make_unique<MidiEventHandler>(config);
            
            uint64_t timestamp = getCurrentTimestamp();
            auto* event = handler->processHitState(true, 0, timestamp);
            
            if (event) {
                std::cout << "  Channel " << static_cast<int>(channel) + 1 
                          << " (MIDI channel " << static_cast<int>(channel) << ")" << std::endl;
            }
        }
    }
    
    void demoStatisticsAndMonitoring()
    {
        std::cout << "--- Statistics and Performance Monitoring ---" << std::endl;
        
        auto handler = createMidiEventHandler();
        
        uint64_t timestamp = getCurrentTimestamp();
        
        // Generate a series of hits to collect statistics
        std::cout << "Generating test pattern..." << std::endl;
        
        for (int cycle = 0; cycle < 10; ++cycle) {
            // Hit start
            handler->processHitState(true, cycle * 1000, timestamp + cycle * 10000);
            
            // Hit end
            handler->processHitState(false, cycle * 1000 + 500, timestamp + cycle * 10000 + 5000);
        }
        
        auto stats = handler->getStatistics();
        
        std::cout << "\nMIDI Event Statistics:" << std::endl;
        std::cout << "  Total events generated: " << stats.totalEvents << std::endl;
        std::cout << "  Note On events: " << stats.noteOnEvents << std::endl;
        std::cout << "  Note Off events: " << stats.noteOffEvents << std::endl;
        std::cout << "  Failed events: " << stats.failedEvents << std::endl;
        std::cout << "  Last event timestamp: " << stats.lastEventTimeStamp << std::endl;
        
        // Test reset functionality
        std::cout << "\nTesting statistics reset..." << std::endl;
        handler->resetStatistics();
        
        auto resetStats = handler->getStatistics();
        std::cout << "  After reset - Total events: " << resetStats.totalEvents << std::endl;
        
        // Performance test
        std::cout << "\nPerformance test (1000 state changes):" << std::endl;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 1000; ++i) {
            bool state = (i % 2 == 0);
            handler->processHitState(state, i, timestamp + i * 1000);
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        double eventsPerSecond = 1000.0 / (duration.count() / 1000000.0);
        
        std::cout << "  Processing time: " << duration.count() << " μs" << std::endl;
        std::cout << "  Events per second: " << std::fixed << std::setprecision(0) << eventsPerSecond << std::endl;
        std::cout << "  Suitable for real-time: " << (eventsPerSecond > 100000 ? "YES" : "NO") << std::endl;
    }
    
    // Helper functions
    uint64_t getCurrentTimestamp() 
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    }
    
    std::string eventTypeToString(MidiEventHandler::EventType type)
    {
        switch (type) {
            case MidiEventHandler::EventType::NoteOn:
                return "NOTE ON";
            case MidiEventHandler::EventType::NoteOff:
                return "NOTE OFF";
            case MidiEventHandler::EventType::ControlChange:
                return "CC";
            default:
                return "UNKNOWN";
        }
    }
};

int main()
{
    try {
        MidiDemo demo;
        demo.runAllDemos();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "MIDI demo failed with exception: " << e.what() << std::endl;
        return 1;
    }
} 