//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/events.h"

namespace Yonie {

//------------------------------------------------------------------------
// CompKnob - a continuous filmstrip knob that drags and scrolls the way a
// hardware pot behaves, with ends that are ends.
//
// It exists because VSTGUI's own knob has two problems here, both of which
// Ronald hit within a minute of the first build:
//
//   1. CIRCULAR DRAG HAS A SEAM. In circular mode the value follows the angle
//      of the pointer around the knob's centre, and the control watches for the
//      pointer crossing more than half the range in one move so it can decide
//      you meant to go the other way round. At the bottom of the travel that
//      test fires on an ordinary small movement and the knob jumps to the
//      opposite end. There is no way to tune it out - a circular control with
//      a 277 degree sweep has a 83 degree wedge at the bottom where "which way
//      did they mean" has no correct answer. Dragging is vertical here, so the
//      question never arises.
//
//   2. THE WHEEL WAS COARSE AND COULD LOSE ITS LAST STEP. VSTGUI's default
//      wheel increment is a tenth of the range - ten notches from -inf to +inf
//      on a knob whose whole job is fine level setting - and it only reports a
//      change when the view happens to be marked dirty, which is not the same
//      question as "did the value move". Here every notch is one filmstrip
//      frame, and a change is reported because the value changed.
//
// Everything else is CAnimKnob: the filmstrip, the bitmap, the drawing.
//------------------------------------------------------------------------
class CompKnob : public VSTGUI::CAnimKnob
{
public:
    CompKnob(const VSTGUI::CRect& size);

    // overrides
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where,
                                          const VSTGUI::CButtonState& buttons) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where,
                                           const VSTGUI::CButtonState& buttons) override;
    VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where,
                                        const VSTGUI::CButtonState& buttons) override;
    VSTGUI::CMouseEventResult onMouseCancel() override;
    void onMouseWheelEvent(VSTGUI::MouseWheelEvent& event) override;

    CLASS_METHODS(CompKnob, VSTGUI::CAnimKnob)

private:
    void applyNormalized(float v);

    // Pixels of vertical drag for the full sweep. 260 is about a hand's travel
    // and matches what the shipped plugins in this line feel like.
    static constexpr float kDragRange = 260.0f;
    // Shift drags eight times finer, which is what a mouse-only user needs to
    // land on a printed mark.
    static constexpr float kFineFactor = 8.0f;

    bool dragging = false;
    VSTGUI::CPoint startPoint;
    float startValue = 0.0f;
};

//------------------------------------------------------------------------
} // namespace Yonie
