// SPDX-FileCopyrightText: 2026 deno
// SPDX-License-Identifier: GPL-2.0-or-later

#include "oledpanel.h"

#include <KConfigGroup>
#include <KPluginFactory>

#include <input_event.h>
#include <cursor.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>

namespace KWin
{
OledPanelEffect::OledPanelEffect(QObject *parent)
    : Effect(parent)
{
    m_idleTimer.setSingleShot(true);
    m_idleTimer.setInterval(m_timeoutMs);
    qInfo("oledpanel: effect loaded (dimFactor=%.2f, timeout=%dms)", m_dimFactor, m_timeoutMs);
    connect(effects, &EffectsHandler::windowAdded, this, &OledPanelEffect::onWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &OledPanelEffect::onWindowClosed);
    m_clock.start();
    m_sinceBoot.start();
    for (EffectWindow *w : effects->stackingOrder()) {
        onWindowAdded(w);
    }
    reconfigure(ReconfigureAll);
    ensureWake(); // bootstrap: guarantees a first evaluation frame
}

OledPanelEffect::~OledPanelEffect() = default;

void OledPanelEffect::reconfigure(ReconfigureFlags /*flags*/)
{
    effects->config()->reparseConfiguration();
    const KConfigGroup conf = effects->config()->group(QStringLiteral("Effect-oledpanel"));
    m_dimFactor = qBound(0.05, conf.readEntry("DimFactor", 0.45), 1.0);
    m_timeoutMs = qBound(0, conf.readEntry("TimeoutMs", 0), 3600000);
    m_fadeMs = qBound(0, conf.readEntry("FadeMs", 200), 300000);
    m_hdrOnly = conf.readEntry("HdrOnly", true);
    m_alwaysDim = conf.readEntry("AlwaysDim", false);
    m_idleTimer.setInterval(m_timeoutMs);
    refreshHdrCache();
    ensureWake();
    effects->addRepaintFull();
}

bool OledPanelEffect::isPanel(const EffectWindow *w)
{
    if (!w || w->isDeleted()) {
        return false;
    }
    // Plasma panels: layer-shell dock surfaces owned by plasmashell.
    return w->isDock() && w->windowClass().contains(QLatin1String("plasmashell"));
}

void OledPanelEffect::refreshHdrCache() const
{
    // KWin exposes no per-output HDR state to effects, so read what
    // KScreen persisted. Reloaded when the file changes (checked per
    // frame via mtime — one stat call, no parsing unless changed).
    const QString path = QStandardPaths::locate(QStandardPaths::ConfigLocation,
                                                QStringLiteral("kwinoutputconfig.json"));
    if (path.isEmpty()) {
        return;
    }
    const QDateTime stamp = QFileInfo(path).lastModified();
    if (stamp.isValid() && stamp == m_hdrCacheStamp && !m_hdrOutputs.isEmpty()) {
        return;
    }
    m_hdrCacheStamp = stamp;
    m_hdrOutputs.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray() || doc.array().isEmpty()) {
        return;
    }
    const QJsonArray outputs = doc.array().first().toObject().value(QStringLiteral("data")).toArray();
    for (const QJsonValue &v : outputs) {
        const QJsonObject o = v.toObject();
        const QString connector = o.value(QStringLiteral("connectorName")).toString();
        const bool hdr = o.value(QStringLiteral("highDynamicRange")).toBool(false);
        if (!connector.isEmpty() && hdr) {
            m_hdrOutputs.insert(connector, true);
        }
    }
}

bool OledPanelEffect::isHdrOutput(const QString &connector) const
{
    if (connector.isEmpty()) {
        return true; // fail open
    }
    refreshHdrCache();
    if (!m_hdrOutputs.contains(connector)) {
        // Unknown connector: if we know nothing at all, fail open;
        // otherwise treat as SDR.
        return m_hdrOutputs.isEmpty();
    }
    return m_hdrOutputs.value(connector, false);
}

