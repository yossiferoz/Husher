#include <gtest/gtest.h>
#include <vector>
#include <chrono>

#include "MidiEventHandler.h"

using namespace KhDetector;

class MidiEventHandlerTest : public ::testing::Test 
{
protected:
    void SetUp() override 
    {
        // Default configuration for most tests
        config = MidiEventHandler::Config{};
        config.hitNote = 45;        // A2
        config.hitVelocity = 127;   // Maximum velocity
        config.channel = 0;         // MIDI channel 1
        config.sendNoteOff = true;  // Enable note off
        config.noteOffDelay = 0;    // Immediate note off
        config.useHostTimeStamp = true;
        
        handler = std::make_unique<MidiEventHandler>(config);
    }
    
    void TearDown() override 
    {
        handler.reset();
    }
    
    MidiEventHandler::Config config;
    std::unique_ptr<MidiEventHandler> handler;
    
    // Helper to simulate time passage
    uint64_t getCurrentTimestamp() 
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    }
};

TEST_F(MidiEventHandlerTest, BasicConstruction)
{
    EXPECT_EQ(handler->getConfig().hitNote, 45);
    EXPECT_EQ(handler->getConfig().hitVelocity, 127);
    EXPECT_EQ(handler->getConfig().channel, 0);
    EXPECT_TRUE(handler->getConfig().sendNoteOff);
    
    auto stats = handler->getStatistics();
    EXPECT_EQ(stats.noteOnEvents, 0);
    EXPECT_EQ(stats.noteOffEvents, 0);
    EXPECT_EQ(stats.totalEvents, 0);
}

TEST_F(MidiEventHandlerTest, ConfigValidation)
{
    // Test automatic config correction
    MidiEventHandler::Config invalidConfig;
    invalidConfig.hitNote = 200;        // Invalid note (>127)
    invalidConfig.hitVelocity = 200;    // Invalid velocity (>127)
    invalidConfig.channel = 20;         // Invalid channel (>15)
    
    auto testHandler = std::make_unique<MidiEventHandler>(invalidConfig);
    auto correctedConfig = testHandler->getConfig();
    
    EXPECT_EQ(correctedConfig.hitNote, 45);     // Should be corrected to default
    EXPECT_EQ(correctedConfig.hitVelocity, 127); // Should be corrected to default
    EXPECT_EQ(correctedConfig.channel, 0);      // Should be corrected to default
}

TEST_F(MidiEventHandlerTest, SimpleHitDetection)
{
    uint64_t timestamp = getCurrentTimestamp();
    
    // No hit initially
    auto* event = handler->processHitState(false, 0, timestamp);
    EXPECT_EQ(event, nullptr);
    
    // Hit starts - should generate note on
    event = handler->processHitState(true, 100, timestamp + 1000);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type, MidiEventHandler::EventType::NoteOn);
    EXPECT_EQ(event->note, 45);
    EXPECT_EQ(event->velocity, 127);
    EXPECT_EQ(event->channel, 0);
    EXPECT_EQ(event->sampleOffset, 100);
    EXPECT_EQ(event->hostTimeStamp, timestamp + 1000);
    
    // Hit continues - no new event
    event = handler->processHitState(true, 200, timestamp + 2000);
    EXPECT_EQ(event, nullptr);
    
    // Hit ends - should generate note off
    event = handler->processHitState(false, 300, timestamp + 3000);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type, MidiEventHandler::EventType::NoteOff);
    EXPECT_EQ(event->note, 45);
    EXPECT_EQ(event->velocity, 0);  // Note off velocity should be 0
    EXPECT_EQ(event->channel, 0);
    EXPECT_EQ(event->sampleOffset, 300);
    EXPECT_EQ(event->hostTimeStamp, timestamp + 3000);
}

