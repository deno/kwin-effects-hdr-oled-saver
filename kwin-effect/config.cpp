// SPDX-FileCopyrightText: 2026 deno
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>

#include <QCheckBox>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>

#include <cstring>

OledPanelConfig::OledPanelConfig(QObject *parent, const KPluginMetaData &metaData)
    : KCModule(parent, metaData)
    , m_settings(KSharedConfig::openConfig(QStringLiteral("kwinrc")))
{
    auto *layout = new QFormLayout(widget());
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setHorizontalSpacing(16);
    layout->setVerticalSpacing(12);

    m_brightness = new QSlider(Qt::Horizontal, widget());
    m_brightness->setRange(qRound(m_settings.dimFactorItem()->minValue().toDouble() * 100),
                           qRound(m_settings.dimFactorItem()->maxValue().toDouble() * 100));
    m_brightness->setSingleStep(5);
    m_brightness->setPageStep(10);
    m_brightnessValue = new QLabel(widget());
    auto *brightnessRow = new QWidget(widget());
    auto *brightnessLayout = new QHBoxLayout(brightnessRow);
    brightnessLayout->setContentsMargins(0, 0, 0, 0);
    brightnessLayout->setSpacing(10);
    brightnessLayout->addWidget(m_brightness, 1);
    brightnessLayout->addWidget(m_brightnessValue);
    layout->addRow(i18n("Minimum brightness:"), brightnessRow);

    m_delay = new QSpinBox(widget());
    m_delay->setRange(m_settings.timeoutMsItem()->minValue().toInt() / 1000,
                      m_settings.timeoutMsItem()->maxValue().toInt() / 1000);
    m_delay->setSuffix(i18n(" s"));
    layout->addRow(i18n("Dim after:"), m_delay);

    m_transition = new QDoubleSpinBox(widget());
    m_transition->setRange(m_settings.fadeMsItem()->minValue().toInt() / 1000.0,
                           m_settings.fadeMsItem()->maxValue().toInt() / 1000.0);
    m_transition->setDecimals(1);
    m_transition->setSingleStep(0.5);
    m_transition->setSuffix(i18n(" s"));
    layout->addRow(i18n("Dimming time:"), m_transition);

    m_alwaysDim = new QCheckBox(i18n("Keep panel dimmed at all times"), widget());
    m_alwaysDim->setToolTip(i18n("Do not restore full brightness when hovering, touching, or clicking panels."));
    layout->addRow(QString(), m_alwaysDim);

    m_hdrOnly = new QCheckBox(i18n("Only affect HDR outputs"), widget());
    m_hdrOnly->setToolTip(i18n("Leave panels on SDR displays at full brightness."));
    layout->addRow(QString(), m_hdrOnly);

    connect(m_brightness, &QSlider::valueChanged, this, [this] {
        refreshLabels();
        markDirty();
    });
    connect(m_delay, &QSpinBox::valueChanged, this, [this](int) {
        markDirty();
    });
    connect(m_transition, &QDoubleSpinBox::valueChanged, this, [this](double) {
        markDirty();
    });
    connect(m_hdrOnly, &QCheckBox::toggled, this, [this](bool) {
        markDirty();
    });
    connect(m_alwaysDim, &QCheckBox::toggled, this, [this] {
        updateDelayEnabled();
        markDirty();
    });
    load();

    // The Desktop Effects page can instantiate this module twice for one request.
    // Keep the repository's existing duplicate-page guard.
    if (QObject *p = QObject::parent()) {
        for (QObject *sibling : p->children()) {
            if (sibling != this && qobject_cast<OledPanelConfig *>(sibling)) {
                widget()->hide();
                break;
            }
        }
    }
}

void OledPanelConfig::refreshLabels()
{
    m_brightnessValue->setText(i18nc("@label minimum brightness percent", "%1%", m_brightness->value()));
}

