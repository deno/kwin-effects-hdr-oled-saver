// SPDX-FileCopyrightText: 2026 deno
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"
#include "monitormode.h"

#include <KCMultiDialog>
#include <KLocalizedString>
#include <KMessageWidget>
#include <KPluginFactory>
#include <KSharedConfig>

#include <KScreen/Config>
#include <KScreen/ConfigMonitor>
#include <KScreen/GetConfigOperation>
#include <KScreen/Output>
#include <KScreen/Screen>

#include <QCheckBox>
#include <QComboBox>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>
#include <utility>

namespace
{
using MonitorMode = KWin::OledPanelSettings::EnumMonitorMode;

QString monitorName(const KScreen::Output &output, const QString &uuid)
{
    QString connector = output.name();
    QString name;
    if (output.type() == KScreen::Output::Panel) {
        name = i18nc("@label internal monitor", "Built-in Screen");
    } else {
        const auto vendor = output.vendor().trimmed();
        const auto model = output.model().trimmed();
        name = vendor.isEmpty() ? model : (model.isEmpty() ? vendor : QStringLiteral("%1 %2").arg(vendor, model));
    }
    if (connector.isEmpty()) {
        connector = uuid.isEmpty() ? QString::number(output.id()) : uuid;
        if (name.isEmpty()) {
            name = i18nc("@label unknown monitor", "Monitor");
        }
    }
    return name.isEmpty() ? connector : i18nc("%1 is a monitor name, %2 is its connector or identifier", "%1 (%2)", name, connector);
}
}

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

    m_monitorMode = new QComboBox(widget());
    m_monitorMode->addItem(i18n("All monitors"), MonitorMode::All);
    m_monitorMode->addItem(i18n("All HDR monitors"), MonitorMode::Hdr);
    m_monitorMode->addItem(i18n("Selected monitors"), MonitorMode::Selected);
    layout->addRow(i18n("Dim panels on:"), m_monitorMode);

    auto *monitorOptions = new QWidget(widget());
    auto *monitorLayout = new QVBoxLayout(monitorOptions);
    monitorLayout->setContentsMargins(0, 0, 0, 0);
    m_monitorStatus = new QLabel(monitorOptions);
    m_monitorStatus->setWordWrap(true);
    monitorLayout->addWidget(m_monitorStatus);
    m_monitorList = new QListWidget(monitorOptions);
    // Keep Breeze's native frame even when the checklist is disabled.
    m_monitorList->setProperty("_breeze_force_frame", true);
    m_monitorList->setUniformItemSizes(true);
    m_monitorList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_monitorList->setAccessibleName(i18n("Monitors to dim"));
    m_monitorList->ensurePolished();
    monitorLayout->addWidget(m_monitorList);
    m_monitorError = new KMessageWidget(i18n("Select at least one monitor."), monitorOptions);
    m_monitorError->setMessageType(KMessageWidget::Error);
    m_monitorError->setCloseButtonVisible(false);
    auto errorPolicy = m_monitorError->sizePolicy();
    errorPolicy.setRetainSizeWhenHidden(true);
    m_monitorError->setSizePolicy(errorPolicy);
    monitorLayout->addWidget(m_monitorError);
    layout->addRow(monitorOptions);

    connect(m_brightness, &QSlider::valueChanged, this, [this] {
        refreshLabels();
        updateChangeState();
    });
    connect(m_delay, &QSpinBox::valueChanged, this, &OledPanelConfig::updateChangeState);
    connect(m_transition, &QDoubleSpinBox::valueChanged, this, &OledPanelConfig::updateChangeState);
    connect(m_monitorMode, &QComboBox::currentIndexChanged, this, &OledPanelConfig::updateChangeState);
    connect(m_monitorList, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        const auto uuid = item->data(Qt::UserRole).toString();
        if (uuid.isEmpty()) {
            return;
        }
        if (item->checkState() == Qt::Checked) {
            m_selectedMonitors.insert(uuid);
        } else {
            m_selectedMonitors.remove(uuid);
        }
        updateChangeState();
    });
    connect(m_alwaysDim, &QCheckBox::toggled, this, [this] {
        updateDelayEnabled();
        updateChangeState();
    });
    widget()->installEventFilter(this);
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
    startMonitorDiscovery();
}

