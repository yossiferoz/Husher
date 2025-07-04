#include "public.sdk/source/main/pluginfactory.h"
#include "KhDetectorProcessor.h"
#include "KhDetectorController.h"
#include "KhDetectorVersion.h"

//------------------------------------------------------------------------
#define stringPluginName kPluginName

using namespace Steinberg::Vst;

//------------------------------------------------------------------------
//  VST Plug-in Entry
//------------------------------------------------------------------------
// Windows: do not forget to include a .def file in your project to export
// GetPluginFactory function!
//------------------------------------------------------------------------

BEGIN_FACTORY_DEF(kVendorName, kVendorUrl, kVendorEmail)

    //---First Plug-in included in this factory-------
    // its kVstAudioEffectClass component
    DEF_CLASS2(INLINE_UID_FROM_FUID(kKhDetectorProcessorUID),
               PClassInfo::kManyInstances,              // cardinality
               kVstAudioEffectClass,                    // the component category (do not changed this)
               stringPluginName,                        // here the Plug-in name (to be changed)
               Vst::kDistributable,                     // the component flag
               kPluginCategory,                         // Subcategory for this Plug-in (to be changed)
               FULL_VERSION_STR,                        // Plug-in version (to be changed)
               kVstVersionString,                       // the VST 3 SDK version (do not changed this, use always this define)
               KhDetectorProcessor::createInstance)     // function pointer called when this component should be instantiated

    // its kVstComponentControllerClass component
    DEF_CLASS2(INLINE_UID_FROM_FUID(kKhDetectorControllerUID),
               PClassInfo::kManyInstances,              // cardinality
               kVstComponentControllerClass,            // the Controller category (do not changed this)
               stringPluginName "Controller",           // controller name (could be the same than component name)
               0,                                       // not used here
               "",                                      // not used here
               FULL_VERSION_STR,                        // Plug-in version (to be changed)
               kVstVersionString,                       // the VST 3 SDK version (do not changed this, use always this define)
               KhDetectorController::createInstance)    // function pointer called when this component should be instantiated

END_FACTORY 