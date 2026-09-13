Name:           kwin-effects-hdr-oled-saver
Version:        0.9.0
Release:        1%{?dist}
Summary:        Dims the Plasma panel brightness when idle to prevent OLED burn-in
URL:            https://github.com/deno/kwin-effects-hdr-oled-saver
License:        GPL-2.0-or-later
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  kf6-kcmutils-devel
BuildRequires:  kf6-kconfig-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-kwindowsystem-devel
BuildRequires:  kwin-devel
BuildRequires:  libdrm-devel
BuildRequires:  libepoxy-devel
BuildRequires:  ninja-build
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  vulkan-headers
BuildRequires:  vulkan-loader-devel
BuildRequires:  wayland-devel
Requires:       kwin%{?_isa}

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
    -DKDE_INSTALL_PLUGINDIR=%{_qt6_plugindir} \
    -DCMAKE_BUILD_TYPE=Release
cmake --build %{_builddir}/build

%install
DESTDIR=%{buildroot} cmake --install %{_builddir}/build

%files
%{_qt6_plugindir}/kwin/effects/plugins/oledpanel.so
%{_qt6_plugindir}/kwin/effects/configs/kwin_oledpanel_config.so
%license LICENSES/GPL-2.0-or-later.txt
%doc README.md

%changelog
* Sun Sep 13 2026 deno - 0.9.0-1
- Initial public release
