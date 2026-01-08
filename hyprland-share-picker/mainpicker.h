#ifndef MAINPICKER_H
#define MAINPICKER_H

#include <QDialog>
#include <QObject>
#include <QEvent>
#include <QListWidgetItem>
#include <QMap>
#include <vector>
#include <string>

QT_BEGIN_NAMESPACE
namespace Ui { class MainPicker; }
QT_END_NAMESPACE

struct SWindowEntry {
    std::string        name;
    std::string        clazz;
    unsigned long long id = 0;
    unsigned long long handle = 0;
    int                workspaceId = -1;
    std::string        workspaceName;
};

class MainPicker : public QDialog
{
    Q_OBJECT

public:
    MainPicker(QWidget *parent = nullptr);
    ~MainPicker();

    void init(const std::vector<SWindowEntry>& windows, bool allowToken, bool showTokenCheckbox);
    void updateWindowPreview(unsigned long long handle, const QImage& image);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onShare();
    void onCancel();
    void onWindowSelectionChanged();
    void onRegion();

private:
    Ui::MainPicker *ui;
    QMap<unsigned long long, QImage> m_windowImages;
    QMap<QListWidgetItem*, unsigned long long> m_windowMap;
    QMap<QListWidgetItem*, unsigned long long> m_windowHandleMap;
    QMap<QListWidgetItem*, QString> m_screenMap;

    void finish(const std::string& result);
    void updatePreviewLabel();
};
#endif // MAINPICKER_H
