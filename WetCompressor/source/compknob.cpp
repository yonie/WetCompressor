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
}

//------------------------------------------------------------------------
void CompKnob::setStepCount(int count)
{
    if (count > 1)
        stepCount = count;
}

//------------------------------------------------------------------------
float CompKnob::snap(float normalized) const
{
    if (stepCount <= 1)
        return 0.0f;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    const int idx = static_cast<int>(normalized * (stepCount - 1) + 0.5f);
    return static_cast<float>(idx) / (stepCount - 1);
}

//------------------------------------------------------------------------
int CompKnob::currentStep() const
{
    if (stepCount <= 1)
        return 0;
    int idx = static_cast<int>(getValueNormalized() * (stepCount - 1) + 0.5f);
    if (idx < 0) idx = 0;
    if (idx > stepCount - 1) idx = stepCount - 1;
    return idx;
}

//------------------------------------------------------------------------
// Every route into the control lands here, so the value cannot be left between
// two detents by anything - not the host, not automation, not a stray drag.
void CompKnob::setValue(float val)
{
    const float range = getMax() - getMin();
    const float norm = range != 0.0f ? (val - getMin()) / range : 0.0f;
    CAnimKnob::setValue(getMin() + snap(norm) * range);
}

//------------------------------------------------------------------------
// Move by whole detents. The change is reported because the VALUE changed, not
// because the view happens to be marked dirty.
void CompKnob::nudge(int detents)
{
    if (detents == 0 || stepCount <= 1)
        return;

    int idx = currentStep() + detents;
    if (idx < 0) idx = 0;
    if (idx > stepCount - 1) idx = stepCount - 1;

    const float want = static_cast<float>(idx) / (stepCount - 1);
    if (want == getValueNormalized())
        return;

    setValueNormalized(want);
    invalid();
    valueChanged();
}

//------------------------------------------------------------------------
CMouseEventResult CompKnob::onMouseDown(CPoint& where, const CButtonState& buttons)
{
    if (!buttons.isLeftButton())
        return kMouseEventNotHandled;

    // Ctrl-click or a double click returns the knob to its default, which for
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
    startStep = currentStep();
    beginEdit();
    return kMouseEventHandled;
}

//------------------------------------------------------------------------
CMouseEventResult CompKnob::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
    if (!dragging || !buttons.isLeftButton())
        return kMouseEventNotHandled;

    // Vertical, with horizontal counting the same way so a diagonal drag does
    // something sensible rather than nothing. Measured from where the drag
    // STARTED, not from the last position, so the knob cannot walk away from
    // the pointer over a long gesture.
    const double dy = startPoint.y - where.y;
    const double dx = where.x - startPoint.x;
    const int detents = static_cast<int>(std::lround((dy + dx) / kPixelsPerStep));

    int idx = startStep + detents;
    if (idx < 0) idx = 0;
    if (idx > stepCount - 1) idx = stepCount - 1;

    const float want = static_cast<float>(idx) / (stepCount - 1);
    if (want != getValueNormalized())
    {
        setValueNormalized(want);
        invalid();
        valueChanged();
    }
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
    // One notch, one detent. VSTGUI's default is a tenth of the range, which on
    // an eleven-position knob is an inconsistent number of steps.
    //
    // The whole gesture is wrapped in one edit rather than leaning on VSTGUI's
    // half-second end-edit timer: that timer ends the edit from a callback, and
    // anything the host does in response lands after the last notch has already
    // been reported.
    const int detents = event.deltaY > 0 ? 1 : (event.deltaY < 0 ? -1 : 0);
    if (detents != 0)
    {
        beginEdit();
        nudge(detents);
        endEdit();
    }
    event.consumed = true;
}

//------------------------------------------------------------------------
void CompKnob::onKeyboardEvent(KeyboardEvent& event)
{
    if (event.type != EventType::KeyDown)
        return;

    int detents = 0;
    switch (event.virt)
    {
        case VirtualKey::Up:
        case VirtualKey::Right: detents = 1; break;
        case VirtualKey::Down:
        case VirtualKey::Left:  detents = -1; break;
        default: return;
    }

    beginEdit();
    nudge(detents);
    endEdit();
    event.consumed = true;
}

//------------------------------------------------------------------------
} // namespace Yonie