TEST_F(MidiEventHandlerTest, DelayedNoteOff)
{
    // Configure delayed note off
    config.noteOffDelay = 480;  // 10ms at 48kHz
    auto delayedHandler = std::make_unique<MidiEventHandler>(config);
    
    uint64_t timestamp = getCurrentTimestamp();
    
    // Hit starts - should generate note on
    auto* event = delayedHandler->processHitState(true, 0, timestamp);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type, MidiEventHandler::EventType::NoteOn);
    
    // Hit ends - should schedule note off but not send immediately
    event = delayedHandler->processHitState(false, 100, timestamp + 1000);
    EXPECT_EQ(event, nullptr);  // No immediate event
    
    // Check for pending events before delay time
    event = delayedHandler->processPendingEvents(200);
    EXPECT_EQ(event, nullptr);  // Too early
    
    // Check for pending events after delay time
    event = delayedHandler->processPendingEvents(600);  // 100 + 480 + margin
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type, MidiEventHandler::EventType::NoteOff);
}

TEST_F(MidiEventHandlerTest, DisabledNoteOff)
{
    // Configure with note off disabled
    config.sendNoteOff = false;
    auto noOffHandler = std::make_unique<MidiEventHandler>(config);
    
    uint64_t timestamp = getCurrentTimestamp();
    
    // Hit starts - should generate note on
    auto* event = noOffHandler->processHitState(true, 0, timestamp);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type, MidiEventHandler::EventType::NoteOn);
    
    // Hit ends - should not generate note off
    event = noOffHandler->processHitState(false, 100, timestamp + 1000);
    EXPECT_EQ(event, nullptr);
    
    // Check statistics
    auto stats = noOffHandler->getStatistics();
    EXPECT_EQ(stats.noteOnEvents, 1);
    EXPECT_EQ(stats.noteOffEvents, 0);
    EXPECT_EQ(stats.totalEvents, 1);
}

TEST_F(MidiEventHandlerTest, MultipleHitCycles)
{
    uint64_t timestamp = getCurrentTimestamp();
    int noteOnCount = 0;
    int noteOffCount = 0;
    
    // Simulate multiple hit cycles
    for (int cycle = 0; cycle < 5; ++cycle) {
        // Hit starts
        auto* event = handler->processHitState(true, cycle * 1000, timestamp + cycle * 10000);
        if (event && event->type == MidiEventHandler::EventType::NoteOn) {
            noteOnCount++;
        }
        
        // Hit ends
        event = handler->processHitState(false, cycle * 1000 + 500, timestamp + cycle * 10000 + 5000);
        if (event && event->type == MidiEventHandler::EventType::NoteOff) {
            noteOffCount++;
        }
    }
    
    EXPECT_EQ(noteOnCount, 5);
    EXPECT_EQ(noteOffCount, 5);
    
    auto stats = handler->getStatistics();
    EXPECT_EQ(stats.noteOnEvents, 5);
    EXPECT_EQ(stats.noteOffEvents, 5);
    EXPECT_EQ(stats.totalEvents, 10);
}

TEST_F(MidiEventHandlerTest, RapidStateChanges)
{
    uint64_t timestamp = getCurrentTimestamp();
    
    // Rapid on/off changes - should generate events for each change
    std::vector<bool> stateSequence = {true, false, true, false, true, false};
    std::vector<MidiEventHandler::EventType> expectedEvents = {
        MidiEventHandler::EventType::NoteOn,
        MidiEventHandler::EventType::NoteOff,
        MidiEventHandler::EventType::NoteOn,
        MidiEventHandler::EventType::NoteOff,
        MidiEventHandler::EventType::NoteOn,
        MidiEventHandler::EventType::NoteOff
    };
    
    for (size_t i = 0; i < stateSequence.size(); ++i) {
        auto* event = handler->processHitState(stateSequence[i], static_cast<int32_t>(i * 10), timestamp + i * 100);
        ASSERT_NE(event, nullptr) << "Expected event at index " << i;
        EXPECT_EQ(event->type, expectedEvents[i]) << "Wrong event type at index " << i;
    }
}

