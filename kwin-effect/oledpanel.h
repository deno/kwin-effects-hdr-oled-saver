// SPDX-FileCopyrightText: 2026 deno
// SPDX-License-Identifier: GPL-2.0-or-later
//
// oledpanel effect: true compositor-side dimming of Plasma panel surfaces.
//
// Unlike the QML MultiEffect prototype (which darkens the panel pre-paperwhite
// in SDR space), this scales the composited panel surface in KWin's own
// pipeline — i.e. real, per-surface HDR luminance control, independent of the
// rest of the desktop. Same idea as the builtin DimInactive effect, but the
// trigger is panel hover/idle (tracked here from KWin input, no plasmashell
// changes needed) instead of window activation.
#pragma once
#include <QDateTime>
#include <QMap>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QTimer>

#include <effect/effect.h>

namespace KWin
{

class OledPanelEffect : public Effect
{
    Q_OBJECT

public:
    explicit OledPanelEffect(QObject *parent = nullptr);
    ~OledPanelEffect() override;
    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(ScreenPrePaintData &data) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data) override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override;

    void pointerMotion(PointerMotionEvent *event) override;
    void pointerButton(PointerButtonEvent *event) override;
    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
private Q_SLOTS:
    void onIdleTimeout();
    void onWindowAdded(EffectWindow *w);
    void onWindowClosed(EffectWindow *w);

private:
    static bool isPanel(const EffectWindow *w);
    // Stamp now as last-active on every panel containing pos.
    void touchPanel(const QPointF &pos);
    // Arm a wake-up for the nearest pending per-panel deadline.
    void ensureWake();
    double targetFor(const EffectWindow *w) const;
    // True when the window's output is HDR-enabled (from kwinoutputconfig).
    // Unknown outputs count as HDR (fail open = historic behavior).
    bool isHdrOutput(const QString &connector) const;
    void refreshHdrCache() const;
    QTimer m_idleTimer; // wake-up pings only; deadlines are per-panel below
    QElapsedTimer m_clock; // frame dt
    QElapsedTimer m_sinceBoot; // steady ms clock for per-panel idle deadlines
    // Current animated factor per panel window: 1.0 = full, m_dimFactor = dimmed.
    QHash<const EffectWindow *, double> m_factors;
    // Last ms (m_sinceBoot) each panel was hovered/touched.
    QHash<const EffectWindow *, qint64> m_lastActive;
    QSet<const EffectWindow *> m_panels;
    QPointF m_pointerPos = QPointF(-1, -1);
    double m_dimFactor = 0.45;
    int m_timeoutMs = 0;
    int m_fadeMs = 200;
    bool m_hdrOnly = true;
    bool m_alwaysDim = false;
    mutable QMap<QString, bool> m_hdrOutputs;
    mutable QDateTime m_hdrCacheStamp;
    mutable bool m_animating = false;
    qint64 m_wakeAt = 0; // m_sinceBoot ms of armed wake-up, 0 = disarmed
};

} // namespace KWin
