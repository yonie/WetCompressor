//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "modeswitch.h"
#include "compengine.h"

#include <cstdio>
#include <cstdlib>

using namespace VSTGUI;

namespace Yonie {

namespace {

// "a,b,c;a,b,c" -> vector of vectors of double. Small and local on purpose:
// the only producer of these strings is the asset script that also draws the
// panel, so there is nothing to be lenient about.
std::vector<std::vector<double>> parseGroups(const std::string& spec)
{
    std::vector<std::vector<double>> out;
    std::vector<double> cur;
    const char* p = spec.c_str();
    while (*p)
    {
        char* end = nullptr;
        const double v = std::strtod(p, &end);
        if (end == p)
        {
            if (*p == ';')
            {
                if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            }
            ++p;
            continue;
        }
        cur.push_back(v);
        p = end;
    }
    if (!cur.empty())
        out.push_back(cur);
    return out;
}

} // namespace

//------------------------------------------------------------------------
ModeSwitch::ModeSwitch(const CRect& size)
: CControl(size, nullptr, -1)
{
    setWantsFocus(false);
}

//------------------------------------------------------------------------
void ModeSwitch::setButtonRects(const std::string& spec)
{
    buttonsSpec = spec;
    buttons.clear();
    for (const auto& g : parseGroups(spec))
        if (g.size() >= 4)
            buttons.emplace_back(g[0], g[1], g[2], g[3]);
}

//------------------------------------------------------------------------
void ModeSwitch::setLEDs(const std::string& spec)
{
    ledsSpec = spec;
    leds.clear();
    for (const auto& g : parseGroups(spec))
        if (g.size() >= 3)
            leds.push_back({g[0], g[1], g[2]});
}

//------------------------------------------------------------------------
int ModeSwitch::selectedIndex() const
{
    const int count = CompRange::kModeCount;
    int idx = static_cast<int>(getValueNormalized() * (count - 1) + 0.5f);
    if (idx < 0) idx = 0;
    if (idx > count - 1) idx = count - 1;
    return idx;
}

//------------------------------------------------------------------------
void ModeSwitch::draw(CDrawContext* context)
{
    const CPoint origin(getViewSize().left, getViewSize().top);
    const int sel = selectedIndex();

    // The selected button reads as pressed: a soft dark wash across it, no
    // outline. An outline over painted art looks like a UI element sitting on
    // top of a photograph, which is exactly what it would be.
    if (sel >= 0 && sel < static_cast<int>(buttons.size()))
    {
        CRect r = buttons[static_cast<size_t>(sel)];
        r.offset(origin.x, origin.y);
        context->setFillColor(CColor(0, 0, 0, 58));
        context->drawRect(r, kDrawFilled);
    }

    // LEDs. Glow rings first, so the lens lands on top of its own spill.
    for (size_t i = 0; i < leds.size(); ++i)
    {
        const LED& l = leds[i];
        // Exactly one is lit: the three buttons are one radio group, and the
        // switch has no off position - a compressor is always in one of its
        // three timings.
        const bool lit = static_cast<int>(i) == sel;

        CRect lens(l.cx - l.r, l.cy - l.r, l.cx + l.r, l.cy + l.r);
        lens.offset(origin.x, origin.y);

        if (lit)
        {
            CColor glow = ledLit;
            for (int s = glowSpread; s >= 1; --s)
            {
                CRect halo = lens;
                halo.extend(static_cast<CCoord>(s), static_cast<CCoord>(s));
                glow.alpha = static_cast<uint8_t>(glowAlpha * (glowSpread - s + 1)
                                                  / (glowSpread + 1));
                context->setFillColor(glow);
                context->drawEllipse(halo, kDrawFilled);
            }
        }

        context->setFillColor(lit ? ledLit : ledDark);
        context->drawEllipse(lens, kDrawFilled);

        // A small off-centre highlight, up and left with the panel's own light.
        // Without it the lens is a flat disc and reads as a printed dot.
        if (lit)
        {
            CRect spot(lens);
            spot.inset(l.r * 0.52, l.r * 0.52);
            spot.offset(-l.r * 0.22, -l.r * 0.24);
            context->setFillColor(CColor(255, 255, 255, 150));
            context->drawEllipse(spot, kDrawFilled);
        }
    }

    setDirty(false);
}

//------------------------------------------------------------------------
CMouseEventResult ModeSwitch::onMouseDown(CPoint& where, const CButtonState& buttonState)
{
    if (!buttonState.isLeftButton())
        return kMouseEventNotHandled;

    CPoint local(where);
    local.offset(-getViewSize().left, -getViewSize().top);

    for (size_t i = 0; i < buttons.size(); ++i)
    {
        if (!buttons[i].pointInside(local))
            continue;

        const int count = CompRange::kModeCount;
        beginEdit();
        setValueNormalized(count > 1 ? static_cast<float>(i) / (count - 1) : 0.f);
        valueChanged();
        endEdit();
        invalid();
        return kMouseEventHandled;
    }

    // The gaps between the buttons are dead panel, and clicking dead panel
    // should do nothing at all.
    return kMouseEventNotHandled;
}

//------------------------------------------------------------------------
} // namespace Yonie
