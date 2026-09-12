// SPDX-FileCopyrightText: 2026 deno
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <cstring>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
OledPanelConfig::OledPanelConfig(QObject *parent, const KPluginMetaData &metaData)
    : KCModule(parent, metaData)
{
    auto *layout = new QFormLayout(widget());
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setHorizontalSpacing(16);
    layout->setVerticalSpacing(12);
    m_brightness = new QSlider(Qt::Horizontal, widget());
    m_brightness->setRange(5, 100);
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
    m_delay->setRange(0, 3600);
    m_delay->setSuffix(i18n(" s"));
    layout->addRow(i18n("Dim after:"), m_delay);

    m_transition = new QDoubleSpinBox(widget());
    m_transition->setRange(0.0, 300.0);
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

    // The Desktop Effects page can fire requestConfigure twice for one
    // click, instantiating this module twice into the same dialog (both
    // widgets paint on top of each other). The second instance hides its
    // own page; both instances still load/save the same keys.
    if (QObject *p = QObject::parent()) {
        for (QObject *sib : p->children()) {
            if (sib != this && qobject_cast<OledPanelConfig *>(sib)) {
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
    return m_brightness->value() == qRound(kDefaultFactor * 100)
        && m_delay->value() == kDefaultTimeoutMs / 1000
        && qAbs(m_transition->value() * 1000.0 - kDefaultFadeMs) < 50.0
        && m_hdrOnly->isChecked() == kDefaultHdrOnly
        && m_alwaysDim->isChecked() == kDefaultAlwaysDim;
}

void OledPanelConfig::load()
{
    const KConfigGroup conf = KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Effect-oledpanel"));
    const double factor = qBound(0.05, conf.readEntry("DimFactor", kDefaultFactor), 1.0);
    const int timeoutMs = qBound(0, conf.readEntry("TimeoutMs", kDefaultTimeoutMs), 3600000);
    const int fadeMs = qBound(0, conf.readEntry("FadeMs", kDefaultFadeMs), 300000);
    {
        const QSignalBlocker blocker(m_brightness);
        m_brightness->setValue(qRound(factor * 100));
    }
    {
        const QSignalBlocker blocker(m_delay);
        m_delay->setValue(timeoutMs / 1000);
    }
    {
        const QSignalBlocker blocker(m_transition);
        m_transition->setValue(fadeMs / 1000.0);
    }
    {
        const QSignalBlocker blocker(m_hdrOnly);
        m_hdrOnly->setChecked(conf.readEntry("HdrOnly", kDefaultHdrOnly));
    }
    {
        const QSignalBlocker blocker(m_alwaysDim);
        m_alwaysDim->setChecked(conf.readEntry("AlwaysDim", kDefaultAlwaysDim));
    }
    updateDelayEnabled();
    refreshLabels();
    setNeedsSave(false);
    setRepresentsDefaults(isDefault());
}

void OledPanelConfig::save()
{
    KConfigGroup conf = KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Effect-oledpanel"));
    conf.writeEntry("DimFactor", m_brightness->value() / 100.0);
    conf.writeEntry("TimeoutMs", m_delay->value() * 1000);
    conf.writeEntry("FadeMs", qRound(m_transition->value() * 1000.0));
    conf.writeEntry("HdrOnly", m_hdrOnly->isChecked());
    conf.writeEntry("AlwaysDim", m_alwaysDim->isChecked());
    conf.sync();
    setNeedsSave(false);
    // Tell the running compositor to re-read the group (same as stock
    // effect KCMs). No-op when the effect is not loaded.
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                      QStringLiteral("/Effects"),
                                                      QStringLiteral("org.kde.kwin.Effects"),
                                                      QStringLiteral("reconfigureEffect"));
    msg.setArguments({QStringLiteral("oledpanel")});
    QDBusConnection::sessionBus().asyncCall(msg);
}
void OledPanelConfig::defaults()
{
    m_brightness->setValue(qRound(kDefaultFactor * 100));
    m_delay->setValue(kDefaultTimeoutMs / 1000);
    m_transition->setValue(kDefaultFadeMs / 1000.0);
    m_hdrOnly->setChecked(kDefaultHdrOnly);
    m_alwaysDim->setChecked(kDefaultAlwaysDim);
    updateDelayEnabled();
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
        // The loader first probes for a QML module (iface KQuickConfigModule)
        // and only then loads the C++ one (iface KCModule). Constructing on
        // the probe would leak a second visible page, so refuse anything
        // that is not a plain KCModule request.
        if (!iface || strcmp(iface, "KCModule") != 0) {
            return nullptr;
        }
        QObject *actualParent = parentWidget ? static_cast<QObject *>(parentWidget) : parent;
        return new OledPanelConfig(actualParent, KPluginMetaData());
    }
};

#include "config.moc"
