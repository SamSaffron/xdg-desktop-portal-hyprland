#include "mainpicker.h"
#include "./ui_mainpicker.h"
#include <QDebug>
#include <QScreen>
#include <QSettings>
#include <iostream>

extern std::string execAndGet(const char* cmd);

MainPicker::MainPicker(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MainPicker)
{
    ui->setupUi(this);

    connect(ui->shareBtn, &QPushButton::clicked, this, &MainPicker::onShare);
    connect(ui->cancelBtn, &QPushButton::clicked, this, &MainPicker::onCancel);
    connect(ui->windowList, &QListWidget::itemSelectionChanged, this, &MainPicker::onWindowSelectionChanged);
    connect(ui->regionButton, &QPushButton::clicked, this, &MainPicker::onRegion);
}

MainPicker::~MainPicker()
{
    delete ui;
}

void MainPicker::init(const std::vector<SWindowEntry>& windows, bool allowToken) {
    if (allowToken)
        ui->checkBox->setCheckState(Qt::Checked);

    // Populate screens
    const auto SCREENS = QGuiApplication::screens();
    for (int i = 0; i < SCREENS.size(); ++i) {
        const auto GEOMETRY = SCREENS[i]->geometry();
        QString text = QString("Screen %1 at %2, %3 (%4x%5) (%6)")
                           .arg(i)
                           .arg(GEOMETRY.x())
                           .arg(GEOMETRY.y())
                           .arg(GEOMETRY.width())
                           .arg(GEOMETRY.height())
                           .arg(SCREENS[i]->name());
        
        auto item = new QListWidgetItem(text);
        ui->screenList->addItem(item);
        m_screenMap[item] = SCREENS[i]->name();
    }
    if (ui->screenList->count() > 0)
        ui->screenList->setCurrentRow(0);

    // Populate windows
    for (const auto& win : windows) {
        QString text = QString::fromStdString(win.clazz + ": " + win.name);
        auto item = new QListWidgetItem(text);
        ui->windowList->addItem(item);
        m_windowMap[item] = win.id;
        m_windowHandleMap[item] = win.handle;
    }
    if (ui->windowList->count() > 0)
        ui->windowList->setCurrentRow(0);

    // Set default tab
    int defaultTab = 1; // Default to Window
    const char* envTab = getenv("XDPH_PICKER_DEFAULT_TAB");
    if (envTab) {
        std::string tabStr = envTab;
        if (tabStr == "screen") defaultTab = 0;
        else if (tabStr == "window") defaultTab = 1;
        else if (tabStr == "region") defaultTab = 2;
    }
    ui->tabWidget->setCurrentIndex(defaultTab);
}

void MainPicker::updateWindowPreview(unsigned long long handle, const QImage& image) {
    m_windowImages[handle] = image;

    // if selected, update preview
    auto items = ui->windowList->selectedItems();
    if (items.isEmpty()) return;

    if (m_windowHandleMap[items[0]] == handle) {
        updatePreviewLabel();
    }
}

void MainPicker::onWindowSelectionChanged() {
    updatePreviewLabel();
}

void MainPicker::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    updatePreviewLabel();
}

void MainPicker::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    updatePreviewLabel();
}

void MainPicker::updatePreviewLabel() {
    auto items = ui->windowList->selectedItems();
    if (items.isEmpty()) {
        ui->windowPreview->setText("Select a window to preview");
        return;
    }

    unsigned long long handle = m_windowHandleMap[items[0]];
    if (m_windowImages.contains(handle)) {
        QPixmap pm = QPixmap::fromImage(m_windowImages[handle]);
        if (!pm.isNull()) {
             QSize previewSize = ui->windowPreview->size();
             
             // Account for device pixel ratio for sharp rendering on hidpi
             qreal dpr = ui->windowPreview->devicePixelRatio();
             QSize targetSize = previewSize * dpr;
             
             if (targetSize.width() < 100 || targetSize.height() < 100) {
                 targetSize = QSize(640, 480) * dpr;
             }
             
             pm = pm.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
             pm.setDevicePixelRatio(dpr);
             
             ui->windowPreview->setPixmap(pm);
             return;
        }
    }
    ui->windowPreview->setText("No preview available");
}

void MainPicker::onShare() {
    int tab = ui->tabWidget->currentIndex();
    std::string selection;

    if (tab == 0) { // Screen
        auto items = ui->screenList->selectedItems();
        if (items.isEmpty()) return;
        selection = "screen:" + m_screenMap[items[0]].toStdString();
    } else if (tab == 1) { // Window
        auto items = ui->windowList->selectedItems();
        if (items.isEmpty()) return;
        selection = "window:" + std::to_string(m_windowMap[items[0]]);
    } else {
        onRegion();
        return;
    }

    finish(selection);
}

void MainPicker::onRegion() {
    auto REGION = execAndGet("slurp -f \"%o %x %y %w %h\"");
    if (REGION.find("error") != std::string::npos || REGION.empty()) return;
    
    REGION = REGION.substr(0, REGION.length()); 
    while (!REGION.empty() && (REGION.back() == '\n' || REGION.back() == '\r')) REGION.pop_back();

    if (REGION.find_first_of(' ') == std::string::npos) {
        std::cout << "error1\n";
        QApplication::quit();
        return;
    }
    
    const auto SCREEN_NAME = REGION.substr(0, REGION.find_first_of(' '));
    
    // Find screen
    QScreen* pScreen = nullptr;
    const auto SCREENS = QGuiApplication::screens();
    for (auto& screen : SCREENS) {
        if (screen->name().toStdString() == SCREEN_NAME) {
            pScreen = screen;
            break;
        }
    }

    if (!pScreen) {
        std::cout << "error2\n";
        QApplication::quit();
        return;
    }

    try {
        std::string rest = REGION.substr(REGION.find_first_of(' ') + 1);
        const auto X = std::stoi(rest.substr(0, rest.find_first_of(' ')));
        rest = rest.substr(rest.find_first_of(' ') + 1);
        const auto Y = std::stoi(rest.substr(0, rest.find_first_of(' ')));
        rest = rest.substr(rest.find_first_of(' ') + 1);
        const auto W = std::stoi(rest.substr(0, rest.find_first_of(' ')));
        rest = rest.substr(rest.find_first_of(' ') + 1);
        const auto H = std::stoi(rest);

        std::string selection = "region:" + SCREEN_NAME + "@" + 
                                std::to_string(X - pScreen->geometry().x()) + "," + 
                                std::to_string(Y - pScreen->geometry().y()) + "," + 
                                std::to_string(W) + "," + std::to_string(H);
        finish(selection);
    } catch (...) {
        std::cout << "error3\n";
        QApplication::quit();
    }
}

void MainPicker::onCancel() {
    QApplication::quit();
}

void MainPicker::finish(const std::string& result) {
    std::cout << "[SELECTION]";
    std::cout << (ui->checkBox->isChecked() ? "r" : "");
    std::cout << "/";
    std::cout << result << "\n";

    QSettings settings("/tmp/hypr/hyprland-share-picker.conf", QSettings::IniFormat);
    settings.setValue("width", width());
    settings.setValue("height", height());
    settings.sync();

    QApplication::quit();
}
