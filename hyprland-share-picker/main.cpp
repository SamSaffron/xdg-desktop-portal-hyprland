#include <QApplication>
#include <QEvent>
#include <QObject>
#include <QPushButton>
#include <QScreen>
#include <QTabWidget>
#include <QWidget>
#include <QtDebug>
#include <QtWidgets>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QWindow>
#include <array>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <hyprutils/os/Process.hpp>
using namespace Hyprutils::OS;

#include "mainpicker.h"
#include "waylandcapture.h"

std::string execAndGet(const char* cmd) {
    std::string command = cmd + std::string{" 2>&1"};
    CProcess    proc("/bin/sh", {"-c", cmd});

    if (!proc.runSync())
        return "error";

    return proc.stdOut();
}

std::vector<SWindowEntry> getWindows(const char* env) {
    std::vector<SWindowEntry> result;

    if (!env)
        return result;

    std::string rolling = env;

    while (!rolling.empty()) {
        // ID
        const auto IDSEPPOS = rolling.find("[HC>]");
        const auto IDSTR    = rolling.substr(0, IDSEPPOS);

        // class
        const auto CLASSSEPPOS = rolling.find("[HT>]");
        const auto CLASSSTR    = rolling.substr(IDSEPPOS + 5, CLASSSEPPOS - IDSEPPOS - 5);

        // title
        const auto TITLESEPPOS = rolling.find("[HE>]");
        const auto TITLESTR    = rolling.substr(CLASSSEPPOS + 5, TITLESEPPOS - 5 - CLASSSEPPOS);

        // window address
        const auto WINDOWSEPPOS = rolling.find("[HA>]");
        const auto WINDOWADDR = rolling.substr(TITLESEPPOS + 5, WINDOWSEPPOS - 5 - TITLESEPPOS);

        try {
            unsigned long long id = std::stoull(IDSTR);
            unsigned long long handle = 0;
            try {
                handle = std::stoull(WINDOWADDR);
            } catch (...) {}
            
            result.push_back({TITLESTR, CLASSSTR, id, handle});
        } catch (std::exception& e) {
            // silent err
        }

        rolling = rolling.substr(WINDOWSEPPOS + 5);
    }

    return result;
}

std::vector<SWindowEntry> getWindowsFromHyprctl() {
    std::vector<SWindowEntry> result;
    std::string jsonRaw = execAndGet("hyprctl clients -j");

    if (jsonRaw == "error" || jsonRaw.empty()) {
        std::cerr << "[picker] failed to get clients from hyprctl" << std::endl;
        return result;
    }

    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(jsonRaw));
    if (doc.isNull() || !doc.isArray()) {
        std::cerr << "[picker] failed to parse hyprctl json" << std::endl;
        return result;
    }

    QJsonArray arr = doc.array();
    for (const auto& val : arr) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        std::string clazz = obj["class"].toString().toStdString();
        std::string title = obj["title"].toString().toStdString();
        std::string addressStr = obj["address"].toString().toStdString();
        unsigned long long id = 0;
        try {
             id = std::stoull(addressStr, nullptr, 16);
        } catch (...) {}

        result.push_back({title, clazz, id, id});
    }
    return result;
}

int main(int argc, char* argv[]) {
    qputenv("QT_LOGGING_RULES", "qml=false");

    bool allowTokenByDefault = true;
    bool allowTokenSelection = getenv("XDPH_PICKER_ALLOW_TOKEN_SELECTION") != nullptr;
    bool testMode = false;

    for (int i = 1; i < argc; ++i) {
        if (argv[i] == std::string{"--allow-token"})
            allowTokenByDefault = true;
        else if (argv[i] == std::string{"--test"})
            testMode = true;
    }

    std::vector<SWindowEntry> WINDOWLIST;

    if (testMode) {
        WINDOWLIST = getWindowsFromHyprctl();
    } else {
        const char*  WINDOWLISTSTR = getenv("XDPH_WINDOW_SHARING_LIST");
        WINDOWLIST    = getWindows(WINDOWLISTSTR);
    }

    QApplication picker(argc, argv);
    QCoreApplication::setApplicationName("org.hyprland.xdg-desktop-portal-hyprland");
    QCoreApplication::setOrganizationName("hyprland");
    QGuiApplication::setDesktopFileName("org.hyprland.xdg-desktop-portal-hyprland");

    MainPicker w;
    w.init(WINDOWLIST, allowTokenByDefault, allowTokenSelection);

    WaylandCapture* waylandCapture = new WaylandCapture(&w);

    QObject::connect(waylandCapture, &WaylandCapture::frameCaptured, &w, &MainPicker::updateWindowPreview);

    // Start capturing
    for (const auto& window : WINDOWLIST) {
        if (window.handle > 0)
            waylandCapture->capture(window.handle);
    }

    QSettings* settings = new QSettings("/tmp/hypr/hyprland-share-picker.conf", QSettings::IniFormat);
    if (settings->contains("width") && settings->contains("height")) {
        int w_val = settings->value("width").toInt();
        int h_val = settings->value("height").toInt();
        if (w_val > 0 && h_val > 0)
            w.resize(w_val, h_val); // Using resize instead of setGeometry
    }

    w.show();
    return picker.exec();
}
