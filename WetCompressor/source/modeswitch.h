//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/cdrawcontext.h"

#include <vector>

namespace Yonie {

//------------------------------------------------------------------------
// ModeSwitch - the FAST / NORMAL / SLOW cluster: three painted buttons with
// three indicator LEDs above them, behaving as one radio group.
//
// One view rather than six, for the reason WetDelay uses explicit per-button
// rects instead of a CSegmentButton: a segment control divides its own width
// evenly, and the painted buttons are not evenly divided. WetReverb shipped
// that bug for two releases - the left edge of every button selected the one
// before it. Here the rects come from the same script that draws the panel, so
// the clickable geometry cannot drift from the painted geometry, and the gaps
// between the buttons are genuinely dead.
//
// The view draws NOTHING over the buttons themselves except a soft press
// shading on the selected one; the button art is backplate. It draws the LEDs,
// because a lit LED has to be able to go out.
//------------------------------------------------------------------------
class ModeSwitch : public VSTGUI::CControl
{
public:
    ModeSwitch(const VSTGUI::CRect& size);

    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where,
                                          const VSTGUI::CButtonState& buttons) override;

    // "x0,y0,x1,y1;x0,y0,x1,y1;..." in view-local pixels.
    void setButtonRects(const std::string& spec);
    // "cx,cy,r;cx,cy,r;..." in view-local pixels.
    void setLEDs(const std::string& spec);

    const std::string& buttonSpec() const { return buttonsSpec; }
    const std::string& ledSpec() const { return ledsSpec; }

    CLASS_METHODS(ModeSwitch, VSTGUI::CControl)

private:
    int selectedIndex() const;

    struct LED { double cx, cy, r; };

    std::vector<VSTGUI::CRect> buttons;
    std::vector<LED> leds;
    std::string buttonsSpec, ledsSpec;

    // ONE colour for all three (Ronald, 2026-08-28). They are three positions
    // of one switch, not three different warnings, and colour-coding them makes
    // the panel say something it does not mean. The art paints FAST red and
    // NORMAL yellow, which is exactly that mistake.
    //
    // Amber, matching the GR strip, so the whole panel has one lamp colour.
    VSTGUI::CColor ledLit{255, 176, 26, 255};
    // Unlit is not black. An LED that is off is still a coloured lens with the
    // panel light on it, which is why a dead one reads as dark amber rather
    // than as a hole.
    VSTGUI::CColor ledDark{44, 31, 8, 255};

    int glowSpread = 5;
    uint8_t glowAlpha = 52;
};

//------------------------------------------------------------------------
} // namespace Yonie
