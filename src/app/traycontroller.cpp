#include "traycontroller.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QMenu>
#include <QWindow>

#include <KLocalizedString>
#include <KStatusNotifierItem>

namespace kvault {

TrayController::TrayController(QObject *parent)
    : QObject(parent)
{}

void TrayController::setMainWindow(QWindow *window)
{
    m_mainWindow = window;
    if (m_trayIcon) {
        m_trayIcon->setAssociatedWindow(window);
    }
    if (m_showWhenWindowReady) {
        m_showWhenWindowReady = false;
        showMainWindow();
    }
}

bool TrayController::hideMainWindow()
{
    if (!m_mainWindow) {
        return false;
    }

    ensureTrayIcon();
    m_trayIcon->setStatus(KStatusNotifierItem::Active);
    m_mainWindow->hide();
    return true;
}

void TrayController::showMainWindow()
{
    if (!m_mainWindow) {
        m_showWhenWindowReady = true;
        return;
    }

    m_mainWindow->show();
    m_mainWindow->raise();
    m_mainWindow->requestActivate();
    if (m_trayIcon) {
        m_trayIcon->setStatus(KStatusNotifierItem::Passive);
    }
}

void TrayController::ensureTrayIcon()
{
    if (m_trayIcon) {
        return;
    }

    m_trayIcon = new KStatusNotifierItem(QStringLiteral("kvault"), this);
    m_trayIcon->setCategory(KStatusNotifierItem::ApplicationStatus);
    m_trayIcon->setTitle(i18n("KVault"));
    // Plasma Vault ships this symbolic Breeze icon. Naming it lets the tray
    // load a size and color treatment appropriate for the active theme.
    const QString trayIconName = QStringLiteral("plasmavault-symbolic");
    if (QIcon::hasThemeIcon(trayIconName)) {
        m_trayIcon->setIconByName(trayIconName);
        m_trayIcon->setToolTipIconByName(trayIconName);
        m_trayIcon->setToolTipTitle(i18n("KVault"));
        m_trayIcon->setToolTipSubTitle(i18n("Password Manager"));
    } else {
        const QIcon appIcon = QApplication::windowIcon();
        m_trayIcon->setIconByPixmap(appIcon);
        m_trayIcon->setToolTip(appIcon, i18n("KVault"), i18n("Password Manager"));
    }
    m_trayIcon->setStatus(KStatusNotifierItem::Passive);
    m_trayIcon->setStandardActionsEnabled(false);

    auto *menu = new QMenu;
    QAction *showAction = menu->addAction(i18n("Show KVault"));
    connect(showAction, &QAction::triggered, this, &TrayController::showMainWindow);
    QAction *quitAction = menu->addAction(i18n("Quit"));
    connect(quitAction, &QAction::triggered, this, []() { QCoreApplication::quit(); });
    m_trayIcon->setContextMenu(menu);

    connect(m_trayIcon, &KStatusNotifierItem::activateRequested, this, [this](bool active, const QPoint &) {
        if (active) {
            showMainWindow();
        } else {
            hideMainWindow();
        }
    });

    m_trayIcon->setAssociatedWindow(m_mainWindow);
}

} // namespace kvault
