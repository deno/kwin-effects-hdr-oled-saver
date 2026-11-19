// SPDX-FileCopyrightText: 2026 deno
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "oledpanelsettings.h"

#include <KCModule>
#include <KScreen/Types>

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

class KMessageWidget;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QSlider;
class QSpinBox;

class OledPanelConfig : public KCModule
{
    Q_OBJECT

public:
    explicit OledPanelConfig(QObject *parent, const KPluginMetaData &metaData);
    ~OledPanelConfig() override;

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

private:
    struct Values {
        int brightness = 0;
        int delay = 0;
        int transitionTenths = 0;
        int monitorMode = 0;
        bool alwaysDim = false;
        QSet<QString> selectedMonitors;

        bool operator==(const Values &) const = default;
    };

    struct Monitor {
        QString name;
        QString uuid;
        bool enabled;

        bool operator==(const Monitor &) const = default;
    };

    enum class MonitorState {
        Loading,
        Ready,
        Error,
    };

    Values values() const;
    Values defaultValues() const;
    void showValues(const Values &values);
    void refreshLabels();
    void updateChangeState();
    void updateDelayEnabled();
    void startMonitorDiscovery();
    void refreshMonitors(MonitorState state);
    bool monitorSelectionValid() const;
    void setDialogOkEnabled(bool enabled);
    bool eventFilter(QObject *watched, QEvent *event) override;
    void updateMonitorList();

    KWin::OledPanelSettings m_settings;
    QSlider *m_brightness = nullptr;
    QLabel *m_brightnessValue = nullptr;
    QSpinBox *m_delay = nullptr;
    QDoubleSpinBox *m_transition = nullptr;
    QComboBox *m_monitorMode = nullptr;
    KMessageWidget *m_monitorError = nullptr;
    QLabel *m_monitorStatus = nullptr;
    QListWidget *m_monitorList = nullptr;
    QCheckBox *m_alwaysDim = nullptr;

    Values m_savedValues;
    QSet<QString> m_selectedMonitors;
    KScreen::ConfigPtr m_monitorConfig;
    MonitorState m_monitorState = MonitorState::Loading;
    QList<Monitor> m_connectedMonitors;
    QHash<QString, QString> m_knownMonitorNames;
};
