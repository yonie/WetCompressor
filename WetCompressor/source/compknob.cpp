//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "compknob.h"

#include <algorithm>
#include <cmath>

using namespace VSTGUI;

namespace Yonie {

//------------------------------------------------------------------------
CompKnob::CompKnob(const CRect& size)
: CAnimKnob(size, nullptr, -1, nullptr)
{
    // One filmstrip frame per notch. The strip is authored with 61 frames
    // across the sweep, so this is the finest step the artwork can show and
    // every notch moves the pointer by exactly one drawn position.
    setWheelInc(1.0f / 60.0f);
}

//------------------------------------------------------------------------
// Clamp, apply, and report - in that order, every time.
//
// The reporting is NOT conditional on the view being dirty. VSTGUI's knob only
// calls valueChanged() when isDirty() happens to be set, which is a drawing
// question standing in for a value question; when the two disagree the host
// keeps the previous value and the control snaps back to it the moment anything
// re-syncs it. That is the "scroll all the way down and it jumps" report.
void CompKnob::applyNormalized(float v)
{
    if (v < 0.0f) v = 0.0f;
    else if (v > 1.0f) v = 1.0f;

    const float before = getValueNormalized();
    setValueNormalized(v);
    if (getValueNormalized() != before)
    {
        invalid();
        valueChanged();
    }
}

//------------------------------------------------------------------------
CMouseEventResult CompKnob::onMouseDown(CPoint& where, const CButtonState& buttons)
{
    if (!buttons.isLeftButton())
        return kMouseEventNotHandled;

    // Ctrl-click (or a double click) returns the knob to its default, which for
    // both of these is 0 dB at twelve o'clock. Same gesture every other plugin
    // uses, so it needs no explaining.
    if (buttons & kControl || buttons.isDoubleClick())
    {
        beginEdit();
        setValue(getDefaultValue());
        invalid();
        valueChanged();
        endEdit();
        return kMouseEventHandled;
    }

    dragging = true;
    startPoint = where;
    startValue = getValueNormalized();
    beginEdit();
    return kMouseEventHandled;
}

//------------------------------------------------------------------------
CMouseEventResult CompKnob::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
    if (!dragging || !buttons.isLeftButton())
        return kMouseEventNotHandled;

    // Vertical, with horizontal counting the same way so a diagonal drag does
    // something sensible rather than nothing.
    const double dy = startPoint.y - where.y;
    const double dx = where.x - startPoint.x;
    float range = kDragRange;
    if (buttons & kShift)
        range *= kFineFactor;

    applyNormalized(startValue + static_cast<float>((dy + dx) / range));
    return kMouseEventHandled;
}

//------------------------------------------------------------------------
CMouseEventResult CompKnob::onMouseUp(CPoint& /*where*/, const CButtonState& /*buttons*/)
{
    if (dragging)
    {
        dragging = false;
        endEdit();
    }
    return kMouseEventHandled;
}

//------------------------------------------------------------------------
CMouseEventResult CompKnob::onMouseCancel()
{
    if (dragging)
    {
        dragging = false;
        endEdit();
    }
    return kMouseEventHandled;
}

//------------------------------------------------------------------------
void CompKnob::onMouseWheelEvent(MouseWheelEvent& event)
{
    // Wrap the whole gesture in one edit rather than leaning on VSTGUI's
    // half-second timer: the timer ends the edit from a callback, and anything
    // the host does in response to that end lands after the last notch has
    // already been reported. Begin here, end here, nothing in flight.
    float inc = getWheelInc();
    if (buttonStateFromEventModifiers(event.modifiers) & kShift)
        inc /= kFineFactor;

    const float v = getValueNormalized() + static_cast<float>(event.deltaY) * inc;

    beginEdit();
    applyNormalized(v);
    endEdit();

    event.consumed = true;
}

//------------------------------------------------------------------------
} // namespace Yonie
