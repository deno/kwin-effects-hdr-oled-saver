// SPDX-FileCopyrightText: 2026 deno
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Config UI for the oledpanel KWin effect (id: kwin_oledpanel_config).
// Wired to the effect via "X-KDE-ConfigModule" in the effect metadata:
// System Settings → Desktop Effects shows a configure button for the effect.
#pragma once

#include <KCModule>

class QSlider;
class QSpinBox;
class QLabel;
class QDoubleSpinBox;
class QCheckBox;
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

    QSlider *m_brightness = nullptr;
    QLabel *m_brightnessValue = nullptr;
    QSpinBox *m_delay = nullptr;
    QDoubleSpinBox *m_transition = nullptr;
    QCheckBox *m_hdrOnly = nullptr;
    QCheckBox *m_alwaysDim = nullptr;

    static constexpr double kDefaultFactor = 0.45;
    static constexpr int kDefaultTimeoutMs = 0;
    static constexpr int kDefaultFadeMs = 200;
    static constexpr bool kDefaultHdrOnly = true;
    static constexpr bool kDefaultAlwaysDim = false;
};