void OledPanelConfig::updateDelayEnabled()
{
    const bool active = !m_alwaysDim->isChecked();
    m_delay->setEnabled(active);
    if (auto *form = qobject_cast<QFormLayout *>(widget()->layout())) {
        if (auto *label = form->labelForField(m_delay)) {
            label->setEnabled(active);
        }
    }
}

void OledPanelConfig::markDirty()
{
    setNeedsSave(true);
    setRepresentsDefaults(isDefault());
}

bool OledPanelConfig::isDefault() const
{
    return m_brightness->value() == qRound(m_settings.defaultDimFactorValue() * 100)
        && m_delay->value() == m_settings.defaultTimeoutMsValue() / 1000
        && qAbs(m_transition->value() * 1000.0 - m_settings.defaultFadeMsValue()) < 50.0
        && m_hdrOnly->isChecked() == m_settings.defaultHdrOnlyValue()
        && m_alwaysDim->isChecked() == m_settings.defaultAlwaysDimValue();
}

void OledPanelConfig::load()
{
    m_settings.load();
    {
        const QSignalBlocker blocker(m_brightness);
        m_brightness->setValue(qRound(m_settings.dimFactor() * 100));
    }
    {
        const QSignalBlocker blocker(m_delay);
        m_delay->setValue(m_settings.timeoutMs() / 1000);
    }
    {
        const QSignalBlocker blocker(m_transition);
        m_transition->setValue(m_settings.fadeMs() / 1000.0);
    }
    {
        const QSignalBlocker blocker(m_hdrOnly);
        m_hdrOnly->setChecked(m_settings.hdrOnly());
    }
    {
        const QSignalBlocker blocker(m_alwaysDim);
        m_alwaysDim->setChecked(m_settings.alwaysDim());
    }
    updateDelayEnabled();
    refreshLabels();
    setNeedsSave(false);
    setRepresentsDefaults(isDefault());
}

void OledPanelConfig::save()
{
    m_settings.setDimFactor(m_brightness->value() / 100.0);
    m_settings.setTimeoutMs(m_delay->value() * 1000);
    m_settings.setFadeMs(qRound(m_transition->value() * 1000.0));
    m_settings.setHdrOnly(m_hdrOnly->isChecked());
    m_settings.setAlwaysDim(m_alwaysDim->isChecked());
    if (!m_settings.save()) {
        setNeedsSave(true);
        return;
    }

    setNeedsSave(false);
    setRepresentsDefaults(isDefault());

    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("/Effects"),
                                                          QStringLiteral("org.kde.kwin.Effects"),
                                                          QStringLiteral("reconfigureEffect"));
    message.setArguments({QStringLiteral("oledpanel")});
    QDBusConnection::sessionBus().asyncCall(message);
}

void OledPanelConfig::defaults()
{
    m_brightness->setValue(qRound(m_settings.defaultDimFactorValue() * 100));
    m_delay->setValue(m_settings.defaultTimeoutMsValue() / 1000);
    m_transition->setValue(m_settings.defaultFadeMsValue() / 1000.0);
    m_hdrOnly->setChecked(m_settings.defaultHdrOnlyValue());
    m_alwaysDim->setChecked(m_settings.defaultAlwaysDimValue());
    updateDelayEnabled();
    setRepresentsDefaults(true);
}

class OledPanelConfigFactory : public KPluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID KPluginFactory_iid FILE "config-metadata.json")
    Q_INTERFACES(KPluginFactory)

public:
    explicit OledPanelConfigFactory() = default;
    ~OledPanelConfigFactory() override = default;

    QObject *create(const char *iface, QWidget *parentWidget, QObject *parent, const QVariantList &args) override
    {
        Q_UNUSED(args);
        if (!iface || strcmp(iface, "KCModule") != 0) {
            return nullptr;
        }
        QObject *actualParent = parentWidget ? static_cast<QObject *>(parentWidget) : parent;
        return new OledPanelConfig(actualParent, KPluginMetaData());
    }
};

#include "config.moc"
