import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import io.github.timpalpant.kvault

FormCard.FormCardPage {
    id: root

    title: i18n("Log in")

    property bool showServerField: false

    Component.onCompleted: {
        emailField.text = AppSettings.lastEmail;
        serverField.text = AppSettings.serverUrl;
        if (emailField.text.length > 0) {
            passwordField.forceActiveFocus();
        } else {
            emailField.forceActiveFocus();
        }
    }

    function submit() {
        errorMessage.visible = false;
        VaultManager.login(emailField.text, passwordField.text, serverField.text);
    }

    Connections {
        target: VaultManager

        function onLoginFailed(message) {
            errorMessage.text = message;
            errorMessage.visible = true;
            passwordField.selectAll();
            passwordField.forceActiveFocus();
        }
    }

    Kirigami.Icon {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Kirigami.Units.gridUnit * 2
        // The bundled resource, not the icon-theme name: the theme only has
        // the icon once the app is installed, so a theme lookup renders blank
        // when running from a build directory.
        source: Qt.resolvedUrl("icons/kvault.svg")
        implicitWidth: Kirigami.Units.iconSizes.enormous
        implicitHeight: Kirigami.Units.iconSizes.enormous
    }

    Kirigami.Heading {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Kirigami.Units.largeSpacing
        Layout.bottomMargin: Kirigami.Units.largeSpacing
        text: i18n("Unlock your Bitwarden vault")
        level: 2
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
            id: emailField
            label: i18n("Email address")
            inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
            enabled: !VaultManager.busy
            onAccepted: passwordField.forceActiveFocus()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormPasswordFieldDelegate {
            id: passwordField
            label: i18n("Master password")
            enabled: !VaultManager.busy
            onAccepted: root.submit()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: root.showServerField ? i18n("Use the default server") : i18n("Use a self-hosted server")
            icon.name: "network-server"
            enabled: !VaultManager.busy
            onClicked: {
                root.showServerField = !root.showServerField;
                if (!root.showServerField) {
                    serverField.text = "https://vault.bitwarden.com";
                }
            }
        }

        FormCard.FormDelegateSeparator {
            visible: root.showServerField
        }

        FormCard.FormTextFieldDelegate {
            id: serverField
            visible: root.showServerField
            label: i18n("Server URL")
            // Bitwarden's own clouds split identity and api across subdomains;
            // anything else is treated as a self-hosted origin.
            statusMessage: i18n("For example https://vault.example.com")
            status: Kirigami.MessageType.Information
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
            enabled: !VaultManager.busy
            onAccepted: root.submit()
        }
    }

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.FormButtonDelegate {
            text: VaultManager.busy ? i18n("Logging in…") : i18n("Log in")
            icon.name: "unlock"
            enabled: !VaultManager.busy && emailField.text.length > 0 && passwordField.text.length > 0
            onClicked: root.submit()
        }
    }

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.FormTextDelegate {
            text: i18n("Your master password never leaves this device.")
            description: i18n("It is used locally to derive the keys that decrypt your vault. "
                + "Only a one-way hash of it is sent to the server.")
        }
    }

    QQC2.BusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Kirigami.Units.largeSpacing
        running: VaultManager.busy
        visible: running
    }
}
