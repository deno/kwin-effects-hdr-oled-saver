// SPDX-FileCopyrightText: 2026 deno
// SPDX-License-Identifier: GPL-2.0-or-later

#include "oledpanel.h"

#include <KPluginFactory>

#include <core/backendoutput.h>
#include <core/output.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <scene/scene.h>
#include <window.h>

#include <cmath>
#include <limits>
#include <utility>

namespace KWin
{

OledPanelEffect::OledPanelEffect(QObject *parent)
    : Effect(parent)
    , m_settings(effects->config())
{
    m_idleTimer.setSingleShot(true);
    m_idleTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_idleTimer, &QTimer::timeout, this, &OledPanelEffect::onIdleTimeout);

    m_sinceBoot.start();
    reconfigure(ReconfigureAll);
    m_pointerPos = effects->cursorPos();

    connect(effects, &EffectsHandler::windowAdded, this, &OledPanelEffect::onWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &OledPanelEffect::onWindowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &OledPanelEffect::onWindowDeleted);
    connect(effects, &EffectsHandler::screenAdded, this, &OledPanelEffect::onScreenAdded);
    connect(effects, &EffectsHandler::screenRemoved, this, &OledPanelEffect::onScreenRemoved);
    connect(effects, &EffectsHandler::mouseChanged, this,
            [this](const QPointF &pos, const QPointF &oldPos,
                   Qt::MouseButtons buttons, Qt::MouseButtons oldButtons,
                   Qt::KeyboardModifiers, Qt::KeyboardModifiers) {
                if (pos != oldPos || buttons != oldButtons) {
                    m_pointerPos = pos;
                    updatePanels();
                }
            });

    for (LogicalOutput *screen : effects->screens()) {
        onScreenAdded(screen);
    }
    for (EffectWindow *w : effects->stackingOrder()) {
        onWindowAdded(w);
    }
}

OledPanelEffect::~OledPanelEffect() = default;

void OledPanelEffect::reconfigure(ReconfigureFlags /*flags*/)
{
    struct PreviousState {
        qreal brightness;
        bool done;
    };
    QHash<EffectWindow *, PreviousState> previous;
    previous.reserve(m_panels.size());
    for (auto it = m_panels.cbegin(); it != m_panels.cend(); ++it) {
        if (!it->closedBrightness) {
            previous.insert(it.key(), {it->brightness(m_dimFactor), it->transition.done()});
        }
    }

    m_settings.load();
    m_dimFactor = m_settings.dimFactor();
    m_timeoutMs = m_settings.timeoutMs();
    m_fadeMs = m_settings.fadeMs();
    m_hdrOnly = m_settings.hdrOnly();
    m_alwaysDim = m_settings.alwaysDim();

    for (auto it = m_panels.begin(); it != m_panels.end(); ++it) {
        PanelState &state = it.value();
        if (state.closedBrightness) {
            continue;
        }
        state.setTarget(state.targetDimmed, m_fadeMs);
        if (m_dimFactor == 1.0) {
            state.transition.setElapsed(state.transition.duration());
        }
    }

    updatePanels();

    for (auto it = previous.cbegin(); it != previous.cend(); ++it) {
        auto current = m_panels.constFind(it.key());
        if (current == m_panels.cend() || current->closedBrightness) {
            continue;
        }
        const qreal brightness = current->brightness(m_dimFactor);
        if (!qFuzzyCompare(it->brightness, brightness)
            || (it->done && !current->transition.done())) {
            it.key()->addRepaintFull();
        }
    }
}

bool OledPanelEffect::isPanel(const EffectWindow *w)
{
    return w && !w->isDeleted() && w->isDock()
        && w->windowClass().contains(QLatin1String("plasmashell"));
}

bool OledPanelEffect::isHdrOutput(const EffectWindow *w)
{
    const LogicalOutput *screen = w ? w->screen() : nullptr;
    const BackendOutput *backend = screen ? screen->backendOutput() : nullptr;
    return backend && backend->highDynamicRange();
}

