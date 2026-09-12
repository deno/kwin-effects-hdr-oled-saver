# Upstream integration (plasma-workspace + plasma-desktop)

Target: Plasma 6.7.5 file layout observed on this machine.
Goal: first-class `OLED-saver` next to Visibility, orthogonal to hide modes.

## Why not a 5th VisibilityMode

`PanelView::VisibilityMode` (`NormalPanel/AutoHide/DodgeWindows/WindowsGoBelow`,
`shell/panelview.h`) drives struts + hide geometry in `panelview.cpp`. A dimming
mode wants `NormalPanel` struts (windows still avoid the panel) with only the
emitted light lowered. A 5th enum value would duplicate the `NormalPanel` strut
path and every `visibilityMode != NormalPanel` branch in `Panel.qml`
(`stateTriggers`, de-float exception). Three orthogonal `Q_PROPERTY`s avoid that.

## Patch sketch — plasma-workspace `shell/panelview.h/.cpp`

```cpp
// panelview.h, next to opacityMode:
Q_PROPERTY(bool oledSaverEnabled READ oledSaverEnabled WRITE setOledSaverEnabled NOTIFY oledSaverEnabledChanged)
Q_PROPERTY(int oledSaverTimeoutMs READ oledSaverTimeoutMs WRITE setOledSaverTimeoutMs NOTIFY oledSaverTimeoutMsChanged)
Q_PROPERTY(double oledSaverDimFactor READ oledSaverDimFactor WRITE setOledSaverDimFactor NOTIFY oledSaverDimFactorChanged)
// signals: oledSaverEnabledChanged(), oledSaverTimeoutMsChanged(), oledSaverDimFactorChanged()
// storage: bool m_oledSaverEnabled = false; int m_oledSaverTimeoutMs = 4000; double m_oledSaverDimFactor = 0.45;
```

Persist in the existing panel `KConfigGroup` (`panelConfig()` in
`shell/panel.cpp`-shared helper) as `oledSaverEnabled / oledSaverTimeoutMs /
oledSaverDimFactor` so each panel keeps its own setting. No change to
`updateExclusiveZone()` — struts stay `NormalPanel`.

## HDR behavior

The panel is SDR content: KWin maps it at SDR paperwhite (`fullNits`, Display
settings, 200–250 nits typical). The MultiEffect multiplies the rendered panel
in-shader before that mapping, so dimmed output ≈ `fullNits × dimFactor`
(0.45 → ~90 nits at 200 paperwhite). Global HDR peak and other windows are
untouched.

Phase 2 (optional, true compositor-side control): a KWin C++ effect matching
`plasmashell` layer-shell panel surfaces, with its strength driven over D-Bus
by the same hover+idle state — like `diminactive`, but for panels. Heavier
(needs kwin dev headers + a rebuild); the QML effect above ships the same
photons for the panel today.
`PanelConfiguration.qml`: insert `config/OledSaverConfig.qml` at the end of
the Visibility `ColumnLayout` (after the `autoHideBox` width-picasso
ComboBox, ~line 447). No model change to `autoHideBox`.