OledPanelConfig::~OledPanelConfig()
{
    if (m_monitorConfig) {
        KScreen::ConfigMonitor::instance()->removeConfig(m_monitorConfig);
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

void OledPanelConfig::startMonitorDiscovery()
{
    auto *monitor = KScreen::ConfigMonitor::instance();
    // Watch before fetching the initial snapshot so a concurrent hotplug wins
    // over an older operation result.
    m_monitorConfig = KScreen::ConfigPtr::create();
    m_monitorConfig->setScreen(KScreen::ScreenPtr::create());
    monitor->addConfig(m_monitorConfig);
    connect(monitor, &KScreen::ConfigMonitor::configurationChanged, this, [this] {
        refreshMonitors(MonitorState::Ready);
    });

    // KWin supplies the display names and UUIDs without an EDID round-trip.
    auto *operation = new KScreen::GetConfigOperation(KScreen::GetConfigOperation::NoEDID, this);
    connect(operation, &KScreen::GetConfigOperation::finished, this, [this, monitor](KScreen::ConfigOperation *finished) {
        if (m_monitorState == MonitorState::Ready) {
            return;
        }
        auto config = finished->config();
        if (finished->hasError() || !config || !config->screen()) {
            refreshMonitors(MonitorState::Error);
            return;
        }
        monitor->removeConfig(m_monitorConfig);
        m_monitorConfig = std::move(config);
        monitor->addConfig(m_monitorConfig);
        refreshMonitors(MonitorState::Ready);
    });
}

void OledPanelConfig::refreshMonitors(MonitorState state)
{
    QList<Monitor> monitors;
    if (state == MonitorState::Ready) {
        const auto outputs = m_monitorConfig->connectedOutputs();
        monitors.reserve(outputs.size());
        for (const auto &output : outputs) {
            auto uuid = output->uuid();
            monitors.append({monitorName(*output, uuid), std::move(uuid), output->isEnabled()});
        }
        std::sort(monitors.begin(), monitors.end(), [](const Monitor &left, const Monitor &right) {
            const int byName = QString::localeAwareCompare(left.name, right.name);
            return byName == 0 ? left.uuid < right.uuid : byName < 0;
        });
    }

    if (m_monitorState == state && m_connectedMonitors == monitors) {
        return;
    }
    const bool firstResult = m_monitorState == MonitorState::Loading;
    m_monitorState = state;
    m_connectedMonitors.swap(monitors);
    for (const auto &monitor : std::as_const(m_connectedMonitors)) {
        if (!monitor.uuid.isEmpty()) {
            m_knownMonitorNames.insert(monitor.uuid, monitor.name);
        }
    }
    updateMonitorList();
    // KCMultiDialog takes its opening size hint before asynchronous KScreen
    // discovery may have reported disabled outputs. Grow once, not on hotplug.
    if (firstResult && widget()->isVisible()) {
        QTimer::singleShot(0, this, [this] {
            auto *dialog = qobject_cast<KCMultiDialog *>(widget()->window());
            if (dialog && widget()->isVisible()) {
                const int height = std::min(dialog->sizeHint().height(), dialog->screen()->availableGeometry().height());
                if (height > dialog->height()) {
                    dialog->resize(dialog->width(), height);
                }
            }
        });
    }
}

bool OledPanelConfig::monitorSelectionValid() const
{
    return m_monitorMode->currentData().toInt() != MonitorMode::Selected || !m_selectedMonitors.isEmpty();
}

void OledPanelConfig::setDialogOkEnabled(bool enabled)
{
    // KCMultiDialog ties Apply to needsSave(), but OK accepts even a clean page.
    if (auto *dialog = qobject_cast<KCMultiDialog *>(widget()->window())) {
        if (auto *ok = dialog->button(QDialogButtonBox::Ok)) {
            ok->setEnabled(enabled);
        }
    }
}

bool OledPanelConfig::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == widget()) {
        if (event->type() == QEvent::Show) {
            setDialogOkEnabled(monitorSelectionValid());
        } else if (event->type() == QEvent::Hide) {
            setDialogOkEnabled(true);
        }
    }
    return KCModule::eventFilter(watched, event);
}

void OledPanelConfig::updateMonitorList()
{
    const QSignalBlocker blocker(m_monitorList);
    m_monitorList->clear();

    // Keep checked IDs when a monitor disappears and allow saved selections
    // to be removed even if the monitor cannot currently be discovered.
    auto offline = m_savedValues.selectedMonitors | m_selectedMonitors;
    const auto addMonitor = [this](const QString &name, const QString &uuid) {
        auto *item = new QListWidgetItem(name, m_monitorList);
        item->setToolTip(name);
        item->setData(Qt::UserRole, uuid);
        if (uuid.isEmpty()) {
            item->setFlags(item->flags() & ~(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable));
        } else {
            item->setCheckState(m_selectedMonitors.contains(uuid) ? Qt::Checked : Qt::Unchecked);
        }
    };
    for (const auto &monitor : std::as_const(m_connectedMonitors)) {
        offline.remove(monitor.uuid);
        auto name = monitor.enabled ? monitor.name : i18nc("@item monitor name and status", "%1 (disabled)", monitor.name);
        if (monitor.uuid.isEmpty()) {
            name = i18nc("@item monitor cannot be selected", "%1 (no monitor identifier)", name);
        }
        addMonitor(name, monitor.uuid);
    }

    auto offlineIds = offline.values();
    std::sort(offlineIds.begin(), offlineIds.end());
    for (const auto &uuid : std::as_const(offlineIds)) {
        const auto knownName = m_knownMonitorNames.constFind(uuid);
        const auto name = knownName == m_knownMonitorNames.cend()
            ? i18nc("@item monitor with known identifier only", "Monitor (%1)", uuid)
            : *knownName;
        addMonitor(m_monitorState == MonitorState::Ready ? i18nc("@item monitor name and status", "%1 (disconnected)", name) : name, uuid);
    }

    int rows = std::max(2, m_monitorList->count());
    if (m_monitorState == MonitorState::Loading) {
        rows = std::max(rows, int(QGuiApplication::screens().size()));
    }
    int rowHeight = m_monitorList->sizeHintForRow(0);
    if (rowHeight < 0) {
        // No delegate row to measure until KScreen supplies the first output.
        QStyleOptionViewItem option;
        option.initFrom(m_monitorList);
        option.features = QStyleOptionViewItem::HasCheckIndicator | QStyleOptionViewItem::HasDisplay;
        option.text = QStringLiteral("M");
        rowHeight = m_monitorList->style()->sizeFromContents(QStyle::CT_ItemViewItem, &option, {}, m_monitorList).height();
    }
    m_monitorList->setFixedHeight(2 * m_monitorList->frameWidth() + rows * rowHeight);

    QString status;
    switch (m_monitorState) {
    case MonitorState::Loading:
        status = i18n("Finding connected monitors…");
        break;
    case MonitorState::Ready:
        if (m_connectedMonitors.isEmpty()) {
            status = i18n("No connected monitors.");
        }
        break;
    case MonitorState::Error:
        status = i18n("Could not detect connected monitors.");
        break;
    }
    m_monitorStatus->setText(status);
    m_monitorStatus->setVisible(!status.isEmpty());
}