double OledPanelEffect::targetFor(const EffectWindow *w) const
{
    if (!isPanel(w)) {
        return 1.0;
    }
    if (m_hdrOnly) {
        const QString connector = w->screen() ? w->screen()->name() : QString();
        if (!isHdrOutput(connector)) {
            return 1.0; // SDR output: nothing to save, leave alone
        }
    }
    if (m_alwaysDim) {
        return m_dimFactor;
    }
    if (w->frameGeometry().contains(m_pointerPos)) {
        return 1.0; // hovered: always full
    }
    if (m_sinceBoot.elapsed() - m_lastActive.value(w, 0) < m_timeoutMs) {
        return 1.0; // grace period after this panel was last touched
    }
    return m_dimFactor;
}

void OledPanelEffect::onIdleTimeout()
{
    // Wake-up ping only: disarm and repaint so per-panel deadlines recompute.
    m_wakeAt = 0;
    effects->addRepaintFull();
}

void OledPanelEffect::touchPanel(const QPointF &pos)
{
    const qint64 now = m_sinceBoot.elapsed();
    for (const EffectWindow *w : std::as_const(m_panels)) {
        if (w->frameGeometry().contains(pos)) {
            m_lastActive[w] = now;
        }
    }
}
void OledPanelEffect::ensureWake()
{
    const qint64 now = m_sinceBoot.elapsed();
    qint64 nearest = std::numeric_limits<qint64>::max();
    bool needRepaint = false;
    for (auto it = m_factors.constBegin(); it != m_factors.constEnd(); ++it) {
        const EffectWindow *w = it.key();
        if (m_hdrOnly) {
            const QString connector = w->screen() ? w->screen()->name() : QString();
            if (!isHdrOutput(connector)) {
                if (!qFuzzyCompare(it.value(), 1.0)) {
                    needRepaint = true;
                }
                continue; // SDR output: never dims, no deadline
            }
        }
        if (!qFuzzyCompare(it.value(), targetFor(w))) {
            needRepaint = true;
        }
        if (m_alwaysDim) {
            continue; // always dimmed: no deadline to arm
        }
        if (w->frameGeometry().contains(m_pointerPos)) {
            continue; // hovered: no deadline pending
        }
        const qint64 deadline = m_lastActive.value(w, 0) + m_timeoutMs;
        if (deadline > now && deadline < nearest) {
            nearest = deadline;
        }
    }
    if (needRepaint) {
        effects->addRepaintFull();
    }
    if (nearest != std::numeric_limits<qint64>::max()) {
        m_wakeAt = nearest;
        m_idleTimer.start(nearest - now);
    }
}

void OledPanelEffect::onWindowAdded(EffectWindow *w)
{
    if (isPanel(w)) {
        m_panels.insert(w);
        m_factors.insert(w, m_alwaysDim ? targetFor(w) : 1.0);
        m_lastActive.insert(w, m_sinceBoot.elapsed());
        ensureWake();
    }
}

void OledPanelEffect::onWindowClosed(EffectWindow *w)
{
    m_panels.remove(w);
    m_factors.remove(w);
    m_lastActive.remove(w);
}

void OledPanelEffect::pointerMotion(PointerMotionEvent *event)
{
    const QPointF oldPos = m_pointerPos;
    m_pointerPos = event->position;
    touchPanel(event->position);
    ensureWake();
    if (!m_alwaysDim) {
        for (const EffectWindow *w : std::as_const(m_panels)) {
            if (w->frameGeometry().contains(oldPos) != w->frameGeometry().contains(m_pointerPos)) {
                effects->addRepaintFull();
                break;
            }
        }
    }
}

void OledPanelEffect::pointerButton(PointerButtonEvent *event)
{
    m_pointerPos = event->position;
    touchPanel(event->position);
    ensureWake();
}

bool OledPanelEffect::touchDown(qint32 /*id*/, const QPointF &pos, std::chrono::microseconds /*time*/)
{
    m_pointerPos = pos;
    touchPanel(pos);
    ensureWake();
    return false; // don't consume
}

bool OledPanelEffect::touchMotion(qint32 /*id*/, const QPointF &pos, std::chrono::microseconds /*time*/)
{
    m_pointerPos = pos;
    touchPanel(pos);
    ensureWake();
    return false; // don't consume
}

