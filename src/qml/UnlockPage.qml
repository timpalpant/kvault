import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import io.github.timpalpant.kvault

FormCard.FormCardPage {
    id: root

    title: i18n("Locked")

    Component.onCompleted: passwordField.forceActiveFocus()

    function submit() {
        errorMessage.visible = false;
        VaultManager.unlock(passwordField.text);
    }

    Connections {
        target: VaultManager

        function onUnlockFailed(message) {
            errorMessage.text = message;
            errorMessage.visible = true;
            passwordField.selectAll();
            passwordField.forceActiveFocus();
        }
    }

    Kirigami.Icon {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Kirigami.Units.gridUnit * 3
        source: "lock"
        implicitWidth: Kirigami.Units.iconSizes.enormous
        implicitHeight: Kirigami.Units.iconSizes.enormous
    }

    Kirigami.Heading {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Kirigami.Units.largeSpacing
        text: i18n("Your vault is locked")
        level: 2
    }

    QQC2.Label {
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: Kirigami.Units.largeSpacing
        text: VaultManager.email
        color: Kirigami.Theme.disabledTextColor
    }

    Kirigami.InlineMessage {
        id: errorMessage
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.largeSpacing
        type: Kirigami.MessageType.Error
        visible: false
    }

    FormCard.FormCard {
        FormCard.FormPasswordFieldDelegate {
            id: passwordField
            label: i18n("Master password")
            enabled: !VaultManager.busy
            onAccepted: root.submit()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: VaultManager.busy ? i18n("Unlocking…") : i18n("Unlock")
            icon.name: "unlock"
            enabled: !VaultManager.busy && passwordField.text.length > 0
            onClicked: root.submit()
        }
    }

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.FormButtonDelegate {
            text: i18n("Log out")
            description: i18n("Forget this account and remove the offline copy of the vault.")
            icon.name: "system-log-out"
            onClicked: logoutPrompt.open()
        }
    }

    QQC2.BusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Kirigami.Units.largeSpacing
        running: VaultManager.busy
        visible: running
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
