import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import io.github.timpalpant.kvault

FormCard.FormCardPage {
    id: root

    // Follow the visible right-hand pane while the sidebar is shown or hidden.
    Kirigami.ColumnView.fillWidth: true
    Binding {
        target: root
        property: "width"
        value: applicationWindow().childPageWidth(root)
    }
    Binding {
        target: root.Kirigami.ColumnView.globalHeader
        property: "width"
        value: root.width
    }
    Binding {
        target: root.flickable
        property: "contentX"
        value: 0
    }

    // FormCardPage normally scrolls both axes whenever a child gains focus.
    // This page is deliberately not horizontally scrollable, and changing a
    // setting must not make its contents slide sideways after the sidebar has
    // been reopened. Keep the useful vertical focus handling only.
    function ensureVisible(item, _xOffset, yOffset) {
        const actualItemY = item.y + (yOffset ?? 0);
        const viewYPosition = item.height <= root.flickable.height
            ? Math.round(actualItemY + item.height / 2 - root.flickable.height / 2)
            : actualItemY;
        const maximumContentY = Math.max(0, root.flickable.contentHeight - root.flickable.height);
        if (actualItemY < root.flickable.contentY) {
            root.flickable.contentY = Math.max(0, viewYPosition);
        } else if (actualItemY + item.height > root.flickable.contentY + root.flickable.height) {
            root.flickable.contentY = Math.min(maximumContentY, viewYPosition);
        }
    }

    title: i18n("Settings")

    readonly property var lockTimeouts: [0, 1, 5, 15, 30, 60]

    function lockTimeoutLabel(minutes) {
        if (minutes === 0) {
            return i18n("Never");
        }
        return i18np("After %1 minute", "After %1 minutes", minutes);
    }

    readonly property var clipboardTimeouts: [0, 10, 20, 30, 60, 120]

    function clipboardLabel(seconds) {
        if (seconds === 0) {
            return i18n("Never");
        }
        return i18np("After %1 second", "After %1 seconds", seconds);
    }

    FormCard.FormHeader {
        title: i18n("Account")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            text: i18n("Signed in as")
            description: VaultManager.email
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            text: i18n("Server")
            description: VaultManager.serverUrl
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            text: i18n("Last sync")
            description: {
                const date = VaultManager.lastSync;
                if (!date || !date.getTime || isNaN(date.getTime())) {
                    return i18n("Never");
                }
                return date.toLocaleString(Qt.locale(), Locale.ShortFormat);
            }
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: VaultManager.syncing ? i18n("Syncing…") : i18n("Sync now")
            icon.name: "view-refresh"
            enabled: !VaultManager.syncing
            onClicked: VaultManager.sync()
        }
    }

    FormCard.FormHeader {
        title: i18n("Security")
    }

    FormCard.FormCard {
        FormCard.FormComboBoxDelegate {
            text: i18n("Lock when idle")
            description: i18n("Locking clears the vault keys from memory. The offline copy stays on disk.")
            displayMode: FormCard.FormComboBoxDelegate.ComboBox
            model: root.lockTimeouts.map(root.lockTimeoutLabel)
            currentIndex: root.lockTimeouts.indexOf(AppSettings.lockTimeoutMinutes)
            onActivated: AppSettings.lockTimeoutMinutes = root.lockTimeouts[currentIndex]
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("Lock when the window is minimized")
            checked: AppSettings.lockOnMinimize
            onToggled: AppSettings.lockOnMinimize = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("Close to notification tray")
            description: i18n("Closing the window keeps KVault running. It locks after the configured idle timeout.")
            checked: AppSettings.closeToTray
            onToggled: AppSettings.closeToTray = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormComboBoxDelegate {
            text: i18n("Clear the clipboard")
            description: i18n("Applies to copied passwords and other secrets.")
            displayMode: FormCard.FormComboBoxDelegate.ComboBox
            model: root.clipboardTimeouts.map(root.clipboardLabel)
            currentIndex: root.clipboardTimeouts.indexOf(AppSettings.clipboardClearSeconds)
            onActivated: AppSettings.clipboardClearSeconds = root.clipboardTimeouts[currentIndex]
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("Conceal passwords by default")
            description: i18n("Passwords are hidden until you choose to reveal them.")
            checked: AppSettings.concealPasswords
            onToggled: AppSettings.concealPasswords = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: i18n("Lock now")
            icon.name: "lock"
            onClicked: VaultManager.lock()
        }
    }

    FormCard.FormHeader {
        title: i18n("Sync")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: i18n("Sync after unlocking")
            description: i18n("Fetch changes from the server each time the vault is unlocked.")
            checked: AppSettings.syncOnUnlock
            onToggled: AppSettings.syncOnUnlock = checked
        }
    }

    FormCard.FormHeader {
        title: i18n("Session")
    }

    FormCard.FormCard {
        FormCard.FormButtonDelegate {
            text: i18n("Log out")
            description: i18n("Forget this account and delete the offline copy of the vault from this computer.")
            icon.name: "system-log-out"
            onClicked: logoutPrompt.open()
        }
    }

    FormCard.FormHeader {
        title: i18n("About")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            text: i18n("KVault %1", "0.1.1")
            description: i18n("An unofficial client for Bitwarden servers. An independent project, "
                + "not affiliated with or endorsed by Bitwarden, Inc.")
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            text: i18n("Offline data")
            description: i18n("The cached vault is stored encrypted; session tokens are kept in your wallet.")
        }
    }

    Item {
        Layout.preferredHeight: Kirigami.Units.gridUnit
    }

    Kirigami.PromptDialog {
        id: logoutPrompt

        title: i18n("Log out?")
        subtitle: i18n("The offline copy of your vault will be deleted from this computer. "
            + "Nothing is removed from the server.")
        standardButtons: Kirigami.Dialog.NoButton

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Log out")
                icon.name: "system-log-out"
                onTriggered: {
                    logoutPrompt.close();
                    VaultManager.logout();
                }
            },
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: logoutPrompt.close()
            }
        ]
    }
}
