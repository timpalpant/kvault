import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import io.github.timpalpant.kvault

FormCard.FormCardPage {
    id: root

    title: i18n("Two-step login")

    /// TwoFactorProvider ids offered by the server.
    required property var providers

    property int selectedProvider: 0

    // Provider ids as defined by the server.
    readonly property int authenticatorProvider: 0
    readonly property int emailProvider: 1
    readonly property int duoProvider: 2
    readonly property int yubiKeyProvider: 3
    readonly property int webAuthnProvider: 7

    function providerName(id) {
        switch (id) {
        case authenticatorProvider:
            return i18n("Authenticator app");
        case emailProvider:
            return i18n("Email");
        case duoProvider:
            return i18n("Duo");
        case yubiKeyProvider:
            return i18n("YubiKey OTP");
        case webAuthnProvider:
            return i18n("Security key (WebAuthn)");
        default:
            return i18n("Provider %1", id);
        }
    }

    /// Providers that need a browser or a proprietary SDK cannot work here.
    function providerSupported(id) {
        return id === authenticatorProvider || id === emailProvider || id === yubiKeyProvider;
    }

    readonly property var supportedProviders: providers.filter(providerSupported)

    Component.onCompleted: {
        if (supportedProviders.length > 0) {
            selectedProvider = supportedProviders[0];
            if (selectedProvider === emailProvider) {
                VaultManager.requestEmailCode();
            }
        }
        codeField.forceActiveFocus();
    }

    function submit() {
        errorMessage.visible = false;
        VaultManager.submitTwoFactor(codeField.text, selectedProvider, rememberSwitch.checked);
    }

    Connections {
        target: VaultManager

        function onLoginFailed(message) {
            errorMessage.text = message;
            errorMessage.visible = true;
            codeField.selectAll();
            codeField.forceActiveFocus();
        }
    }

    Kirigami.Icon {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Kirigami.Units.gridUnit * 2
        source: "security-medium"
        implicitWidth: Kirigami.Units.iconSizes.huge
        implicitHeight: Kirigami.Units.iconSizes.huge
    }

    Kirigami.Heading {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Kirigami.Units.largeSpacing
        Layout.bottomMargin: Kirigami.Units.largeSpacing
        text: i18n("Enter your verification code")
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

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.largeSpacing
        type: Kirigami.MessageType.Warning
        visible: root.supportedProviders.length === 0
        text: i18n("This account uses a two-step method this app cannot handle "
            + "(such as Duo or a security key). Add an authenticator app or email "
            + "as a second method in the web vault, then try again.")
    }

    FormCard.FormCard {
        visible: root.supportedProviders.length > 1

        Repeater {
            model: root.supportedProviders

            FormCard.FormRadioDelegate {
                required property int modelData

                text: root.providerName(modelData)
                checked: root.selectedProvider === modelData
                onToggled: {
                    if (checked) {
                        root.selectedProvider = modelData;
                        if (modelData === root.emailProvider) {
                            VaultManager.requestEmailCode();
                        }
                    }
                }
            }
        }
    }

    FormCard.FormCard {
        Layout.topMargin: root.supportedProviders.length > 1 ? Kirigami.Units.largeSpacing : 0
        visible: root.supportedProviders.length > 0

        FormCard.FormTextFieldDelegate {
            id: codeField
            label: root.selectedProvider === root.yubiKeyProvider ? i18n("Touch your YubiKey") : i18n("Verification code")
            enabled: !VaultManager.busy
            onAccepted: root.submit()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            id: rememberSwitch
            text: i18n("Remember this device")
            description: i18n("Skip the second step on this computer next time.")
            checked: true
        }

        FormCard.FormDelegateSeparator {
            visible: root.selectedProvider === root.emailProvider
        }

        FormCard.FormButtonDelegate {
            visible: root.selectedProvider === root.emailProvider
            text: i18n("Send another code")
            icon.name: "mail-send"
            onClicked: VaultManager.requestEmailCode()
        }
    }

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.FormButtonDelegate {
            visible: root.supportedProviders.length > 0
            text: VaultManager.busy ? i18n("Verifying…") : i18n("Continue")
            icon.name: "unlock"
            enabled: !VaultManager.busy && codeField.text.length > 0
            onClicked: root.submit()
        }

        FormCard.FormDelegateSeparator {
            visible: root.supportedProviders.length > 0
        }

        FormCard.FormButtonDelegate {
            text: i18n("Cancel")
            icon.name: "dialog-cancel"
            onClicked: VaultManager.cancelLogin()
        }
    }
}
