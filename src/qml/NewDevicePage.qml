import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import io.github.timpalpant.kvault

FormCard.FormCardPage {
    id: root

    title: i18n("Verify this device")

    /// Explanation supplied by the server challenge.
    property string message: ""

    Component.onCompleted: codeField.forceActiveFocus()

    function submit() {
        errorMessage.visible = false;
        VaultManager.submitNewDeviceCode(codeField.text);
    }

    Connections {
        target: VaultManager

        function onLoginFailed(text) {
            errorMessage.text = text;
            errorMessage.visible = true;
            codeField.selectAll();
            codeField.forceActiveFocus();
        }
    }

    Kirigami.Icon {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Kirigami.Units.gridUnit * 2
        source: "mail-mark-unread"
        implicitWidth: Kirigami.Units.iconSizes.huge
        implicitHeight: Kirigami.Units.iconSizes.huge
    }

    Kirigami.Heading {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Kirigami.Units.largeSpacing
        text: i18n("Check your email")
        level: 2
    }

    QQC2.Label {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit * 2
        Layout.rightMargin: Kirigami.Units.gridUnit * 2
        Layout.bottomMargin: Kirigami.Units.largeSpacing
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: Kirigami.Theme.disabledTextColor
        text: root.message.length > 0 ? root.message : i18n("This device is new to your account, so the server sent a verification code to your email address.")
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
        FormCard.FormTextFieldDelegate {
            id: codeField
            label: i18n("Verification code")
            inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhNoPredictiveText
            enabled: !VaultManager.busy
            onAccepted: root.submit()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: VaultManager.busy ? i18n("Verifying…") : i18n("Continue")
            icon.name: "unlock"
            enabled: !VaultManager.busy && codeField.text.length > 0
            onClicked: root.submit()
        }
    }

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.FormButtonDelegate {
            text: i18n("Send another code")
            description: i18n("Ask the server to email a fresh verification code.")
            icon.name: "mail-send"
            enabled: !VaultManager.busy
            onClicked: {
                codeField.text = "";
                VaultManager.resendNewDeviceCode();
            }
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: i18n("Cancel")
            icon.name: "dialog-cancel"
            onClicked: VaultManager.cancelLogin()
        }
    }

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.FormTextDelegate {
            text: i18n("Why is this being asked?")
            description: i18n("Bitwarden verifies logins from devices it has not seen before. "
                + "Once this device is known, you will not be asked again.")
        }
    }

    QQC2.BusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Kirigami.Units.largeSpacing
        running: VaultManager.busy
        visible: running
    }
}
