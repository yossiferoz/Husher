#include "MidiEventHandler.h"
#include <iostream>
#include <algorithm>
#include <array>
#include <string>

#ifdef VST3_SUPPORT
#include "pluginterfaces/vst/ivstevents.h"
#endif

namespace KhDetector {

MidiEventHandler::MidiEventHandler(const Config& config)
    : mConfig(config)
{
    // Validate configuration
    if (!isValidMidiNote(mConfig.hitNote)) {
        std::cerr << "MidiEventHandler: Invalid hit note " << static_cast<int>(mConfig.hitNote) 
                  << ", using default (45)" << std::endl;
        mConfig.hitNote = 45;
    }
    
    if (!isValidMidiVelocity(mConfig.hitVelocity)) {
        std::cerr << "MidiEventHandler: Invalid velocity " << static_cast<int>(mConfig.hitVelocity) 
                  << ", using default (127)" << std::endl;
        mConfig.hitVelocity = 127;
    }
    
    if (!isValidMidiChannel(mConfig.channel)) {
        std::cerr << "MidiEventHandler: Invalid MIDI channel " << static_cast<int>(mConfig.channel) 
                  << ", using default (0)" << std::endl;
        mConfig.channel = 0;
    }
    
    std::cout << "MidiEventHandler initialized:" << std::endl;
    std::cout << "  Hit note: " << static_cast<int>(mConfig.hitNote) 
              << " (" << midiNoteToString(mConfig.hitNote) << ")" << std::endl;
    std::cout << "  Velocity: " << static_cast<int>(mConfig.hitVelocity) << std::endl;
    std::cout << "  Channel: " << static_cast<int>(mConfig.channel) << std::endl;
    std::cout << "  Note off enabled: " << (mConfig.sendNoteOff ? "yes" : "no") << std::endl;
}

MidiEventHandler::MidiEvent* MidiEventHandler::processHitState(bool hitState, int32_t sampleOffset, uint64_t hostTimeStamp)
{
    // Check for state change
    if (hitState == mLastHitState) {
        return nullptr; // No change in state
    }
    
    mLastHitState = hitState;
    mHasCurrentEvent = false;
    
    if (hitState && !mNoteIsOn) {
        // Hit started - send note on
        mCurrentEvent = createEvent(EventType::NoteOn, sampleOffset, hostTimeStamp);
        mNoteIsOn = true;
        mHasCurrentEvent = true;
        
        std::cout << "MidiEventHandler: Sending NOTE ON - Note " << static_cast<int>(mConfig.hitNote) 
                  << ", Velocity " << static_cast<int>(mConfig.hitVelocity) 
                  << ", Offset " << sampleOffset << std::endl;
        
    } else if (!hitState && mNoteIsOn) {
        // Hit ended - schedule note off or send immediately
        if (mConfig.sendNoteOff) {
            if (mConfig.noteOffDelay > 0) {
                // Schedule note off for later
                mNoteOffScheduledAt = sampleOffset + mConfig.noteOffDelay;
                std::cout << "MidiEventHandler: Scheduled NOTE OFF for sample " << mNoteOffScheduledAt << std::endl;
            } else {
                // Send note off immediately
                mCurrentEvent = createEvent(EventType::NoteOff, sampleOffset, hostTimeStamp);
                mNoteIsOn = false;
                mHasCurrentEvent = true;
                
                std::cout << "MidiEventHandler: Sending NOTE OFF - Note " << static_cast<int>(mConfig.hitNote) 
                          << ", Offset " << sampleOffset << std::endl;
            }
        } else {
            // Note off disabled, just mark note as off
            mNoteIsOn = false;
        }
    }
    
    return mHasCurrentEvent ? &mCurrentEvent : nullptr;
}

MidiEventHandler::MidiEvent* MidiEventHandler::processPendingEvents(int32_t currentSampleOffset)
{
    // Check if we have a scheduled note off
    if (mNoteOffScheduledAt >= 0 && currentSampleOffset >= mNoteOffScheduledAt) {
        // Time to send the note off
        mCurrentEvent = createEvent(EventType::NoteOff, currentSampleOffset, mLastEventTimeStamp);
        mNoteIsOn = false;
        mNoteOffScheduledAt = -1;
        mHasCurrentEvent = true;
        
        std::cout << "MidiEventHandler: Sending scheduled NOTE OFF - Note " << static_cast<int>(mConfig.hitNote) 
                  << ", Offset " << currentSampleOffset << std::endl;
        
        return &mCurrentEvent;
    }
    
    return nullptr;
}

void MidiEventHandler::reset()
{
    mLastHitState = false;
    mNoteIsOn = false;
    mNoteOffScheduledAt = -1;
    mLastEventTimeStamp = 0;
    mHasCurrentEvent = false;
    
    std::cout << "MidiEventHandler: Reset complete" << std::endl;
}

void MidiEventHandler::updateConfig(const Config& config)
{
    mConfig = config;
    
    // Validate new configuration
    if (!isValidMidiNote(mConfig.hitNote)) {
        mConfig.hitNote = 45;
    }
    if (!isValidMidiVelocity(mConfig.hitVelocity)) {
        mConfig.hitVelocity = 127;
    }
    if (!isValidMidiChannel(mConfig.channel)) {
        mConfig.channel = 0;
    }
    
    std::cout << "MidiEventHandler: Configuration updated" << std::endl;
}

MidiEventHandler::MidiEvent MidiEventHandler::createEvent(EventType type, int32_t sampleOffset, uint64_t hostTimeStamp)
{
    MidiEvent event;
    event.type = type;
    event.channel = mConfig.channel;
    event.note = mConfig.hitNote;
    event.velocity = (type == EventType::NoteOn) ? mConfig.hitVelocity : 0;
    event.sampleOffset = sampleOffset;
    event.hostTimeStamp = mConfig.useHostTimeStamp ? hostTimeStamp : 0;
    
    mLastEventTimeStamp = hostTimeStamp;
    updateStatistics(event);
    
    return event;
}

void MidiEventHandler::updateStatistics(const MidiEvent& event)
{
    mStats.totalEvents++;
    mStats.lastEventTimeStamp = event.hostTimeStamp;
    
    switch (event.type) {
        case EventType::NoteOn:
            mStats.noteOnEvents++;
            break;
        case EventType::NoteOff:
            mStats.noteOffEvents++;
            break;
        default:
            break;
    }
}

#ifdef VST3_SUPPORT
bool MidiEventHandler::sendVST3Event(IEventList* eventList, const MidiEvent& event)
{
    if (!eventList) {
        return false;
    }
    
    Event vstEvent = {};
    vstEvent.busIndex = 0;  // Assume first MIDI bus
    vstEvent.sampleOffset = event.sampleOffset;
    vstEvent.ppqPosition = 0;  // Could be calculated from host time
    vstEvent.flags = Event::kIsLive;
    
    switch (event.type) {
        case EventType::NoteOn:
            vstEvent.type = Event::kNoteOnEvent;
            vstEvent.noteOn.channel = event.channel;
            vstEvent.noteOn.pitch = event.note;
            vstEvent.noteOn.velocity = static_cast<float>(event.velocity) / 127.0f;
            vstEvent.noteOn.length = 0;  // Indefinite length
            vstEvent.noteOn.tuning = 0.0f;
            vstEvent.noteOn.noteId = -1;  // No specific note ID
            break;
            
        case EventType::NoteOff:
            vstEvent.type = Event::kNoteOffEvent;
            vstEvent.noteOff.channel = event.channel;
            vstEvent.noteOff.pitch = event.note;
            vstEvent.noteOff.velocity = static_cast<float>(event.velocity) / 127.0f;
            vstEvent.noteOff.noteId = -1;  // No specific note ID
            vstEvent.noteOff.tuning = 0.0f;
            break;
            
        default:
            return false;  // Unsupported event type
    }
    
    tresult result = eventList->addEvent(vstEvent);
    return result == kResultOk;
}
#endif

#ifdef AAX_SUPPORT
bool MidiEventHandler::sendAAXEvent(const MidiEvent& event)
{
    // AAX implementation would go here
    // This is a placeholder for AAX MIDI event sending
    
    /*
    // Example AAX MIDI event sending (pseudo-code):
    AAX_CMidiPacket midiPacket;
    
    switch (event.type) {
        case EventType::NoteOn:
            midiPacket.mTimestamp = event.hostTimeStamp;
            midiPacket.mData[0] = 0x90 | event.channel;  // Note on + channel
            midiPacket.mData[1] = event.note;
            midiPacket.mData[2] = event.velocity;
            midiPacket.mLength = 3;
            break;
            
        case EventType::NoteOff:
            midiPacket.mTimestamp = event.hostTimeStamp;
            midiPacket.mData[0] = 0x80 | event.channel;  // Note off + channel
            midiPacket.mData[1] = event.note;
            midiPacket.mData[2] = event.velocity;
            midiPacket.mLength = 3;
            break;
            
        default:
            return false;
    }
    
    return midiNode->PostMIDIPacket(&midiPacket) == AAX_SUCCESS;
    */
    
    (void)event;  // Suppress unused parameter warning
    return false; // Not implemented yet
}
#endif

#ifdef AU_SUPPORT
bool MidiEventHandler::sendAUEvent(const MidiEvent& event)
{
    // AudioUnit implementation would go here
    // This is a placeholder for AU MIDI event sending
    
    /*
    // Example AU MIDI event sending (pseudo-code):
    MusicDeviceMIDIEvent midiEvent;
    
    switch (event.type) {
        case EventType::NoteOn:
            midiEvent.status = 0x90 | event.channel;  // Note on + channel
            midiEvent.data1 = event.note;
            midiEvent.data2 = event.velocity;
            break;
            
        case EventType::NoteOff:
            midiEvent.status = 0x80 | event.channel;  // Note off + channel
            midiEvent.data1 = event.note;
            midiEvent.data2 = event.velocity;
            break;
            
        default:
            return false;
    }
    
    OSStatus result = MusicDeviceMIDIEvent(audioUnit, midiEvent.status, 
                                          midiEvent.data1, midiEvent.data2, 
                                          event.sampleOffset);
    return result == noErr;
    */
    
    (void)event;  // Suppress unused parameter warning
    return false; // Not implemented yet
}
#endif

// Factory function
std::unique_ptr<MidiEventHandler> createMidiEventHandler(uint8_t hitNote, uint8_t velocity)
{
    MidiEventHandler::Config config;
    config.hitNote = hitNote;
    config.hitVelocity = velocity;
    return std::make_unique<MidiEventHandler>(config);
}

// Helper functions
std::string midiNoteToString(uint8_t noteNumber)
{
    static const std::array<const char*, 12> noteNames = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    
    if (noteNumber > 127) {
        return "Invalid";
    }
    
    int octave = (noteNumber / 12) - 1;
    int noteIndex = noteNumber % 12;
    
    return std::string(noteNames[noteIndex]) + std::to_string(octave);
}

bool isValidMidiNote(uint8_t note)
{
    return note <= 127;
}

bool isValidMidiVelocity(uint8_t velocity)
{
    return velocity <= 127;
}

bool isValidMidiChannel(uint8_t channel)
{
    return channel <= 15;
}

} // namespace KhDetector 