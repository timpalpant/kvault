import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as Dialogs
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

    required property string cipherId

    property var details: ({})

    // Matches FormCard's own default maximumWidth so the header and banners
    // line up with the cards instead of hugging the left edge.
    readonly property real cardMaximumWidth: Kirigami.Units.gridUnit * 30

    // Item type ids, mirroring CipherType on the C++ side.
    readonly property int typeLogin: 1
    readonly property int typeSecureNote: 2
    readonly property int typeCard: 3
    readonly property int typeIdentity: 4
    readonly property int typeSshKey: 5

    readonly property int type: details.type ?? typeLogin
    readonly property bool inTrash: details.inTrash ?? false

    title: details.name ?? ""

    function reload() {
        details = VaultManager.cipherDetails(cipherId);
    }

    Component.onCompleted: reload()

    Connections {
        target: VaultManager

        function onCipherSaved(savedId) {
            if (savedId === root.cipherId) {
                root.reload();
            }
        }
    }

    actions: [
        Kirigami.Action {
            visible: !root.inTrash
            text: i18n("Edit")
            icon.name: "document-edit"
            enabled: root.details.editable ?? true
            onTriggered: {
                if (VaultManager.beginEdit(root.cipherId)) {
                    // Replace rather than push: editing is a mode change on this
                    // item, not a step deeper into the vault.
                    applicationWindow().pageStack.replace(Qt.resolvedUrl("CipherEditPage.qml"));
                }
            }
        },
        Kirigami.Action {
            visible: !root.inTrash
            text: root.details.favorite ? i18n("Remove from favorites") : i18n("Add to favorites")
            icon.name: root.details.favorite ? "favorite" : "bookmark-new"
            onTriggered: VaultManager.setFavorite(root.cipherId, !root.details.favorite)
        },
        Kirigami.Action {
            visible: root.inTrash
            text: i18n("Restore")
            icon.name: "edit-undo"
            onTriggered: {
                VaultManager.restoreFromTrash(root.cipherId);
                root.reload();
            }
        },
        Kirigami.Action {
            text: root.inTrash ? i18n("Delete permanently") : i18n("Move to trash")
            icon.name: "edit-delete"
            onTriggered: deletePrompt.open()
        }
    ]

    // ------------------------------------------------------------------
    // Header
    // ------------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        Layout.maximumWidth: root.cardMaximumWidth
        Layout.alignment: Qt.AlignHCenter
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.topMargin: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        CipherTypeIcon {
            name: root.details.name ?? ""
            type: root.type
            implicitWidth: Kirigami.Units.iconSizes.large
            implicitHeight: Kirigami.Units.iconSizes.large
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Kirigami.Heading {
                Layout.fillWidth: true
                text: root.details.name ?? ""
                level: 2
                elide: Text.ElideRight
            }

            QQC2.Label {
                text: IconHelper.labelForType(root.type)
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
            }
        }
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        Layout.maximumWidth: root.cardMaximumWidth
        Layout.alignment: Qt.AlignHCenter
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.topMargin: Kirigami.Units.largeSpacing
        visible: root.inTrash
        type: Kirigami.MessageType.Warning
        text: i18n("This item is in the trash.")
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        Layout.maximumWidth: root.cardMaximumWidth
        Layout.alignment: Qt.AlignHCenter
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.topMargin: Kirigami.Units.largeSpacing
        visible: root.details.decryptionFailed ?? false
        type: Kirigami.MessageType.Error
        text: i18n("Some fields of this item could not be decrypted and are shown as empty.")
    }

    // ------------------------------------------------------------------
    // Login
    // ------------------------------------------------------------------
    FormCard.FormHeader {
        visible: root.type === root.typeLogin
        title: i18n("Login")
    }

    FormCard.FormCard {
        visible: root.type === root.typeLogin

        FormCard.AbstractFormDelegate {
            Layout.fillWidth: true
            visible: (root.details.username ?? "").length > 0
            background: null
            contentItem: CopyableRow {
                label: i18n("Username")
                value: root.details.username ?? ""
            }
        }

        FormCard.AbstractFormDelegate {
            Layout.fillWidth: true
            visible: (root.details.password ?? "").length > 0
            background: null
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                SecretField {
                    Layout.fillWidth: true
                    label: i18n("Password")
                    value: root.details.password ?? ""
                }

                PasswordStrengthBar {
                    Layout.fillWidth: true
                    showLabel: false
                    strength: IconHelper.passwordStrength(root.details.password ?? "")
                }
            }
        }

        FormCard.AbstractFormDelegate {
            Layout.fillWidth: true
            visible: (root.details.totp ?? "").length > 0
            background: null
            contentItem: TotpRing {
                seed: root.details.totp ?? ""
            }
        }

        Repeater {
            model: root.details.uris ?? []

            FormCard.AbstractFormDelegate {
                required property var modelData
                required property int index

                Layout.fillWidth: true
                background: null
                contentItem: CopyableRow {
                    label: index === 0 ? i18n("Website") : i18n("Website %1", index + 1)
                    value: modelData.uri
                    isLink: true
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // Card
    // ------------------------------------------------------------------
    FormCard.FormHeader {
        visible: root.type === root.typeCard
        title: i18n("Card")
    }

    FormCard.FormCard {
        visible: root.type === root.typeCard

        FormCard.AbstractFormDelegate {
            Layout.fillWidth: true
            visible: (root.details.cardholderName ?? "").length > 0
            background: null
            contentItem: CopyableRow {
                label: i18n("Cardholder name")
                value: root.details.cardholderName ?? ""
            }
        }

        FormCard.AbstractFormDelegate {
            Layout.fillWidth: true
            visible: (root.details.cardBrand ?? "").length > 0
            background: null
            contentItem: CopyableRow {
                label: i18n("Brand")
                value: root.details.cardBrand ?? ""
            }
        }

        FormCard.AbstractFormDelegate {
            Layout.fillWidth: true
            visible: (root.details.cardNumber ?? "").length > 0
            background: null
            contentItem: SecretField {
                label: i18n("Number")
                value: root.details.cardNumber ?? ""
            }
        }

        FormCard.AbstractFormDelegate {
            Layout.fillWidth: true
            visible: (root.details.cardExpMonth ?? "").length > 0 || (root.details.cardExpYear ?? "").length > 0
            background: null
            contentItem: CopyableRow {
                label: i18n("Expires")
                value: {
                    const month = root.details.cardExpMonth ?? "";
                    const year = root.details.cardExpYear ?? "";
                    if (month.length > 0 && year.length > 0) {
                        return `${month.padStart(2, "0")} / ${year}`;
                    }
                    return month.length > 0 ? month : year;
                }
            }
        }

        FormCard.AbstractFormDelegate {
            Layout.fillWidth: true
            visible: (root.details.cardCode ?? "").length > 0
            background: null
            contentItem: SecretField {
                label: i18n("Security code")
                value: root.details.cardCode ?? ""
            }
        }
    }

    // ------------------------------------------------------------------
    // Identity
    // ------------------------------------------------------------------
    FormCard.FormHeader {
        visible: root.type === root.typeIdentity
        title: i18n("Identity")
    }

    FormCard.FormCard {
        visible: root.type === root.typeIdentity

        Repeater {
            model: root.type !== root.typeIdentity ? [] : [
                {
                    label: i18n("Title"),
                    value: root.details.identityTitle ?? ""
                },
                {
                    label: i18n("First name"),
                    value: root.details.firstName ?? ""
                },
                {
                    label: i18n("Middle name"),
                    value: root.details.middleName ?? ""
                },
                {
                    label: i18n("Last name"),
                    value: root.details.lastName ?? ""
                },
                {
                    label: i18n("Username"),
                    value: root.details.identityUsername ?? ""
                },
                {
                    label: i18n("Company"),
                    value: root.details.company ?? ""
                },
                {
                    label: i18n("Email"),
                    value: root.details.email ?? ""
                },
                {
                    label: i18n("Phone"),
                    value: root.details.phone ?? ""
                },
                {
                    label: i18n("Address"),
                    value: root.details.address1 ?? ""
                },
                {
                    label: i18n("Address line 2"),
                    value: root.details.address2 ?? ""
                },
                {
                    label: i18n("Address line 3"),
                    value: root.details.address3 ?? ""
                },
                {
                    label: i18n("City"),
                    value: root.details.city ?? ""
                },
                {
                    label: i18n("State"),
                    value: root.details.state ?? ""
                },
                {
                    label: i18n("Postal code"),
                    value: root.details.postalCode ?? ""
                },
                {
                    label: i18n("Country"),
                    value: root.details.country ?? ""
                }
            ]

            FormCard.AbstractFormDelegate {
                required property var modelData

                Layout.fillWidth: true
                visible: modelData.value.length > 0
                background: null
                contentItem: CopyableRow {
                    label: modelData.label
                    value: modelData.value
                }
            }
        }

        // Government identifiers are concealed by default.
        Repeater {
            model: root.type !== root.typeIdentity ? [] : [
                {
                    label: i18n("Social security number"),
                    value: root.details.ssn ?? ""
                },
                {
                    label: i18n("Passport number"),
                    value: root.details.passportNumber ?? ""
                },
                {
                    label: i18n("License number"),
                    value: root.details.licenseNumber ?? ""
                }
            ]

            FormCard.AbstractFormDelegate {
                required property var modelData

                Layout.fillWidth: true
                visible: modelData.value.length > 0
                background: null
                contentItem: SecretField {
                    label: modelData.label
                    value: modelData.value
                    monospace: false
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // SSH key
    // ------------------------------------------------------------------
    FormCard.FormHeader {
        visible: root.type === root.typeSshKey
        title: i18n("SSH key")
    }

    FormCard.FormCard {
        visible: root.type === root.typeSshKey

        FormCard.AbstractFormDelegate {
            Layout.fillWidth: true
            visible: (root.details.sshFingerprint ?? "").length > 0
            background: null
            contentItem: CopyableRow {
                label: i18n("Fingerprint")
                value: root.details.sshFingerprint ?? ""
                monospace: true
            }
        }

        FormCard.AbstractFormDelegate {
            Layout.fillWidth: true
            visible: (root.details.sshPublicKey ?? "").length > 0
            background: null
            contentItem: CopyableRow {
                label: i18n("Public key")
                value: root.details.sshPublicKey ?? ""
                monospace: true
                multiline: true
            }
        }

        FormCard.AbstractFormDelegate {
            Layout.fillWidth: true
            visible: (root.details.sshPrivateKey ?? "").length > 0
            background: null
            contentItem: SecretField {
                label: i18n("Private key")
                value: root.details.sshPrivateKey ?? ""
                multiline: true
            }
        }
    }

    // ------------------------------------------------------------------
    // Notes, shared by every type
    // ------------------------------------------------------------------
    FormCard.FormHeader {
        visible: (root.details.notes ?? "").length > 0
        title: i18n("Notes")
    }

    FormCard.FormCard {
        visible: (root.details.notes ?? "").length > 0

        FormCard.AbstractFormDelegate {
            Layout.fillWidth: true
            background: null
            contentItem: CopyableRow {
                label: i18n("Notes")
                value: root.details.notes ?? ""
                multiline: true
            }
        }
    }

    // ------------------------------------------------------------------
    // Custom fields
    // ------------------------------------------------------------------
    FormCard.FormHeader {
        visible: (root.details.fields ?? []).length > 0
        title: i18n("Custom fields")
    }

    FormCard.FormCard {
        visible: (root.details.fields ?? []).length > 0

        Repeater {
            model: root.details.fields ?? []

            FormCard.AbstractFormDelegate {
                required property var modelData

                Layout.fillWidth: true
                background: null

                // Field type 1 is "hidden", 2 is "boolean".
                contentItem: Loader {
                    sourceComponent: modelData.type === 1 ? hiddenField : (modelData.type === 2 ? booleanField : textField)

                    Component {
                        id: textField
                        CopyableRow {
                            label: modelData.name
                            value: modelData.value
                        }
                    }

                    Component {
                        id: hiddenField
                        SecretField {
                            label: modelData.name
                            value: modelData.value
                            monospace: false
                        }
                    }

                    Component {
                        id: booleanField
                        RowLayout {
                            QQC2.CheckBox {
                                enabled: false
                                checked: modelData.value === "true"
                            }
                            QQC2.Label {
                                Layout.fillWidth: true
                                text: modelData.name
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // Attachments
    // ------------------------------------------------------------------
    FormCard.FormHeader {
        visible: (root.details.attachments ?? []).length > 0
        title: i18n("Attachments")
    }

    FormCard.FormCard {
        visible: (root.details.attachments ?? []).length > 0

        Repeater {
            model: root.details.attachments ?? []

            FormCard.FormButtonDelegate {
                required property var modelData

                text: modelData.fileName.length > 0 ? modelData.fileName : i18n("(unnamed file)")
                description: IconHelper.formatFileSize(modelData.size)
                icon.name: "mail-attachment"
                enabled: !VaultManager.attachments.busy
                onClicked: {
                    saveDialog.attachmentId = modelData.id;
                    saveDialog.selectedFile = "file://" + modelData.fileName;
                    saveDialog.open();
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // History and metadata
    // ------------------------------------------------------------------
    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.FormButtonDelegate {
            visible: (root.details.passwordHistory ?? []).length > 0
            text: i18n("Password history")
            description: i18np("%1 previous password", "%1 previous passwords", (root.details.passwordHistory ?? []).length)
            icon.name: "view-history"
            onClicked: applicationWindow().pageStack.push(Qt.resolvedUrl("PasswordHistoryPage.qml"), {
                entries: root.details.passwordHistory
            })
        }

        FormCard.FormTextDelegate {
            text: i18n("Last edited")
            description: {
                const date = root.details.revisionDate;
                return date && date.getTime && !isNaN(date.getTime()) ? date.toLocaleString(Qt.locale(), Locale.ShortFormat) : i18n("Unknown");
            }
        }
    }

    Item {
        Layout.preferredHeight: Kirigami.Units.gridUnit
    }

    Dialogs.FileDialog {
        id: saveDialog

        property string attachmentId: ""

        fileMode: Dialogs.FileDialog.SaveFile
        title: i18n("Save attachment")
        onAccepted: VaultManager.attachments.download(root.cipherId, attachmentId, selectedFile)
    }

    Connections {
        target: VaultManager.attachments

        function onDownloadFinished(attachmentId, success, message) {
            applicationWindow().showMessage(message, success ? Kirigami.MessageType.Positive : Kirigami.MessageType.Error);
        }
    }

    Kirigami.PromptDialog {
        id: deletePrompt

        title: root.inTrash ? i18n("Delete permanently?") : i18n("Move to trash?")
        subtitle: root.inTrash ? i18n("“%1” will be deleted from your vault. This cannot be undone.", root.details.name ?? "") : i18n("“%1” will be moved to the trash. You can restore it later.", root.details.name ?? "")
        standardButtons: Kirigami.Dialog.NoButton

        customFooterActions: [
            Kirigami.Action {
                text: root.inTrash ? i18n("Delete permanently") : i18n("Move to trash")
                icon.name: "edit-delete"
                onTriggered: {
                    deletePrompt.close();
                    if (root.inTrash) {
                        VaultManager.deleteForever(root.cipherId);
                        applicationWindow().pageStack.pop();
                    } else {
                        VaultManager.moveToTrash(root.cipherId);
                        root.reload();
                    }
                }
            },
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: deletePrompt.close()
            }
        ]
    }
}
