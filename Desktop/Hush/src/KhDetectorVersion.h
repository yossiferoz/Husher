#pragma once

#include "pluginterfaces/base/fplatform.h"

//------------------------------------------------------------------------
// Project Information
//------------------------------------------------------------------------
#define MAJOR_VERSION_STR "1"
#define MAJOR_VERSION_INT 1

#define SUB_VERSION_STR "0"
#define SUB_VERSION_INT 0

#define RELEASE_NUMBER_STR "0"
#define RELEASE_NUMBER_INT 0

#define BUILD_NUMBER_STR "1"
#define BUILD_NUMBER_INT 1

#define FULL_VERSION_STR MAJOR_VERSION_STR "." SUB_VERSION_STR "." RELEASE_NUMBER_STR "." BUILD_NUMBER_STR

// Unique plugin identifiers
// These GUIDs need to be unique for your plugin!
// You can generate new ones at: https://www.guidgenerator.com/
//------------------------------------------------------------------------
static const FUID kKhDetectorProcessorUID(0x12345678, 0x12345678, 0x12345678, 0x12345678);
static const FUID kKhDetectorControllerUID(0x87654321, 0x87654321, 0x87654321, 0x87654321);

//------------------------------------------------------------------------
#define kVendorName "YourCompany"
#define kVendorUrl "https://www.yourcompany.com"
#define kVendorEmail "contact@yourcompany.com"

#define kPluginName "KhDetector"
#define kPluginCategory "Fx" 