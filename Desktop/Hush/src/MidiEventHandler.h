#pragma once

#include <cstdint>
#include <functional>

// Forward declarations for different plugin formats
#ifdef VST3_SUPPORT
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
using namespace Steinberg;
using namespace Steinberg::Vst;
#endif

#ifdef AAX_SUPPORT
// AAX includes would go here
// #include "AAX_IMIDINode.h"
#endif

#ifdef AU_SUPPORT
// AudioUnit includes would go here
// #include <AudioUnit/AudioUnit.h>
#endif

namespace KhDetector {

/**
 * @brief Cross-platform MIDI event handler for hit detection
 * 
 * This class abstracts MIDI event generation across different plugin formats.
 * It handles note on/off events with proper timestamping and velocity.
 */
class MidiEventHandler
{
public:
    /**
     * @brief MIDI event types
     */
    enum class EventType
    {
        NoteOn,
        NoteOff,
        ControlChange
    };
    
    /**
     * @brief MIDI event data structure
     */
    struct MidiEvent
    {
        EventType type;
        uint8_t channel = 0;        // MIDI channel (0-15)
        uint8_t note = 45;          // MIDI note number (A2)
        uint8_t velocity = 127;     // Velocity value (0-127)
        uint8_t controller = 0;     // For CC events
        int32_t sampleOffset = 0;   // Sample offset within buffer
        uint64_t hostTimeStamp = 0; // Host timestamp
    };
    
    /**
     * @brief Configuration for MIDI output
     */
    struct Config
    {
        uint8_t hitNote = 45;           // MIDI note for hit (A2)
        uint8_t hitVelocity = 127;      // Velocity for hit note
        uint8_t channel = 0;            // MIDI channel (0-15)
        bool sendNoteOff = true;        // Send note off after hit ends
        int32_t noteOffDelay = 100;     // Delay in samples before note off
        bool useHostTimeStamp = true;   // Use host timestamp when available
    };
    
    /**
     * @brief Constructor
     */
    explicit MidiEventHandler(const Config& config = Config{});
    
    /**
     * @brief Destructor
     */
    ~MidiEventHandler() = default;
    
    // Non-copyable but movable
    MidiEventHandler(const MidiEventHandler&) = delete;
    MidiEventHandler& operator=(const MidiEventHandler&) = delete;
    MidiEventHandler(MidiEventHandler&&) = default;
    MidiEventHandler& operator=(MidiEventHandler&&) = default;
    
    /**
     * @brief Process hit state change and generate MIDI events
     * 
     * @param hitState Current hit state (true = hit detected)
     * @param sampleOffset Sample offset within current buffer
     * @param hostTimeStamp Host timestamp for the event
     * @return MIDI event to send (or nullptr if no event needed)
     */
    MidiEvent* processHitState(bool hitState, int32_t sampleOffset, uint64_t hostTimeStamp);
    
    /**
     * @brief Check if there are pending note off events to process
     * 
     * @param currentSampleOffset Current sample position
     * @return MIDI event to send (or nullptr if no pending events)
     */
    MidiEvent* processPendingEvents(int32_t currentSampleOffset);
    
    /**
     * @brief Reset internal state
     */
    void reset();
    
    /**
     * @brief Update configuration
     */
    void updateConfig(const Config& config);
    
    /**
     * @brief Get current configuration
     */
    const Config& getConfig() const { return mConfig; }
    
#ifdef VST3_SUPPORT
    /**
     * @brief Send MIDI event via VST3 event list
     * 
     * @param eventList VST3 event list to add event to
     * @param event MIDI event to send
     * @return true if event was successfully added
     */
    static bool sendVST3Event(IEventList* eventList, const MidiEvent& event);
#endif

#ifdef AAX_SUPPORT
    /**
     * @brief Send MIDI event via AAX MIDI node
     * 
     * @param midiNode AAX MIDI node
     * @param event MIDI event to send
     * @return true if event was successfully sent
     */
    static bool sendAAXEvent(/* AAX_IMIDINode* midiNode, */ const MidiEvent& event);
#endif

#ifdef AU_SUPPORT
    /**
     * @brief Send MIDI event via AudioUnit MIDI output
     * 
     * @param audioUnit AudioUnit instance
     * @param event MIDI event to send
     * @return true if event was successfully sent
     */
    static bool sendAUEvent(/* AudioUnit audioUnit, */ const MidiEvent& event);
#endif
    
    /**
     * @brief Get statistics about sent events
     */
    struct Statistics
    {
        uint64_t noteOnEvents = 0;
        uint64_t noteOffEvents = 0;
        uint64_t totalEvents = 0;
        uint64_t failedEvents = 0;
        uint64_t lastEventTimeStamp = 0;
    };
    
    const Statistics& getStatistics() const { return mStats; }
    void resetStatistics() { mStats = Statistics{}; }

private:
    Config mConfig;
    
    // State tracking
    bool mLastHitState = false;
    bool mNoteIsOn = false;
    int32_t mNoteOffScheduledAt = -1;   // Sample position when note off should be sent
    uint64_t mLastEventTimeStamp = 0;
    
    // Current event buffer
    MidiEvent mCurrentEvent;
    bool mHasCurrentEvent = false;
    
    // Statistics
    mutable Statistics mStats;
    
    /**
     * @brief Create a MIDI event
     */
    MidiEvent createEvent(EventType type, int32_t sampleOffset, uint64_t hostTimeStamp);
    
    /**
     * @brief Update statistics
     */
    void updateStatistics(const MidiEvent& event);
};

/**
 * @brief Factory function to create MIDI event handler with default settings
 */
std::unique_ptr<MidiEventHandler> createMidiEventHandler(uint8_t hitNote = 45, uint8_t velocity = 127);

/**
 * @brief Helper function to convert MIDI note number to note name
 */
std::string midiNoteToString(uint8_t noteNumber);

/**
 * @brief Helper function to validate MIDI values
 */
bool isValidMidiNote(uint8_t note);
bool isValidMidiVelocity(uint8_t velocity);
bool isValidMidiChannel(uint8_t channel);

} // namespace KhDetector 