OledPanelConfig::Values OledPanelConfig::values() const
{
    return {
        m_brightness->value(),
        m_delay->value(),
        qRound(m_transition->value() * 10.0),
        m_monitorMode->currentData().toInt(),
        m_alwaysDim->isChecked(),
        m_selectedMonitors,
    };
}

OledPanelConfig::Values OledPanelConfig::defaultValues() const
{
    return {
        qRound(m_settings.defaultDimFactorValue() * 100),
        m_settings.defaultTimeoutMsValue() / 1000,
        qRound(m_settings.defaultFadeMsValue() / 100.0),
        m_settings.defaultMonitorModeValue(),
        m_settings.defaultAlwaysDimValue(),
        {},
    };
}

void OledPanelConfig::updateChangeState()
{
    const auto current = values();
    const bool valid = monitorSelectionValid();
    m_monitorList->setEnabled(current.monitorMode == MonitorMode::Selected);
    m_monitorError->setVisible(!valid);
    setNeedsSave(valid && current != m_savedValues);
    setRepresentsDefaults(current == defaultValues());
    if (widget()->isVisible()) {
        setDialogOkEnabled(valid);
    }
}

void OledPanelConfig::showValues(const Values &values)
{
    const QSignalBlocker brightnessBlocker(m_brightness);
    const QSignalBlocker delayBlocker(m_delay);
    const QSignalBlocker transitionBlocker(m_transition);
    const QSignalBlocker modeBlocker(m_monitorMode);
    const QSignalBlocker alwaysDimBlocker(m_alwaysDim);
    m_brightness->setValue(values.brightness);
    m_delay->setValue(values.delay);
    m_transition->setValue(values.transitionTenths / 10.0);
    m_monitorMode->setCurrentIndex(m_monitorMode->findData(values.monitorMode));
    m_alwaysDim->setChecked(values.alwaysDim);
    m_selectedMonitors = values.selectedMonitors;
    updateDelayEnabled();
    refreshLabels();
}

void OledPanelConfig::load()
{
    m_settings.load();
    const auto ids = m_settings.selectedMonitors();
    QSet<QString> selected(ids.cbegin(), ids.cend());
    selected.remove(QString());
    const Values loaded{
        qRound(m_settings.dimFactor() * 100),
        m_settings.timeoutMs() / 1000,
        qRound(m_settings.fadeMs() / 100.0),
        KWin::effectiveMonitorMode(m_settings),
        m_settings.alwaysDim(),
        std::move(selected),
    };
    showValues(loaded);
    m_savedValues = values();
    updateMonitorList();
    updateChangeState();
}

void OledPanelConfig::save()
{
    if (!monitorSelectionValid()) {
        return;
    }
    m_settings.setDimFactor(m_brightness->value() / 100.0);
    m_settings.setTimeoutMs(m_delay->value() * 1000);
    m_settings.setFadeMs(qRound(m_transition->value() * 1000.0));
    m_settings.setMonitorMode(m_monitorMode->currentData().toInt());
    auto selected = m_selectedMonitors.values();
    std::sort(selected.begin(), selected.end());
    m_settings.setSelectedMonitors(selected);
    m_settings.setAlwaysDim(m_alwaysDim->isChecked());
    KWin::clearLegacyMonitorMode(m_settings);
    if (!m_settings.save()) {
        setNeedsSave(true);
        return;
    }

    m_savedValues = values();
    updateMonitorList();
    updateChangeState();

    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("/Effects"),
                                                          QStringLiteral("org.kde.kwin.Effects"),
                                                          QStringLiteral("reconfigureEffect"));
    message.setArguments({QStringLiteral("oledpanel")});
    QDBusConnection::sessionBus().asyncCall(message);
}

void OledPanelConfig::defaults()
{
    showValues(defaultValues());
    updateMonitorList();
    updateChangeState();
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
