//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "wetcompprocessor.h"
#include "wetcompcontroller.h"
#include "wetcompcids.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

#define stringPluginName "WetCompressor"

using namespace Steinberg::Vst;
using namespace Yonie;

//------------------------------------------------------------------------
//  VST Plug-in Entry
//------------------------------------------------------------------------

BEGIN_FACTORY_DEF ("Yonie",
                   "https://github.com/yonie",
                   "mailto:contact@wetvst.com")

	DEF_CLASS2 (INLINE_UID_FROM_FUID(kWetCompProcessorUID),
				PClassInfo::kManyInstances,
				kVstAudioEffectClass,
				stringPluginName,
				Vst::kDistributable,
				WetCompVST3Category,
				FULL_VERSION_STR,
				kVstVersionString,
				WetCompProcessor::createInstance)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (kWetCompControllerUID),
				PClassInfo::kManyInstances,
				kVstComponentControllerClass,
				stringPluginName "Controller",
				0,
				"",
				FULL_VERSION_STR,
				kVstVersionString,
				WetCompController::createInstance)

END_FACTORY
