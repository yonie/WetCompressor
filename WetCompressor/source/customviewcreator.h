//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/uidescription/iviewcreator.h"
#include "vstgui/uidescription/uiviewfactory.h"
#include "vstgui/uidescription/uiattributes.h"
#include "vstgui/uidescription/iuidescription.h"

#include "ledmeterview.h"
#include "modeswitch.h"
#include "compknob.h"

namespace Yonie {

//------------------------------------------------------------------------
// LEDMeterViewCreator - same view as WetDelay/WetReverb, reused unchanged so
// the metering behaves identically across the line.
//------------------------------------------------------------------------
class LEDMeterViewCreator : public VSTGUI::ViewCreatorAdapter
{
public:
    LEDMeterViewCreator();

    VSTGUI::IdStringPtr getViewName() const override { return "LEDMeterView"; }
    VSTGUI::IdStringPtr getBaseViewName() const override { return "CControl"; }
    VSTGUI::UTF8StringPtr getDisplayName() const override { return "LED Meter View"; }

    VSTGUI::CView* create(const VSTGUI::UIAttributes& attributes,
                          const VSTGUI::IUIDescription* description) const override;
    bool apply(VSTGUI::CView* view, const VSTGUI::UIAttributes& attributes,
               const VSTGUI::IUIDescription* description) const override;
    bool getAttributeNames(StringList& attributeNames) const override;
    AttrType getAttributeType(const string& attributeName) const override;
    bool getAttributeValue(VSTGUI::CView* view, const string& attributeName,
                           string& stringValue,
                           const VSTGUI::IUIDescription* desc) const override;
};

//------------------------------------------------------------------------
// ModeSwitchCreator - the FAST / NORMAL / SLOW cluster.
//------------------------------------------------------------------------
class ModeSwitchCreator : public VSTGUI::ViewCreatorAdapter
{
public:
    ModeSwitchCreator();

    VSTGUI::IdStringPtr getViewName() const override { return "ModeSwitch"; }
    VSTGUI::IdStringPtr getBaseViewName() const override { return "CControl"; }
    VSTGUI::UTF8StringPtr getDisplayName() const override { return "Mode Switch"; }

    VSTGUI::CView* create(const VSTGUI::UIAttributes& attributes,
                          const VSTGUI::IUIDescription* description) const override;
    bool apply(VSTGUI::CView* view, const VSTGUI::UIAttributes& attributes,
               const VSTGUI::IUIDescription* description) const override;
    bool getAttributeNames(StringList& attributeNames) const override;
    AttrType getAttributeType(const string& attributeName) const override;
    bool getAttributeValue(VSTGUI::CView* view, const string& attributeName,
                           string& stringValue,
                           const VSTGUI::IUIDescription* desc) const override;
};

//------------------------------------------------------------------------
// CompKnobCreator - the continuous filmstrip knob. No attributes of its own;
// everything it needs comes from CAnimKnob and the bitmap.
//------------------------------------------------------------------------
class CompKnobCreator : public VSTGUI::ViewCreatorAdapter
{
public:
    CompKnobCreator();

    VSTGUI::IdStringPtr getViewName() const override { return "CompKnob"; }
    VSTGUI::IdStringPtr getBaseViewName() const override { return "CAnimKnob"; }
    VSTGUI::UTF8StringPtr getDisplayName() const override { return "Compressor Knob"; }

    VSTGUI::CView* create(const VSTGUI::UIAttributes& attributes,
                          const VSTGUI::IUIDescription* description) const override;
    bool apply(VSTGUI::CView* view, const VSTGUI::UIAttributes& attributes,
               const VSTGUI::IUIDescription* description) const override;
    bool getAttributeNames(StringList& attributeNames) const override;
    AttrType getAttributeType(const string& attributeName) const override;
    bool getAttributeValue(VSTGUI::CView* view, const string& attributeName,
                           string& stringValue,
                           const VSTGUI::IUIDescription* desc) const override;
};

//------------------------------------------------------------------------
void registerCustomViews();

//------------------------------------------------------------------------
} // namespace Yonie
