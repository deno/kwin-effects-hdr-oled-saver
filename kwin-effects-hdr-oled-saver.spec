Name:           kwin-effects-hdr-oled-saver
Version:        0.9.0
Release:        2%{?dist}
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
BuildRequires:  cmake(KWin)
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
%if 0%{?suse_version}
Requires:       kwin6
%else
Requires:       kwin%{?_isa}
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
* Sun Sep 13 2026 deno - 0.9.0-2
- Portable dependencies (cmake()/pkgconfig() provides, /usr/bin/ninja) for SUSE builds
* Sun Sep 13 2026 deno - 0.9.0-1
- Initial public release