bool OledPanelEffect::hasInput(const EffectWindow *w) const
{
    const RectF geometry = w->frameGeometry();
    if (geometry.contains(m_pointerPos)) {
        return true;
    }
    for (const QPointF &point : m_touchPoints) {
        if (geometry.contains(point)) {
            return true;
        }
    }
    return false;
}

bool OledPanelEffect::shouldDim(EffectWindow *w, const PanelState &state, qint64 now) const
{
    if (state.closedBrightness || !isPanel(w)) {
        return false;
    }
    if (m_hdrOnly && !isHdrOutput(w)) {
        return false;
    }
    if (m_alwaysDim) {
        return true;
    }
    if (state.inputActive) {
        return false;
    }
    if (now - state.lastInteraction < m_timeoutMs) {
        return false;
    }
    return true;
}

void OledPanelEffect::updatePanels()
{
    const qint64 now = m_sinceBoot.elapsed();
    for (auto it = m_panels.begin(); it != m_panels.end(); ++it) {
        EffectWindow *w = it.key();
        PanelState &state = it.value();
        if (state.closedBrightness) {
            continue;
        }

        const bool inputActive = hasInput(w);
        if (inputActive || state.inputActive != inputActive) {
            state.lastInteraction = now;
        }
        state.inputActive = inputActive;

        const bool dimmed = shouldDim(w, state, now);
        if (dimmed != state.targetDimmed) {
            state.setTarget(dimmed, m_fadeMs);
            if (m_dimFactor == 1.0) {
                state.transition.setElapsed(state.transition.duration());
            } else {
                w->addRepaintFull();
            }
        }
    }
    ensureWake();
}

void OledPanelEffect::ensureWake()
{
    const qint64 now = m_sinceBoot.elapsed();
    qint64 nearest = std::numeric_limits<qint64>::max();

    if (!m_alwaysDim && m_dimFactor < 1.0) {
        for (auto it = m_panels.cbegin(); it != m_panels.cend(); ++it) {
            EffectWindow *w = it.key();
            const PanelState &state = it.value();
            if (state.closedBrightness || state.inputActive || (m_hdrOnly && !isHdrOutput(w))) {
                continue;
            }
            const qint64 deadline = state.lastInteraction + m_timeoutMs;
            if (deadline > now) {
                nearest = std::min(nearest, deadline);
            }
        }
    }

    if (nearest == std::numeric_limits<qint64>::max()) {
        m_idleTimer.stop();
        m_wakeAt = 0;
        return;
    }
    if (!m_idleTimer.isActive() || m_wakeAt != nearest) {
        m_wakeAt = nearest;
        m_idleTimer.start(static_cast<int>(nearest - now));
    }
}

void OledPanelEffect::onIdleTimeout()
{
    m_wakeAt = 0;
    updatePanels();
}

void OledPanelEffect::disconnectWindowCallbacks(PanelState &state)
{
    for (const QMetaObject::Connection &connection : std::as_const(state.windowConnections)) {
        QObject::disconnect(connection);
    }
    state.windowConnections.clear();
}

void OledPanelEffect::onWindowAdded(EffectWindow *w)
{
    if (!isPanel(w) || m_panels.contains(w)) {
        return;
    }

    auto it = m_panels.insert(w, PanelState(m_sinceBoot.elapsed()));
    PanelState &state = it.value();
    state.windowConnections.append(connect(w, &EffectWindow::windowFrameGeometryChanged,
                                           this, [this](EffectWindow *, const RectF &) {
                                               updatePanels();
                                           }));
    if (Window *window = w->window()) {
        state.windowConnections.append(connect(window, &Window::outputChanged,
                                               this, [this](LogicalOutput *) {
                                                   updatePanels();
                                               }));
    }
    updatePanels();
}

void OledPanelEffect::onWindowClosed(EffectWindow *w)
{
    auto it = m_panels.find(w);
    if (it == m_panels.end() || it->closedBrightness) {
        return;
    }
    it->closedBrightness = it->brightness(m_dimFactor);
    it->inputActive = false;
    disconnectWindowCallbacks(it.value());
    ensureWake();
}

