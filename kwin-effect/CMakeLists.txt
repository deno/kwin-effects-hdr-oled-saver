cmake_minimum_required(VERSION 3.20)
project(oledpanel)

set(QT_MIN_VERSION "6.7.0")
set(KF6_MIN_VERSION "6.0.0")

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(ECMSetupQtPluginMacroNames)
ecm_setup_qtplugin_macro_names()
include(KDEInstallDirs)
include(KDECMakeSettings)
find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Widgets)
find_package(KWin REQUIRED)
find_package(KF6 REQUIRED COMPONENTS Config CoreAddons I18n KCMUtils)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(oledpanel MODULE oledpanel.cpp)
target_include_directories(oledpanel PRIVATE /usr/include/kwin)
target_link_libraries(oledpanel PRIVATE
    KWin::kwin
    KF6::ConfigCore
    KF6::CoreAddons
    Qt6::Core
    Qt6::Gui
)

# User-local install: cmake -DCMAKE_INSTALL_PREFIX=$HOME/.local
install(TARGETS oledpanel DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins/)
add_library(kwin_oledpanel_config MODULE config.cpp config.h)
target_link_libraries(kwin_oledpanel_config PRIVATE
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::I18n
    KF6::KCMUtils
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Widgets
)
install(TARGETS kwin_oledpanel_config DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs/)
