// SPDX-FileCopyrightText: 2026 deno
// SPDX-License-Identifier: GPL-2.0-or-later

#include "oledpanel.h"

#include <cmath>
#include <iostream>

using namespace std::chrono_literals;

namespace
{

bool near(qreal actual, qreal expected, qreal tolerance = 0.0001)
{
    return std::abs(actual - expected) <= tolerance;
}

int fail(const char *message, qreal actual = 0, qreal expected = 0)
{
    std::cerr << message;
    if (actual != 0 || expected != 0) {
        std::cerr << ": actual=" << actual << " expected=" << expected;
    }
    std::cerr << '\n';
    return 1;
}

} // namespace

int main()
{
    {
        KWin::PanelState state;
        if (!state.transition.done() || !near(state.brightness(0.45), 1.0)) {
            return fail("fresh state must be settled bright", state.brightness(0.45), 1.0);
        }
        state.setTarget(true, 200);
        state.transition.advance(0ns);
        state.transition.advance(100ms);
        if (!near(state.brightness(0.45), 0.725)) {
            return fail("half fade must use normalized progress for factor 0.45", state.brightness(0.45), 0.725);
        }
        if (!near(state.brightness(0.10), 0.55)) {
            return fail("half fade must use normalized progress for factor 0.10", state.brightness(0.10), 0.55);
        }
        state.transition.advance(200ms);
        if (!near(state.brightness(0.45), 0.45) || !near(state.brightness(0.10), 0.10)) {
            return fail("full fade must reach the configured endpoint", state.brightness(0.45), 0.45);
        }
    }

    {
        KWin::PanelState state;
        state.setTarget(true, 400);
        state.transition.advance(0ns);
        state.transition.advance(100ms);
        const qreal before = state.brightness(0.45);
        state.setTarget(false, 25);
        const qreal after = state.brightness(0.45);
        if (!near(after, before, 0.005)) {
            return fail("duration-changing reversal must preserve brightness", after, before);
        }
        state.transition.advance(300ms);
        if (!near(state.brightness(0.45), 1.0)) {
            return fail("partial reversal must converge to bright", state.brightness(0.45), 1.0);
        }
    }

    {
        KWin::PanelState beforeFirstFrame;
        beforeFirstFrame.setTarget(true, 200);
        beforeFirstFrame.setTarget(false, 200);
        beforeFirstFrame.transition.advance(0ns);
        beforeFirstFrame.transition.advance(200ms);
        if (!near(beforeFirstFrame.brightness(0.45), 1.0)) {
            return fail("reversal before first frame must remain bright", beforeFirstFrame.brightness(0.45), 1.0);
        }

        KWin::PanelState settled;
        settled.setTarget(true, 100);
        settled.transition.advance(0ns);
        settled.transition.advance(100ms);
        if (!near(settled.brightness(0.45), 0.45)) {
            return fail("dimming must reach the settled endpoint", settled.brightness(0.45), 0.45);
        }
        settled.setTarget(false, 100);
        if (!near(settled.brightness(0.45), 0.45)) {
            return fail("reversing at dim endpoint must not jump", settled.brightness(0.45), 0.45);
        }
        settled.setTarget(true, 100);
        if (!near(settled.brightness(0.45), 0.45)) {
            return fail("reversing again at dim endpoint must not stick or jump", settled.brightness(0.45), 0.45);
        }
    }

    {
        KWin::PanelState state;
        state.setTarget(true, 0);
        if (!state.transition.done() || !near(state.brightness(0.45), 0.45)) {
            return fail("zero fade must settle dim immediately", state.brightness(0.45), 0.45);
        }
        state.setTarget(false, 0);
        state.transition.advance(0ns);
        state.transition.advance(100ms);
        if (!near(state.brightness(0.45), 0.725)) {
            return fail("restoration after zero fade must still take 200ms", state.brightness(0.45), 0.725);
        }
        state.transition.advance(200ms);
        if (!near(state.brightness(0.45), 1.0)) {
            return fail("restoration must finish bright", state.brightness(0.45), 1.0);
        }

        KWin::PanelState reconfigured;
        reconfigured.setTarget(true, 1000);
        reconfigured.transition.advance(0ns);
        reconfigured.transition.advance(100ms);
        reconfigured.setTarget(true, 0);
        if (!reconfigured.transition.done() || !near(reconfigured.brightness(0.45), 0.45)) {
            return fail("reconfiguring an active dim fade to zero must settle", reconfigured.brightness(0.45), 0.45);
        }
    }

    {
        KWin::PanelState state;
        state.setTarget(true, 400);
        state.transition.advance(0ns);
        state.transition.advance(100ms);
        const qreal frozen = state.brightness(0.45);
        state.closedBrightness = frozen;
        state.setTarget(false, 200);
        state.transition.advance(400ms);
        if (!near(state.brightness(0.10), frozen)) {
            return fail("closed brightness must ignore retarget and factor changes", state.brightness(0.10), frozen);
        }
    }

    return 0;
}