void OledPanelEffect::onWindowDeleted(EffectWindow *w)
{
    auto it = m_panels.find(w);
    if (it == m_panels.end()) {
        return;
    }
    disconnectWindowCallbacks(it.value());
    m_panels.erase(it);
    ensureWake();
}

void OledPanelEffect::onScreenAdded(LogicalOutput *screen)
{
    if (!screen || m_outputConnections.contains(screen)) {
        return;
    }

    QList<QMetaObject::Connection> connections;
    connections.append(connect(screen, &LogicalOutput::geometryChanged, this, &OledPanelEffect::updatePanels));
    connections.append(connect(screen, &LogicalOutput::changed, this, &OledPanelEffect::updatePanels));
    if (BackendOutput *backend = screen->backendOutput()) {
        connections.append(connect(backend, &BackendOutput::highDynamicRangeChanged,
                                   this, &OledPanelEffect::updatePanels));
    }
    connections.append(connect(screen, &QObject::destroyed, this, [this, screen] {
        onScreenRemoved(screen);
    }));
    m_outputConnections.insert(screen, connections);
    updatePanels();
}

void OledPanelEffect::onScreenRemoved(LogicalOutput *screen)
{
    auto it = m_outputConnections.find(screen);
    if (it == m_outputConnections.end()) {
        return;
    }
    for (const QMetaObject::Connection &connection : std::as_const(it.value())) {
        QObject::disconnect(connection);
    }
    m_outputConnections.erase(it);
    updatePanels();
}

bool OledPanelEffect::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds /*time*/)
{
    m_touchPoints.insert(id, pos);
    updatePanels();
    return false;
}

bool OledPanelEffect::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds /*time*/)
{
    auto it = m_touchPoints.find(id);
    if (it != m_touchPoints.end() && it.value() != pos) {
        it.value() = pos;
        updatePanels();
    }
    return false;
}

bool OledPanelEffect::touchUp(qint32 id, std::chrono::microseconds /*time*/)
{
    if (m_touchPoints.remove(id) != 0) {
        updatePanels();
    }
    return false;
}

void OledPanelEffect::touchCancel()
{
    if (!m_touchPoints.isEmpty()) {
        m_touchPoints.clear();
        updatePanels();
    }
}

void OledPanelEffect::prePaintScreen(ScreenPrePaintData &data)
{
    for (auto it = m_panels.begin(); it != m_panels.end(); ++it) {
        if (!it->closedBrightness && !it->transition.done()
            && it.key()->screen() == data.view->logicalOutput()) {
            it->transition.advance(data.view);
        }
    }
    effects->prePaintScreen(data);
}

void OledPanelEffect::postPaintScreen()
{
    for (auto it = m_panels.cbegin(); it != m_panels.cend(); ++it) {
        if (!it->closedBrightness && !it->transition.done()) {
            it.key()->addRepaintFull();
        }
    }
    effects->postPaintScreen();
}

auto OledPanelEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport,
                                EffectWindow *w, int mask, const Region &deviceRegion,
                                WindowPaintData &data)
    -> decltype(Effect::paintWindow(renderTarget, viewport, w, mask, deviceRegion, data))
{
    const auto it = m_panels.constFind(w);
    if (it != m_panels.cend()) {
        const qreal brightness = it->brightness(m_dimFactor);
        if (brightness < 1.0) {
            data.multiplyBrightness(brightness);
        }
    }
    return effects->paintWindow(renderTarget, viewport, w, mask, deviceRegion, data);
}

bool OledPanelEffect::isActive() const
{
    return !m_panels.isEmpty();
}

int OledPanelEffect::requestedEffectChainPosition() const
{
    return 60;
}

} // namespace KWin

KWIN_EFFECT_FACTORY(KWin::OledPanelEffect,
                    "metadata.json")

#include "oledpanel.moc"
