//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/events.h"

namespace Yonie {

//------------------------------------------------------------------------
// CompKnob - a stepped filmstrip knob that only rests on printed positions.
//
// Every WET control is a stepped encoder: coarse on purpose, so it forces a
// decision instead of inviting a 0.5 dB fiddle. Here that means:
//
//   * the value always snaps to a detent, however it was changed
//   * one mouse-wheel notch moves exactly one detent
//   * dragging moves THROUGH the detents rather than sliding between them
//
// The filmstrip is authored with exactly one frame per position, so the frame
// index equals the step index and every pointer angle is exactly right - and
// because the panel prints eleven values around the arc, the pointer always
// ends up on a printed number.
//
// It subclasses CAnimKnob rather than reusing VSTGUI's mouse handling, for two
// reasons found in the first build:
//
//   1. CIRCULAR DRAG HAS A SEAM. In circular mode the value follows the angle
//      of the pointer around the centre, and the control watches for the
//      pointer crossing more than half the range in one move so it can decide
//      you meant to go the other way round. On a 277 degree sweep that leaves
//      an 83 degree wedge at the bottom where the question has no correct
//      answer, and an ordinary small movement there jumps the knob to the
//      opposite end. Dragging is vertical here, so the question never arises.
//
//   2. VSTGUI reports a change only when the VIEW happens to be marked dirty,
//      which is a drawing question standing in for a value question. When the
//      two disagree the host keeps the old value and the knob snaps back to it
//      the moment anything re-syncs. Here a change is reported because the
//      value changed.
//------------------------------------------------------------------------
class CompKnob : public VSTGUI::CAnimKnob
{
public:
    CompKnob(const VSTGUI::CRect& size);

    void setStepCount(int count);
    int getStepCount() const { return stepCount; }

    // overrides
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where,
                                          const VSTGUI::CButtonState& buttons) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where,
                                           const VSTGUI::CButtonState& buttons) override;
    VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where,
                                        const VSTGUI::CButtonState& buttons) override;
    VSTGUI::CMouseEventResult onMouseCancel() override;
    void onMouseWheelEvent(VSTGUI::MouseWheelEvent& event) override;
    void onKeyboardEvent(VSTGUI::KeyboardEvent& event) override;
    void setValue(float val) override;

    CLASS_METHODS(CompKnob, VSTGUI::CAnimKnob)

private:
    // Move by whole detents and report the change.
    void nudge(int detents);
    // Nearest detent to a normalised value, as a normalised value.
    float snap(float normalized) const;
    int currentStep() const;

    // Pixels of vertical drag per detent. 26 is far enough that a detent is a
    // deliberate move and close enough that the whole range is one hand's
    // travel: eleven positions across 260 px.
    static constexpr float kPixelsPerStep = 26.0f;

    int stepCount = 11;          // number of positions, not the number of gaps
    bool dragging = false;
    VSTGUI::CPoint startPoint;
    int startStep = 0;
};

//------------------------------------------------------------------------
} // namespace Yonie