TEST_F(MidiEventHandlerTest, ResetFunctionality)
{
    uint64_t timestamp = getCurrentTimestamp();
    
    // Generate some events
    handler->processHitState(true, 0, timestamp);
    handler->processHitState(false, 100, timestamp + 1000);
    
    auto statsBefore = handler->getStatistics();
    EXPECT_GT(statsBefore.totalEvents, 0);
    
    // Reset should clear statistics but preserve configuration
    handler->reset();
    
    auto statsAfter = handler->getStatistics();
    EXPECT_EQ(statsAfter.noteOnEvents, 0);
    EXPECT_EQ(statsAfter.noteOffEvents, 0);
    EXPECT_EQ(statsAfter.totalEvents, 0);
    
    // Configuration should remain unchanged
    auto resetConfig = handler->getConfig();
    EXPECT_EQ(resetConfig.hitNote, config.hitNote);
    EXPECT_EQ(resetConfig.hitVelocity, config.hitVelocity);
    EXPECT_EQ(resetConfig.channel, config.channel);
}

TEST_F(MidiEventHandlerTest, ConfigurationUpdate)
{
    // Change configuration
    MidiEventHandler::Config newConfig = config;
    newConfig.hitNote = 60;         // C4
    newConfig.hitVelocity = 100;    // Different velocity
    newConfig.channel = 1;          // Different channel
    
    handler->updateConfig(newConfig);
    
    auto updatedConfig = handler->getConfig();
    EXPECT_EQ(updatedConfig.hitNote, 60);
    EXPECT_EQ(updatedConfig.hitVelocity, 100);
    EXPECT_EQ(updatedConfig.channel, 1);
    
    // Test event generation with new config
    uint64_t timestamp = getCurrentTimestamp();
    auto* event = handler->processHitState(true, 0, timestamp);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->note, 60);
    EXPECT_EQ(event->velocity, 100);
    EXPECT_EQ(event->channel, 1);
}

TEST_F(MidiEventHandlerTest, TimestampHandling)
{
    // Test with timestamp disabled
    config.useHostTimeStamp = false;
    auto noTimestampHandler = std::make_unique<MidiEventHandler>(config);
    
    uint64_t timestamp = getCurrentTimestamp();
    auto* event = noTimestampHandler->processHitState(true, 0, timestamp);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->hostTimeStamp, 0);  // Should be 0 when disabled
    
    // Test with timestamp enabled (default)
    event = handler->processHitState(true, 0, timestamp);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->hostTimeStamp, timestamp);  // Should preserve timestamp
}

TEST_F(MidiEventHandlerTest, NoteCollision)
{
    uint64_t timestamp = getCurrentTimestamp();
    
    // Start hit
    auto* event = handler->processHitState(true, 0, timestamp);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type, MidiEventHandler::EventType::NoteOn);
    
    // Try to start another hit while one is active - should not generate new note on
    event = handler->processHitState(true, 100, timestamp + 1000);
    EXPECT_EQ(event, nullptr);
    
    // End hit
    event = handler->processHitState(false, 200, timestamp + 2000);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type, MidiEventHandler::EventType::NoteOff);
    
    // Try to end hit again - should not generate new note off
    event = handler->processHitState(false, 300, timestamp + 3000);
    EXPECT_EQ(event, nullptr);
}

TEST_F(MidiEventHandlerTest, FactoryFunction)
{
    // Test factory function
    auto factoryHandler = createMidiEventHandler(60, 100);
    
    auto factoryConfig = factoryHandler->getConfig();
    EXPECT_EQ(factoryConfig.hitNote, 60);
    EXPECT_EQ(factoryConfig.hitVelocity, 100);
    
    // Test default factory
    auto defaultHandler = createMidiEventHandler();
    auto defaultConfig = defaultHandler->getConfig();
    EXPECT_EQ(defaultConfig.hitNote, 45);
    EXPECT_EQ(defaultConfig.hitVelocity, 127);
}

