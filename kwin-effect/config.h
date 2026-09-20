// SPDX-FileCopyrightText: 2026 deno
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "oledpanelsettings.h"

#include <KCModule>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QSlider;
class QSpinBox;

class OledPanelConfig : public KCModule
{
    Q_OBJECT

public:
    explicit OledPanelConfig(QObject *parent, const KPluginMetaData &metaData);

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

private:
    void refreshLabels();
    void markDirty();
    void updateDelayEnabled();
    bool isDefault() const;

    KWin::OledPanelSettings m_settings;
    QSlider *m_brightness = nullptr;
    QLabel *m_brightnessValue = nullptr;
    QSpinBox *m_delay = nullptr;
    QDoubleSpinBox *m_transition = nullptr;
    QCheckBox *m_hdrOnly = nullptr;
    QCheckBox *m_alwaysDim = nullptr;
};
