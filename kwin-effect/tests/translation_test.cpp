// SPDX-FileCopyrightText: 2026 deno
// SPDX-License-Identifier: GPL-2.0-or-later

#include <KCModule>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginMetaData>

#include <QApplication>
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QPluginLoader>
#include <QSlider>
#include <QStandardPaths>

#include <clocale>
#include <iostream>
#include <memory>

namespace
{
QString brightnessLabel(KPluginFactory *factory)
{
    QWidget parent;
    std::unique_ptr<KCModule> module(factory->create<KCModule>(&parent));
    if (!module) {
        return {};
    }
    auto *form = qobject_cast<QFormLayout *>(module->widget()->layout());
    auto *slider = module->widget()->findChild<QSlider *>();
    if (!form || !slider) {
        return {};
    }
    auto *label = qobject_cast<QLabel *>(form->labelForField(slider->parentWidget()));
    return label ? label->text() : QString();
}
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        std::cerr << "Expected config plugin, effect plugin, and locale directory\n";
        return 1;
    }
    const char *locale = nullptr;
    for (const char *candidate : {"en_US.UTF-8", "en_US.utf8", "pl_PL.UTF-8", "pl_PL.utf8", "fr_FR.UTF-8", "fr_FR.utf8"}) {
        if (std::setlocale(LC_ALL, candidate)) {
            locale = candidate;
            break;
        }
    }
    if (!locale) {
        std::cerr << "Skipping gettext test: no suitable UTF-8 system locale is installed\n";
        return 77;
    }
    qputenv("LC_ALL", locale);
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("LANGUAGE", "pl");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QLocale::setDefault(QLocale(QStringLiteral("pl_PL")));
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("unrelated-host"));
    KLocalizedString::addDomainLocaleDir(QByteArrayLiteral("kwin_effect_oledpanel"), QString::fromLocal8Bit(argv[3]));

    QPluginLoader loader(QString::fromLocal8Bit(argv[1]));
    auto *factory = qobject_cast<KPluginFactory *>(loader.instance());
    if (!factory) {
        std::cerr << "Could not load the config plugin: " << loader.errorString().toStdString() << '\n';
        return 1;
    }

    KLocalizedString::setLanguages({QStringLiteral("pl")});
    const QString polishLabel = brightnessLabel(factory);
    if (polishLabel != QString::fromUtf8("Najmniejsza jasność:")) {
        std::cerr << "Polish settings text was not loaded from the plugin domain: " << polishLabel.toStdString() << '\n';
        return 1;
    }
    const KPluginMetaData effect(QString::fromLocal8Bit(argv[2]));
    const KPluginMetaData config(QString::fromLocal8Bit(argv[1]));
    if (!effect.isValid() || !config.isValid()
        || effect.name() != QString::fromUtf8("Ochrona panelu OLED HDR")
        || config.name() != effect.name()
        || !effect.description().startsWith(QStringLiteral("Przyciemnia panel"))
        || config.description() != QString::fromUtf8("Ustawienia efektu ochrony panelu OLED HDR")) {
        std::cerr << "Polish embedded plugin metadata was not loaded\n";
        return 1;
    }

    KLocalizedString::setLanguages({QStringLiteral("ar")});
    if (brightnessLabel(factory) != QString::fromUtf8("السطوع الأدنى:")) {
        std::cerr << "Arabic settings text was not loaded\n";
        return 1;
    }
    KLocalizedString::setLanguages({QStringLiteral("en_US")});
    if (brightnessLabel(factory) != QStringLiteral("Minimum brightness:")) {
        std::cerr << "Untranslated settings did not fall back to English\n";
        return 1;
    }
    return 0;
}
