Name:           kwin-effects-hdr-oled-saver
Version:        0.9.4
Release:        1%{?dist}
Summary:        Dims the Plasma panel brightness when idle to prevent OLED burn-in
URL:            https://github.com/deno/kwin-effects-hdr-oled-saver
License:        GPL-2.0-or-later
Source0:        %{name}-%{version}.tar.gz

%if 0%{?suse_version}
%define plugindir %{_kf6_plugindir}
%else
%define plugindir %{_qt6_plugindir}
%endif

%if 0%{?suse_version}
BuildRequires:  ninja
%else
BuildRequires:  ninja-build
%endif
BuildRequires:  cmake
BuildRequires:  cmake(KWin) >= 6.7
BuildRequires:  cmake(Qt6Core)
BuildRequires:  cmake(Qt6Qml)
BuildRequires:  cmake(Qt6Quick)
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  kf6-kcmutils-devel
BuildRequires:  kf6-kconfig-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-kwindowsystem-devel
BuildRequires:  libdrm-devel
BuildRequires:  libepoxy-devel
BuildRequires:  pkgconfig(vulkan)
BuildRequires:  vulkan-headers
BuildRequires:  wayland-devel
# KWin's effect factory IID includes the exact version used to build the plugin.
%if 0%{?suse_version}
Requires:       kwin6 = %{pkg_version cmake(KWin)}
%else
Requires:       kwin%{?_isa} = %{_kwin_version}
%endif

%description
Dims the Plasma panel brightness when idle independently of overall
brightness settings to prevent OLED burn-in. Works best in HDR mode.

Hovering, touching, or clicking the panel restores full brightness
instantly (unless configured to remain dimmed at all times). Panels on
SDR displays can be excluded via the "Only affect HDR outputs" option
(enabled by default). Settings live in System Settings under Desktop
Effects (minimum brightness, idle delay, dimming time).

%prep
%autosetup -n %{name}-%{version}

%build
cmake -S %{_builddir}/%{name}-%{version}/kwin-effect -B %{_builddir}/build -G Ninja \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DKDE_INSTALL_PLUGINDIR=%{plugindir} \
    -DCMAKE_BUILD_TYPE=Release
cmake --build %{_builddir}/build

%install
DESTDIR=%{buildroot} cmake --install %{_builddir}/build

%check
ctest --test-dir %{_builddir}/build --output-on-failure

%files
%dir %{plugindir}/kwin
%dir %{plugindir}/kwin/effects
%dir %{plugindir}/kwin/effects/configs
%dir %{plugindir}/kwin/effects/plugins
%{plugindir}/kwin/effects/plugins/oledpanel.so
%{plugindir}/kwin/effects/configs/kwin_oledpanel_config.so
%license LICENSES/GPL-2.0-or-later.txt
%doc README.md

%changelog
* Tue Nov 10 2026 deno - 0.9.4-1
- Add Ubuntu 26.10 Debian packaging with an exact build-time KWin ABI requirement.
- Include Debian recipes in the OBS release pipeline.
* Tue Nov 10 2026 deno - 0.9.3-1
- Pin Arch packages to the KWin version recorded during compilation.
- Preserve the ABI requirement when repackaging without recompiling.
* Tue Nov 10 2026 deno - 0.9.2-1
- Fix KWin 6.8 painting compatibility while retaining KWin 6.7 support.
- Require the exact KWin build version for Fedora and openSUSE RPMs.
- Run the existing panel transition tests during RPM builds.
* Sun Sep 20 2026 deno - 0.9.1-1
- Refactor: event-driven panel dimming architecture and KDE-standard generated settings.
* Sun Sep 13 2026 deno - 0.9.0-2
- Portable dependencies (cmake()/pkgconfig() provides, /usr/bin/ninja) for SUSE builds
* Sun Sep 13 2026 deno - 0.9.0-1
- Initial public release