void OledPanelEffect::prePaintScreen(ScreenPrePaintData &data)
{
    // Poll the live cursor position every frame and stamp hovered panels.
    // Then arm one wake-up for the nearest per-panel deadline so fades
    // start on time even on a static screen (no re-arm churn: only when
    // the deadline moves earlier).
    if (Cursors *cursors = Cursors::self()) {
        if (Cursor *mouse = cursors->mouse()) {
            m_pointerPos = mouse->pos();
            touchPanel(m_pointerPos);
        }
    }
    const qint64 now = m_sinceBoot.elapsed();
    // Keep frames flowing while any panel needs attention. Cursor motion
    // alone often repaints nothing (hardware cursor plane), so a quiet
    // screen would otherwise freeze grace countdowns and fades mid-way.
    // Cost: continuous repaint only during grace/fade windows.
    bool needFrames = false;
    for (auto it = m_factors.constBegin(); it != m_factors.constEnd(); ++it) {
        const EffectWindow *w = it.key();
        if (!qFuzzyCompare(it.value(), targetFor(w))) {
            needFrames = true; // settling toward target
            break;
        }
        if (m_alwaysDim) {
            continue; // always dimmed: steady at target, no grace countdown
        }
        if (w->frameGeometry().contains(m_pointerPos)) {
            continue; // hovered steady full: nothing pending
        }
        if (now - m_lastActive.value(w, 0) >= m_timeoutMs) {
            continue; // grace over (dimmed or SDR-skipped): steady
        }
        if (m_hdrOnly) {
            const QString connector = w->screen() ? w->screen()->name() : QString();
            if (!isHdrOutput(connector)) {
                continue; // never dims: no frames needed
            }
        }
        needFrames = true; // grace countdown pending a real dim
        break;
    }
    if (needFrames) {
        effects->addRepaintFull();
    }
    const double dt = m_clock.restart();
    // FadeMs governs dimming only (bright -> dim); restoring is always a
    // fast 200ms glide. Animation eases from the live value, so re-hovering
    // mid-fade reverses smoothly without jumps. FadeMs==0 means instant dim.
    m_animating = false;
    for (auto it = m_factors.begin(); it != m_factors.end(); ++it) {
        const double target = targetFor(it.key());
        double current = it.value();
        if (!qFuzzyCompare(current, target)) {
            const int fade = (target < current) ? m_fadeMs : 200;
            const double step = (fade > 0) ? dt / fade : std::numeric_limits<double>::max();
            current += qBound(-step, target - current, step);
            it.value() = current;
            m_animating = true;
        }
    }
    if (m_animating) {
        effects->addRepaintFull();
    }
    effects->prePaintScreen(data);
}

bool OledPanelEffect::isActive() const
{
    // Stay in the chain whenever panels exist: dropping out would freeze
    // the last painted factor and break hover-restore. Cost is one hash
    // lookup per window, like DimInactive.
    return !m_panels.isEmpty();
}

int OledPanelEffect::requestedEffectChainPosition() const
{
    return 60;
}

void OledPanelEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    const auto it = m_factors.find(w);
    if (it != m_factors.end() && it.value() < 1.0) {
        data.multiplyBrightness(it.value());
        // TEMPORARY: which outputs actually dim?
        static QElapsedTimer s_dbg;
        static bool s_init = false;
        if (!s_init) {
            s_dbg.start();
            s_init = true;
        }
        if (s_dbg.elapsed() > 2000) {
            s_dbg.restart();
            if (FILE *f = fopen("/tmp/oledpanel-dbg", "a")) {
                const RectF g = w->frameGeometry();
                const QString out = w->screen() ? w->screen()->name() : QStringLiteral("?");
                fprintf(f, "factor=%.2f geo=%.0f,%.0f %.0fx%.0f out=%s class=%s\n",
                        it.value(), g.x(), g.y(), g.width(), g.height(),
                        qPrintable(out), qPrintable(w->windowClass()));
                fclose(f);
            }
        }
    }
    effects->paintWindow(renderTarget, viewport, w, mask, deviceRegion, data);
}

} // namespace KWin

KWIN_EFFECT_FACTORY(KWin::OledPanelEffect,
                    "metadata.json")

#include "oledpanel.moc"
