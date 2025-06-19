#include "KhDetectorController.h"
#include "KhDetectorVersion.h"
#include "KhDetectorEditor.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/vstpresetkeys.h"
#include <iostream>

using namespace Steinberg;

//------------------------------------------------------------------------
KhDetectorController::KhDetectorController()
{
}

//------------------------------------------------------------------------
KhDetectorController::~KhDetectorController()
{
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorController::initialize(FUnknown* context)
{
    tresult result = EditControllerEx1::initialize(context);
    if (result != kResultOk)
    {
        return result;
    }

    // Create Parameters
    parameters.addParameter(STR16("Bypass"), nullptr, 1, 0,
                          ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass, kBypass);

    // Add hit detection parameter (read-only, for display/automation only)
    parameters.addParameter(STR16("Hit Detected"), nullptr, 1, 0,
                          ParameterInfo::kIsReadOnly, kHitDetected);
    
    // Add sensitivity parameter (0.0 - 1.0, default 0.6)
    parameters.addParameter(STR16("Sensitivity"), STR16("%"), 0, 0.6,
                          ParameterInfo::kCanAutomate, kSensitivity);

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorController::terminate()
{
    return EditControllerEx1::terminate();
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorController::setComponentState(IBStream* state)
{
    // Here we get the state of the component (Processor part)
    if (!state)
        return kResultFalse;

    IBStreamer streamer(state, kLittleEndian);
    
    int32 savedBypass = 0;
    if (streamer.readInt32(savedBypass) == false)
        return kResultFalse;

    setParamNormalized(kBypass, savedBypass ? 1 : 0);
    
    // Read sensitivity parameter
    float savedSensitivity = 0.6f;
    if (streamer.readFloat(savedSensitivity) == false)
        return kResultFalse;
    
    setParamNormalized(kSensitivity, savedSensitivity);

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorController::setState(IBStream* state)
{
    IBStreamer streamer(state, kLittleEndian);

    int32 savedBypass = 0;
    if (streamer.readInt32(savedBypass) == false)
        return kResultFalse;

    setParamNormalized(kBypass, savedBypass ? 1 : 0);
    
    // Read sensitivity parameter
    float savedSensitivity = 0.6f;
    if (streamer.readFloat(savedSensitivity) == false)
        return kResultFalse;
    
    setParamNormalized(kSensitivity, savedSensitivity);

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorController::getState(IBStream* state)
{
    IBStreamer streamer(state, kLittleEndian);

    int32 toSaveBypass = getParamNormalized(kBypass) ? 1 : 0;
    streamer.writeInt32(toSaveBypass);
    
    // Save sensitivity parameter
    float toSaveSensitivity = static_cast<float>(getParamNormalized(kSensitivity));
    streamer.writeFloat(toSaveSensitivity);

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorController::setParamNormalized(ParamID tag, ParamValue value)
{
    tresult result = EditControllerEx1::setParamNormalized(tag, value);
    
    // Update editor if it exists
    if (mCurrentEditor && tag == kSensitivity) {
        mCurrentEditor->updateSensitivity(static_cast<float>(value));
    }
    
    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorController::getParamStringByValue(ParamID tag, ParamValue valueNormalized, String128 string)
{
    switch (tag)
    {
        case kBypass:
            if (valueNormalized > 0.5)
                Steinberg::UString(string, 128).fromAscii("On");
            else
                Steinberg::UString(string, 128).fromAscii("Off");
            return kResultTrue;
        
        case kHitDetected:
            if (valueNormalized > 0.5)
                Steinberg::UString(string, 128).fromAscii("HIT");
            else
                Steinberg::UString(string, 128).fromAscii("---");
            return kResultTrue;
            
        case kSensitivity:
        {
            char text[32];
            snprintf(text, sizeof(text), "%.2f", valueNormalized);
            Steinberg::UString(string, 128).fromAscii(text);
            return kResultTrue;
        }
    }
    return EditControllerEx1::getParamStringByValue(tag, valueNormalized, string);
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorController::getParamValueByString(ParamID tag, TChar* string, ParamValue& valueNormalized)
{
    switch (tag)
    {
        case kBypass:
        {
            Steinberg::UString wrapper((TChar*)string, -1);
            if (wrapper.isEqual(STR16("On")))
                valueNormalized = 1.0;
            else if (wrapper.isEqual(STR16("Off")))
                valueNormalized = 0.0;
            else
                return kResultFalse;
            return kResultTrue;
        }
        
        case kSensitivity:
        {
            Steinberg::UString wrapper((TChar*)string, -1);
            char8 text[128];
            wrapper.toAscii(text, 128);
            valueNormalized = strtod(text, nullptr);
            valueNormalized = std::max(0.0, std::min(1.0, valueNormalized));
            return kResultTrue;
        }
    }
    return EditControllerEx1::getParamValueByString(tag, string, valueNormalized);
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API KhDetectorController::createView(FIDString name)
{
    // Here the Host wants to open your editor (if you have one)
    if (FIDStringsEqual(name, Vst::ViewType::kEditor))
    {
        // Create our custom VSTGUI editor with medium size by default
        VSTGUI::CRect editorSize(0, 0, 1100, 680);  // Medium size
        
        // Create dummy atomic bool if no hit state reference is available
        static std::atomic<bool> dummyHitState{false};
        std::atomic<bool>& hitStateRef = mHitStateRef ? *mHitStateRef : dummyHitState;
        
        auto editor = KhDetector::KhDetectorEditor::create(editorSize, this, hitStateRef);
        
        // Keep track of current editor
        mCurrentEditor = editor.get();
        
        // Update editor with current parameter values
        mCurrentEditor->updateSensitivity(static_cast<float>(getParamNormalized(kSensitivity)));
        
        std::cout << "KhDetectorController: Created VSTGUI editor" << std::endl;
        
        return editor.release();  // Transfer ownership to host
    }
    return nullptr;
}

//------------------------------------------------------------------------
void KhDetectorController::setHitStateReference(std::atomic<bool>* hitState)
{
    mHitStateRef = hitState;
    std::cout << "KhDetectorController: Hit state reference set" << std::endl;
}

//------------------------------------------------------------------------
void KhDetectorController::setSensitivity(float value)
{
    // Clamp value to valid range
    value = std::max(0.0f, std::min(1.0f, value));
    
    // Update parameter
    setParamNormalized(kSensitivity, value);
    
    // Notify host of parameter change
    performEdit(kSensitivity, value);
    
    std::cout << "KhDetectorController: Sensitivity set to " << value << std::endl;
}

//------------------------------------------------------------------------
void KhDetectorController::writeMarkers()
{
    // TODO: Implement marker writing functionality
    // This would typically involve:
    // 1. Getting current playback position from host
    // 2. Creating timeline markers at hit detection points
    // 3. Sending marker data to host via appropriate VST3 interfaces
    
    std::cout << "KhDetectorController: Write Markers requested (not yet implemented)" << std::endl;
    
    // For now, just log the request
    // In a full implementation, you would:
    // - Access IHostApplication interface
    // - Create marker events
    // - Send them to the host timeline
} 