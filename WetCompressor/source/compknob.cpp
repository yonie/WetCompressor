//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "compknob.h"

#include <algorithm>
#include <cmath>

using namespace VSTGUI;

namespace Yonie {

namespace {

// Shift. VSTGUI already calls this the zoom modifier and uses it to slow a
// drag, so reusing it means one key does one thing: finer.
inline bool isFine(const CButtonState& buttons)
{
    return (buttons & CControl::kZoomModifier) != 0;
}

inline bool isFine(const Modifiers& modifiers)
{
    return modifiers.has(ModifierKey::Shift);
}

} // namespace

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
void CompKnob::setCoarseStep(int count)
{
    coarseStep = count > 1 ? count : 1;
}

//------------------------------------------------------------------------
// Nearest index on the grid currently in force. coarseStep divides the number
// of gaps exactly, so both ends of the travel are on the coarse grid too.
int CompKnob::snapIndex(int index, bool fine) const
{
    const int gaps = stepCount - 1;
    const int unit = fine ? 1 : coarseStep;
    int snapped = ((index + unit / 2) / unit) * unit;
    if (index < 0) snapped = 0;
    if (snapped < 0) snapped = 0;
    if (snapped > gaps) snapped = gaps;
    return snapped;
}

//------------------------------------------------------------------------
float CompKnob::snap(float normalized, bool fine) const
{
    if (stepCount <= 1)
        return 0.0f;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    const int idx = snapIndex(
        static_cast<int>(normalized * (stepCount - 1) + 0.5f), fine);
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
    // Always the FINE grid here, never the coarse one. This is the path the
    // host and an automation lane write in through, and quantising someone
    // else's value to our coarse grid would throw away a fine setting the
    // moment anything touched the knob. The coarse feel belongs to the input
    // handlers below, not to the value.
    CAnimKnob::setValue(getMin() + snap(norm, true) * range);
}

//------------------------------------------------------------------------
// Move by whole detents. The change is reported because the VALUE changed, not
// because the view happens to be marked dirty.
void CompKnob::nudge(int detents, bool fine)
{
    if (detents == 0 || stepCount <= 1)
        return;

    // Snap to the active grid FIRST, so a knob left between coarse detents by
    // a Shift move steps onto the grid rather than carrying the offset with it
    // for ever.
    const int unit = fine ? 1 : coarseStep;
    int idx = snapIndex(snapIndex(currentStep(), fine) + detents * unit, fine);
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
    dragFine = isFine(buttons);
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

    // Pressing or releasing Shift part way through re-bases the gesture on the
    // spot: the drag counts detents from where it started, and a detent just
    // changed size, so keeping the old origin would jump the knob.
    const bool fine = isFine(buttons);
    if (fine != dragFine)
    {
        dragFine = fine;
        startPoint = where;
        startStep = currentStep();
    }

    // Vertical, with horizontal counting the same way so a diagonal drag does
    // something sensible rather than nothing. Measured from where the drag
    // STARTED, not from the last position, so the knob cannot walk away from
    // the pointer over a long gesture.
    //
    // The pixels per notch do not change with the modifier; what a notch MEANS
    // does. Shift therefore moves the knob a third as far for the same hand
    // travel, which is what a fine mode should feel like.
    const double dy = startPoint.y - where.y;
    const double dx = where.x - startPoint.x;
    const int detents = static_cast<int>(std::lround((dy + dx) / kPixelsPerStep));

    const int unit = fine ? 1 : coarseStep;
    int idx = snapIndex(startStep + detents * unit, fine);
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
        nudge(detents, isFine(event.modifiers));
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
    nudge(detents, isFine(event.modifiers));
    endEdit();
    event.consumed = true;
}

//------------------------------------------------------------------------
} // namespace Yonie
