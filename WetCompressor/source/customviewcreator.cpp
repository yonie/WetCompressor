//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "customviewcreator.h"
#include "vstgui/uidescription/uiviewcreator.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cframe.h"

using namespace VSTGUI;

namespace Yonie {

static const std::string kAttrNumSegments = "num-segments";
static const std::string kAttrSegmentGap  = "segment-gap";
static const std::string kAttrHorizontal  = "horizontal";
static const std::string kAttrInverted    = "inverted";
static const std::string kAttrReduction   = "reduction-scheme";
static const std::string kAttrButtons     = "button-rects";
static const std::string kAttrLEDs        = "led-spots";

//------------------------------------------------------------------------
// LEDMeterViewCreator
//------------------------------------------------------------------------
LEDMeterViewCreator::LEDMeterViewCreator()
{
    UIViewFactory::registerViewCreator(*this);
}

CView* LEDMeterViewCreator::create(const UIAttributes&, const IUIDescription*) const
{
    return new LEDMeterView(CRect(0, 0, 20, 200));
}

bool LEDMeterViewCreator::apply(CView* view, const UIAttributes& attributes,
                                const IUIDescription*) const
{
    auto* meter = dynamic_cast<LEDMeterView*>(view);
    if (!meter)
        return false;

    int32_t intValue;
    if (attributes.getIntegerAttribute(kAttrNumSegments, intValue))
        meter->setNumSegments(intValue);

    double doubleValue;
    if (attributes.getDoubleAttribute(kAttrSegmentGap, doubleValue))
        meter->setSegmentGap(static_cast<CCoord>(doubleValue));

    bool boolValue;
    if (attributes.getBooleanAttribute(kAttrHorizontal, boolValue))
        meter->setHorizontal(boolValue);
    if (attributes.getBooleanAttribute(kAttrInverted, boolValue))
        meter->setInverted(boolValue);
    if (attributes.getBooleanAttribute(kAttrReduction, boolValue))
        meter->setReductionScheme(boolValue);

    return true;
}

bool LEDMeterViewCreator::getAttributeNames(StringList& names) const
{
    names.emplace_back(kAttrNumSegments);
    names.emplace_back(kAttrSegmentGap);
    names.emplace_back(kAttrHorizontal);
    names.emplace_back(kAttrInverted);
    names.emplace_back(kAttrReduction);
    return true;
}

IViewCreator::AttrType LEDMeterViewCreator::getAttributeType(const string& name) const
{
    if (name == kAttrNumSegments) return kIntegerType;
    if (name == kAttrSegmentGap)  return kFloatType;
    if (name == kAttrHorizontal)  return kBooleanType;
    if (name == kAttrInverted)    return kBooleanType;
    if (name == kAttrReduction)   return kBooleanType;
    return kUnknownType;
}

bool LEDMeterViewCreator::getAttributeValue(CView* view, const string& name,
                                            string& value, const IUIDescription*) const
{
    auto* meter = dynamic_cast<LEDMeterView*>(view);
    if (!meter)
        return false;
    if (name == kAttrNumSegments) { value = "12";    return true; }
    if (name == kAttrSegmentGap)  { value = "2";     return true; }
    if (name == kAttrHorizontal)  { value = "false"; return true; }
    if (name == kAttrInverted)    { value = meter->isInverted() ? "true" : "false"; return true; }
    if (name == kAttrReduction)   { value = meter->isReductionScheme() ? "true" : "false"; return true; }
    return false;
}

//------------------------------------------------------------------------
// ModeSwitchCreator
//------------------------------------------------------------------------
ModeSwitchCreator::ModeSwitchCreator()
{
    UIViewFactory::registerViewCreator(*this);
}

CView* ModeSwitchCreator::create(const UIAttributes&, const IUIDescription*) const
{
    return new ModeSwitch(CRect(0, 0, 100, 100));
}

bool ModeSwitchCreator::apply(CView* view, const UIAttributes& attributes,
                              const IUIDescription*) const
{
    auto* sw = dynamic_cast<ModeSwitch*>(view);
    if (!sw)
        return false;

    if (auto* s = attributes.getAttributeValue(kAttrButtons))
        sw->setButtonRects(*s);
    if (auto* s = attributes.getAttributeValue(kAttrLEDs))
        sw->setLEDs(*s);

    return true;
}

bool ModeSwitchCreator::getAttributeNames(StringList& names) const
{
    names.emplace_back(kAttrButtons);
    names.emplace_back(kAttrLEDs);
    return true;
}

IViewCreator::AttrType ModeSwitchCreator::getAttributeType(const string& name) const
{
    if (name == kAttrButtons) return kStringType;
    if (name == kAttrLEDs)    return kStringType;
    return kUnknownType;
}

bool ModeSwitchCreator::getAttributeValue(CView* view, const string& name,
                                          string& value, const IUIDescription*) const
{
    auto* sw = dynamic_cast<ModeSwitch*>(view);
    if (!sw)
        return false;
    if (name == kAttrButtons) { value = sw->buttonSpec(); return true; }
    if (name == kAttrLEDs)    { value = sw->ledSpec();    return true; }
    return false;
}

//------------------------------------------------------------------------
// CompKnobCreator
//------------------------------------------------------------------------
CompKnobCreator::CompKnobCreator()
{
    UIViewFactory::registerViewCreator(*this);
}

CView* CompKnobCreator::create(const UIAttributes&, const IUIDescription*) const
{
    return new CompKnob(CRect(0, 0, 60, 60));
}

// No apply(): UIViewFactory walks getBaseViewName() and runs the CAnimKnob and
// CControl creators for us, which is where the bitmap and the control tag come
// from. There is nothing left for this one to set.

//------------------------------------------------------------------------
void registerCustomViews()
{
    // Static so each creator registers exactly once, however many editor
    // windows the host opens.
    static LEDMeterViewCreator ledMeterCreator;
    static ModeSwitchCreator modeSwitchCreator;
    static CompKnobCreator compKnobCreator;

    // Belt as well as braces. CompKnob handles its own mouse and wheel, so this
    // no longer decides anything for the two knobs - but the frame-wide default
    // is still what any other knob-like control would inherit, and circular is
    // the wrong default for this panel.
    VSTGUI::CFrame::kDefaultKnobMode = VSTGUI::kLinearMode;
}

//------------------------------------------------------------------------
} // namespace Yonie
