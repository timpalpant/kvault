#pragma once

#include <QObject>
#include <QPointer>

class KStatusNotifierItem;
class QWindow;

namespace kvault {

/// Keeps a hidden application window reachable through the notification area.
class TrayController final : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QObject *parent = nullptr);

    void setMainWindow(QWindow *window);

    /// Hides the window and makes the notification-area item active.
    Q_INVOKABLE bool hideMainWindow();

public Q_SLOTS:
    /// Raises the window, including after an activation from another instance.
    void showMainWindow();

private:
    void ensureTrayIcon();

    QPointer<QWindow> m_mainWindow;
    KStatusNotifierItem *m_trayIcon = nullptr;
    bool m_showWhenWindowReady = false;
};

} // namespace kvault
