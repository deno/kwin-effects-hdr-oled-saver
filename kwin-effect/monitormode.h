// SPDX-FileCopyrightText: 2026 deno
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "oledpanelsettings.h"

#include <KConfigGroup>

namespace KWin
{

// Honor the previous switch until the user saves the new monitor settings.
inline int effectiveMonitorMode(const OledPanelSettings &settings)
{
    const auto group = settings.config()->group(settings.currentGroup());
    if (!group.hasKey("MonitorMode") && !group.readEntry("HdrOnly", true)) {
        return OledPanelSettings::EnumMonitorMode::All;
    }
    return settings.monitorMode();
}

inline void clearLegacyMonitorMode(OledPanelSettings &settings)
{
    auto group = settings.config()->group(settings.currentGroup());
    if (group.hasKey("HdrOnly")) {
        group.deleteEntry("HdrOnly");
    }
}

} // namespace KWin