TEST_F(MidiEventHandlerTest, HelperFunctions)
{
    // Test MIDI note to string conversion
    EXPECT_EQ(midiNoteToString(45), "A2");      // A2
    EXPECT_EQ(midiNoteToString(60), "C4");      // Middle C
    EXPECT_EQ(midiNoteToString(69), "A4");      // A440
    EXPECT_EQ(midiNoteToString(128), "Invalid"); // Out of range
    
    // Test validation functions
    EXPECT_TRUE(isValidMidiNote(0));
    EXPECT_TRUE(isValidMidiNote(127));
    EXPECT_FALSE(isValidMidiNote(128));
    
    EXPECT_TRUE(isValidMidiVelocity(0));
    EXPECT_TRUE(isValidMidiVelocity(127));
    EXPECT_FALSE(isValidMidiVelocity(128));
    
    EXPECT_TRUE(isValidMidiChannel(0));
    EXPECT_TRUE(isValidMidiChannel(15));
    EXPECT_FALSE(isValidMidiChannel(16));
}

TEST_F(MidiEventHandlerTest, StatisticsAccuracy)
{
    uint64_t timestamp = getCurrentTimestamp();
    
    // Generate a known pattern of events
    handler->processHitState(true, 0, timestamp);      // Note on
    handler->processHitState(false, 100, timestamp);   // Note off
    handler->processHitState(true, 200, timestamp);    // Note on
    handler->processHitState(false, 300, timestamp);   // Note off
    
    auto stats = handler->getStatistics();
    EXPECT_EQ(stats.noteOnEvents, 2);
    EXPECT_EQ(stats.noteOffEvents, 2);
    EXPECT_EQ(stats.totalEvents, 4);
    EXPECT_EQ(stats.failedEvents, 0);
    EXPECT_EQ(stats.lastEventTimeStamp, timestamp);
    
    // Reset statistics
    handler->resetStatistics();
    auto resetStats = handler->getStatistics();
    EXPECT_EQ(resetStats.totalEvents, 0);
    EXPECT_EQ(resetStats.noteOnEvents, 0);
    EXPECT_EQ(resetStats.noteOffEvents, 0);
}

TEST_F(MidiEventHandlerTest, EdgeCases)
{
    // Test with zero timestamps
    auto* event = handler->processHitState(true, 0, 0);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->hostTimeStamp, 0);
    
    // Test with large sample offsets
    event = handler->processHitState(false, 1000000, 0);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->sampleOffset, 1000000);
    
    // Test pending events with negative sample positions (edge case)
    config.noteOffDelay = 100;
    auto delayHandler = std::make_unique<MidiEventHandler>(config);
    
    // Should handle gracefully without crashing
    event = delayHandler->processPendingEvents(-100);
    EXPECT_EQ(event, nullptr);
}

TEST_F(MidiEventHandlerTest, PerformanceTest)
{
    // Performance test with many state changes
    const int numIterations = 10000;
    uint64_t timestamp = getCurrentTimestamp();
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numIterations; ++i) {
        bool hitState = (i % 2 == 0);  // Alternating hit states
        handler->processHitState(hitState, i, timestamp + i);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // Should process many events quickly (less than 1ms per 1000 events)
    double eventsPerMicrosecond = static_cast<double>(numIterations) / duration.count();
    EXPECT_GT(eventsPerMicrosecond, 10.0);  // Very conservative performance requirement
    
    std::cout << "MidiEventHandler Performance: " << eventsPerMicrosecond << " events/μs" << std::endl;
    std::cout << "Total processing time: " << duration.count() << " μs for " << numIterations << " events" << std::endl;
    
    auto stats = handler->getStatistics();
    EXPECT_EQ(stats.totalEvents, numIterations);
} 