#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/base/ustring.h"
#include <atomic>
#include <memory>

// Forward declarations
namespace KhDetector {
    class KhDetectorEditor;
}

using namespace Steinberg;
using namespace Steinberg::Vst;

//------------------------------------------------------------------------
class KhDetectorController : public EditControllerEx1
{
public:
    KhDetectorController();
    ~KhDetectorController() override;

    // Create function
    static FUnknown* createInstance(void* /*context*/)
    {
        return (IEditController*)new KhDetectorController;
    }

    // IPluginBase
    tresult PLUGIN_API initialize(FUnknown* context) override;
    tresult PLUGIN_API terminate() override;

    // EditController
    tresult PLUGIN_API setComponentState(IBStream* state) override;
    IPlugView* PLUGIN_API createView(FIDString name) override;
    tresult PLUGIN_API setState(IBStream* state) override;
    tresult PLUGIN_API getState(IBStream* state) override;
    tresult PLUGIN_API setParamNormalized(ParamID tag, ParamValue value) override;
    tresult PLUGIN_API getParamStringByValue(ParamID tag, ParamValue valueNormalized, String128 string) override;
    tresult PLUGIN_API getParamValueByString(ParamID tag, TChar* string, ParamValue& valueNormalized) override;
    
    /**
     * @brief Set hit state reference for GUI visualization
     * 
     * This should be called to connect the processor's atomic hit state
     * to the GUI for real-time hit detection visualization.
     */
    void setHitStateReference(std::atomic<bool>* hitState);
    
    /**
     * @brief Set sensitivity (threshold) value
     * 
     * @param value Normalized sensitivity value (0.0-1.0)
     */
    void setSensitivity(float value);
    
    /**
     * @brief Write markers to host timeline
     * 
     * This method triggers writing of hit detection markers to the host's
     * timeline for synchronization with other audio events.
     */
    void writeMarkers();

    // Interface
    DEFINE_INTERFACES
        // Here you can add more supported WST3 interfaces
        // DEF_INTERFACE (Vst::IXXX)
    END_DEFINE_INTERFACES(EditController)
    DELEGATE_REFCOUNT(EditController)

private:
    // Parameters
    enum Parameters
    {
        kBypass = 0,
        kHitDetected,
        kSensitivity,
        kNumParameters
    };
    
    // GUI management
    std::atomic<bool>* mHitStateRef = nullptr;
    KhDetector::KhDetectorEditor* mCurrentEditor = nullptr;
}; 