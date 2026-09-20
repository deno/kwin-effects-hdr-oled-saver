// SPDX-FileCopyrightText: 2026 deno
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "oledpanelsettings.h"

#include <effect/effect.h>
#include <effect/timeline.h>

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QPointF>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <optional>

namespace KWin
{

class LogicalOutput;

struct PanelState
{
    TimeLine transition{std::chrono::milliseconds(200), TimeLine::Backward};
    bool targetDimmed = false;
    bool inputActive = false;
    qint64 lastInteraction = 0;
    std::optional<qreal> closedBrightness;
    QList<QMetaObject::Connection> windowConnections;

    explicit PanelState(qint64 now = 0)
        : lastInteraction(now)
    {
        transition.setEasingCurve(QEasingCurve::InOutSine);
        transition.setSourceRedirectMode(TimeLine::RedirectMode::Strict);
        transition.setTargetRedirectMode(TimeLine::RedirectMode::Relaxed);
        transition.setElapsed(transition.duration());
    }

    void setTarget(bool dimmed, int fadeMs)
    {
        if (closedBrightness) {
            return;
        }

        targetDimmed = dimmed;
        transition.setDirection(dimmed ? TimeLine::Forward : TimeLine::Backward);
        transition.setDuration(std::chrono::milliseconds(dimmed ? std::max(1, fadeMs) : 200));
        if (dimmed && fadeMs == 0) {
            transition.setElapsed(transition.duration());
        }
    }

    qreal brightness(qreal dimFactor) const
    {
        if (closedBrightness) {
            return *closedBrightness;
        }
        return 1.0 + (dimFactor - 1.0) * transition.value();
    }
};

class OledPanelEffect : public Effect
{
    Q_OBJECT

public:
    explicit OledPanelEffect(QObject *parent = nullptr);
    ~OledPanelEffect() override;

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(ScreenPrePaintData &data) override;
    void postPaintScreen() override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data) override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override;

    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchUp(qint32 id, std::chrono::microseconds time) override;
    void touchCancel() override;

private Q_SLOTS:
    void onIdleTimeout();
    void onWindowAdded(EffectWindow *w);
    void onWindowClosed(EffectWindow *w);
    void onWindowDeleted(EffectWindow *w);
    void onScreenAdded(LogicalOutput *screen);
    void onScreenRemoved(LogicalOutput *screen);

private:
    static bool isPanel(const EffectWindow *w);
    static bool isHdrOutput(const EffectWindow *w);
    bool shouldDim(EffectWindow *w, const PanelState &state, qint64 now) const;
    bool hasInput(const EffectWindow *w) const;
    void updatePanels();
    void ensureWake();
    static void disconnectWindowCallbacks(PanelState &state);

    OledPanelSettings m_settings;
    QTimer m_idleTimer;
    QElapsedTimer m_sinceBoot;
    QHash<EffectWindow *, PanelState> m_panels;
    QHash<qint32, QPointF> m_touchPoints;
    QHash<LogicalOutput *, QList<QMetaObject::Connection>> m_outputConnections;
    QPointF m_pointerPos = QPointF(-1, -1);
    qreal m_dimFactor;
    int m_timeoutMs;
    int m_fadeMs;
    bool m_hdrOnly;
    bool m_alwaysDim;
    qint64 m_wakeAt = 0;
};

} // namespace KWin
