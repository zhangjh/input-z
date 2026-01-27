/**
 * TrayManager - Windows 系统托盘管理器
 * Task 7: TrayManager 实现
 */

#ifndef SUYAN_WINDOWS_TRAY_MANAGER_H
#define SUYAN_WINDOWS_TRAY_MANAGER_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QStyle>
#include <string>

namespace suyan {

enum class InputMode;

class TrayManager : public QObject {
    Q_OBJECT

public:
    static TrayManager& instance();

    bool initialize(const std::string& resourcePath);
    void shutdown();
    void updateIcon(InputMode mode);
    void show();
    void hide();
    bool isInitialized() const { return initialized_; }

signals:
    void toggleModeRequested();
    void openSettingsRequested();
    void showAboutRequested();
    void exitRequested();

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onToggleMode();
    void onOpenSettings();
    void onShowAbout();
    void onExit();

private:
    TrayManager();
    ~TrayManager();
    TrayManager(const TrayManager&) = delete;
    TrayManager& operator=(const TrayManager&) = delete;

    void createMenu();
    QIcon loadIcon(const QString& name);

    bool initialized_ = false;
    std::string resourcePath_;
    InputMode currentMode_;
    
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;
    QAction* toggleModeAction_ = nullptr;
    QAction* settingsAction_ = nullptr;
    QAction* aboutAction_ = nullptr;
    QAction* exitAction_ = nullptr;
};

} // namespace suyan

#endif // SUYAN_WINDOWS_TRAY_MANAGER_H
