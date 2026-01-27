/**
 * TrayManager 实现
 * Task 7: TrayManager 实现
 */

#include "tray_manager.h"
#include "input_engine.h"
#include <QApplication>
#include <QMessageBox>

namespace suyan {

TrayManager& TrayManager::instance() {
    static TrayManager instance;
    return instance;
}

TrayManager::TrayManager()
    : currentMode_(InputMode::Chinese) {
}

TrayManager::~TrayManager() {
    shutdown();
}

bool TrayManager::initialize(const std::string& resourcePath) {
    if (initialized_) {
        return true;
    }
    
    resourcePath_ = resourcePath;
    
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return false;
    }
    
    trayIcon_ = new QSystemTrayIcon(this);
    
    createMenu();
    
    QIcon defaultIcon = loadIcon(QStringLiteral("chinese.svg"));
    if (defaultIcon.isNull()) {
        defaultIcon = loadIcon(QStringLiteral("chinese.png"));
    }
    if (defaultIcon.isNull()) {
        defaultIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    trayIcon_->setIcon(defaultIcon);
    trayIcon_->setToolTip(QStringLiteral("素言输入法 - 中文"));
    
    connect(trayIcon_, &QSystemTrayIcon::activated,
            this, &TrayManager::onTrayActivated);
    
    initialized_ = true;
    return true;
}

void TrayManager::shutdown() {
    if (!initialized_) {
        return;
    }
    
    if (trayIcon_) {
        trayIcon_->hide();
        delete trayIcon_;
        trayIcon_ = nullptr;
    }
    
    if (trayMenu_) {
        delete trayMenu_;
        trayMenu_ = nullptr;
    }
    
    toggleModeAction_ = nullptr;
    settingsAction_ = nullptr;
    aboutAction_ = nullptr;
    exitAction_ = nullptr;
    
    initialized_ = false;
}

void TrayManager::updateIcon(InputMode mode) {
    if (!trayIcon_) {
        return;
    }
    
    currentMode_ = mode;
    
    QString iconName;
    QString tooltip;
    
    switch (mode) {
    case InputMode::Chinese:
        iconName = QStringLiteral("chinese");
        tooltip = QStringLiteral("素言输入法 - 中文");
        break;
    case InputMode::English:
    case InputMode::TempEnglish:
        iconName = QStringLiteral("english");
        tooltip = QStringLiteral("素言输入法 - 英文");
        break;
    }
    
    QIcon icon = loadIcon(iconName + QStringLiteral(".svg"));
    if (icon.isNull()) {
        icon = loadIcon(iconName + QStringLiteral(".png"));
    }
    if (icon.isNull()) {
        icon = loadIcon(iconName + QStringLiteral(".ico"));
    }
    
    if (!icon.isNull()) {
        trayIcon_->setIcon(icon);
    }
    trayIcon_->setToolTip(tooltip);
}

void TrayManager::show() {
    if (trayIcon_) {
        trayIcon_->show();
    }
}

void TrayManager::hide() {
    if (trayIcon_) {
        trayIcon_->hide();
    }
}

void TrayManager::createMenu() {
    trayMenu_ = new QMenu();
    
    toggleModeAction_ = trayMenu_->addAction(QStringLiteral("切换中/英文"));
    connect(toggleModeAction_, &QAction::triggered, this, &TrayManager::onToggleMode);
    
    settingsAction_ = trayMenu_->addAction(QStringLiteral("设置..."));
    settingsAction_->setEnabled(false);
    connect(settingsAction_, &QAction::triggered, this, &TrayManager::onOpenSettings);
    
    trayMenu_->addSeparator();
    
    aboutAction_ = trayMenu_->addAction(QStringLiteral("关于素言"));
    connect(aboutAction_, &QAction::triggered, this, &TrayManager::onShowAbout);
    
    exitAction_ = trayMenu_->addAction(QStringLiteral("退出"));
    connect(exitAction_, &QAction::triggered, this, &TrayManager::onExit);
    
    if (trayIcon_) {
        trayIcon_->setContextMenu(trayMenu_);
    }
}

QIcon TrayManager::loadIcon(const QString& name) {
    QString iconPath = QString::fromStdString(resourcePath_) + QStringLiteral("/") + name;
    QIcon icon(iconPath);
    
    if (icon.isNull()) {
        iconPath = QString::fromStdString(resourcePath_) + QStringLiteral("/status/") + name;
        icon = QIcon(iconPath);
    }
    
    return icon;
}

void TrayManager::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        emit toggleModeRequested();
    }
}

void TrayManager::onToggleMode() {
    emit toggleModeRequested();
}

void TrayManager::onOpenSettings() {
    emit openSettingsRequested();
}

void TrayManager::onShowAbout() {
    emit showAboutRequested();
}

void TrayManager::onExit() {
    emit exitRequested();
}

} // namespace suyan
