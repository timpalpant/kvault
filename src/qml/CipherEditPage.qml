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

    readonly property var editor: VaultManager.editor

    readonly property int typeLogin: 1
    readonly property int typeSecureNote: 2
    readonly property int typeCard: 3
    readonly property int typeIdentity: 4
    readonly property int typeSshKey: 5

    title: editor.isNew ? i18n("New %1", IconHelper.labelForType(editor.type).toLowerCase()) : i18n("Edit item")

    function save() {
        if (nameField.text.trim().length === 0) {
            nameField.forceActiveFocus();
            return;
        }
        VaultManager.saveEditor();
    }

    Component.onCompleted: nameField.forceActiveFocus()

    Connections {
        target: VaultManager

        function onCipherSaved(cipherId) {
            // Swap the form back out for the saved item, in the same column.
            applicationWindow().pageStack.replace(Qt.resolvedUrl("CipherDetailPage.qml"), {
                cipherId: cipherId
            });
        }
    }

    actions: [
        Kirigami.Action {
            text: i18n("Save")
            icon.name: "document-save"
            enabled: !VaultManager.busy && nameField.text.trim().length > 0
            shortcut: "Ctrl+S"
            onTriggered: root.save()
        },
        Kirigami.Action {
            text: i18n("Cancel")
            icon.name: "dialog-cancel"
            onTriggered: {
                // A new item has nothing to go back to, so close the column.
                // An edit returns to the item it came from.
                if (root.editor.isNew) {
                    applicationWindow().pageStack.pop();
                } else {
                    applicationWindow().pageStack.replace(Qt.resolvedUrl("CipherDetailPage.qml"), {
                        cipherId: root.editor.cipherId
                    });
                }
            }
        }
    ]

    // ------------------------------------------------------------------
    FormCard.FormHeader {
        title: i18n("Item")
    }

    FormCard.FormCard {
        FormCard.FormTextFieldDelegate {
            id: nameField
            label: i18n("Name")
            text: root.editor.name
            onTextEdited: root.editor.name = text
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormComboBoxDelegate {
            id: folderCombo

            text: i18n("Folder")
            displayMode: FormCard.FormComboBoxDelegate.ComboBox
            textRole: "name"
            valueRole: "folderId"

            // A synthetic "no folder" entry sits above the real folders.
            model: {
                const entries = [
                    {
                        folderId: "",
                        name: i18n("No folder")
                    }
                ];
                const folders = VaultManager.folders;
                for (let i = 0; i < folders.rowCount(); ++i) {
                    const index = folders.index(i, 0);
                    entries.push({
                        folderId: folders.data(index, FolderModel.IdRole),
                        name: folders.data(index, FolderModel.NameRole)
                    });
                }
                return entries;
            }

            Component.onCompleted: currentIndex = indexOfValue(root.editor.folderId)
            onCurrentValueChanged: root.editor.folderId = currentValue ?? ""
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("Favorite")
            checked: root.editor.favorite
            onToggled: root.editor.favorite = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("Ask for the master password before revealing")
            description: i18n("Other clients will prompt again before showing this item's secrets.")
            checked: root.editor.reprompt === 1
            onToggled: root.editor.reprompt = checked ? 1 : 0
        }
    }

    // ------------------------------------------------------------------
    // Login
    // ------------------------------------------------------------------
    FormCard.FormHeader {
        visible: root.editor.type === root.typeLogin
        title: i18n("Login")
    }

    FormCard.FormCard {
        visible: root.editor.type === root.typeLogin

        FormCard.FormTextFieldDelegate {
            label: i18n("Username")
            text: root.editor.username
            onTextEdited: root.editor.username = text
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormPasswordFieldDelegate {
            id: passwordField
            label: i18n("Password")
            text: root.editor.password
            onTextEdited: root.editor.password = text
        }

        FormCard.AbstractFormDelegate {
            Layout.fillWidth: true
            background: null
            visible: root.editor.password.length > 0
            contentItem: PasswordStrengthBar {
                strength: IconHelper.passwordStrength(root.editor.password)
            }
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: i18n("Generate a password…")
            icon.name: "roll"
            onClicked: generatorSheet.open()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextFieldDelegate {
            label: i18n("Authenticator key")
            text: root.editor.totp
            onTextEdited: root.editor.totp = text
            statusMessage: i18n("A base32 secret or an otpauth:// URI")
            status: Kirigami.MessageType.Information
        }
    }

    FormCard.FormHeader {
        visible: root.editor.type === root.typeLogin
        title: i18n("Websites")
    }

    FormCard.FormCard {
        visible: root.editor.type === root.typeLogin

        Repeater {
            model: root.editor.uris

            FormCard.AbstractFormDelegate {
                required property var modelData
                required property int index

                Layout.fillWidth: true
                background: null

                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.TextField {
                        Layout.fillWidth: true
                        text: modelData.uri
                        placeholderText: i18n("https://example.com")
                        onEditingFinished: root.editor.setUri(index, text)
                    }

                    QQC2.ToolButton {
                        icon.name: "list-remove"
                        display: QQC2.AbstractButton.IconOnly
                        text: i18n("Remove")
                        onClicked: root.editor.removeUri(index)

                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }
            }
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: i18n("Add website")
            icon.name: "list-add"
            onClicked: root.editor.addUri()
        }
    }

    // ------------------------------------------------------------------
    // Card
    // ------------------------------------------------------------------
    FormCard.FormHeader {
        visible: root.editor.type === root.typeCard
        title: i18n("Card")
    }

    FormCard.FormCard {
        visible: root.editor.type === root.typeCard

        FormCard.FormTextFieldDelegate {
            label: i18n("Cardholder name")
            text: root.editor.cardholderName
            onTextEdited: root.editor.cardholderName = text
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormComboBoxDelegate {
            text: i18n("Brand")
            displayMode: FormCard.FormComboBoxDelegate.ComboBox
            model: ["", "Visa", "Mastercard", "American Express", "Discover", "Diners Club", "JCB", "Maestro", "UnionPay", "RuPay", "Other"]
            Component.onCompleted: currentIndex = Math.max(0, model.indexOf(root.editor.cardBrand))
            onCurrentTextChanged: root.editor.cardBrand = currentText
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormPasswordFieldDelegate {
            label: i18n("Number")
            text: root.editor.cardNumber
            onTextEdited: root.editor.cardNumber = text
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextFieldDelegate {
            label: i18n("Expiry month")
            text: root.editor.cardExpMonth
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.editor.cardExpMonth = text
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextFieldDelegate {
            label: i18n("Expiry year")
            text: root.editor.cardExpYear
            inputMethodHints: Qt.ImhDigitsOnly
            onTextEdited: root.editor.cardExpYear = text
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormPasswordFieldDelegate {
            label: i18n("Security code")
            text: root.editor.cardCode
            onTextEdited: root.editor.cardCode = text
        }
    }

    // ------------------------------------------------------------------
    // Identity
    // ------------------------------------------------------------------
    FormCard.FormHeader {
        visible: root.editor.type === root.typeIdentity
        title: i18n("Identity")
    }

    FormCard.FormCard {
        visible: root.editor.type === root.typeIdentity

        FormCard.FormTextFieldDelegate {
            label: i18n("Title")
            text: root.editor.identityTitle
            onTextEdited: root.editor.identityTitle = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextFieldDelegate {
            label: i18n("First name")
            text: root.editor.firstName
            onTextEdited: root.editor.firstName = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextFieldDelegate {
            label: i18n("Middle name")
            text: root.editor.middleName
            onTextEdited: root.editor.middleName = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextFieldDelegate {
            label: i18n("Last name")
            text: root.editor.lastName
            onTextEdited: root.editor.lastName = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextFieldDelegate {
            label: i18n("Username")
            text: root.editor.identityUsername
            onTextEdited: root.editor.identityUsername = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextFieldDelegate {
            label: i18n("Company")
            text: root.editor.company
            onTextEdited: root.editor.company = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextFieldDelegate {
            label: i18n("Email")
            text: root.editor.email
            inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
            onTextEdited: root.editor.email = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextFieldDelegate {
            label: i18n("Phone")
            text: root.editor.phone
            inputMethodHints: Qt.ImhDialableCharactersOnly
            onTextEdited: root.editor.phone = text
        }
    }

    FormCard.FormHeader {
        visible: root.editor.type === root.typeIdentity
        title: i18n("Address")
    }

    FormCard.FormCard {
        visible: root.editor.type === root.typeIdentity

        FormCard.FormTextFieldDelegate {
            label: i18n("Address line 1")
            text: root.editor.address1
            onTextEdited: root.editor.address1 = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextFieldDelegate {
            label: i18n("Address line 2")
            text: root.editor.address2
            onTextEdited: root.editor.address2 = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextFieldDelegate {
            label: i18n("Address line 3")
            text: root.editor.address3
            onTextEdited: root.editor.address3 = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextFieldDelegate {
            label: i18n("City")
            text: root.editor.city
            onTextEdited: root.editor.city = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextFieldDelegate {
            label: i18n("State or province")
            text: root.editor.state
            onTextEdited: root.editor.state = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextFieldDelegate {
            label: i18n("Postal code")
            text: root.editor.postalCode
            onTextEdited: root.editor.postalCode = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextFieldDelegate {
            label: i18n("Country")
            text: root.editor.country
            onTextEdited: root.editor.country = text
        }
    }

    FormCard.FormHeader {
        visible: root.editor.type === root.typeIdentity
        title: i18n("Identification")
    }

    FormCard.FormCard {
        visible: root.editor.type === root.typeIdentity

        FormCard.FormPasswordFieldDelegate {
            label: i18n("Social security number")
            text: root.editor.ssn
            onTextEdited: root.editor.ssn = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormPasswordFieldDelegate {
            label: i18n("Passport number")
            text: root.editor.passportNumber
            onTextEdited: root.editor.passportNumber = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormPasswordFieldDelegate {
            label: i18n("Licence number")
            text: root.editor.licenseNumber
            onTextEdited: root.editor.licenseNumber = text
        }
    }

    // ------------------------------------------------------------------
    // SSH key
    // ------------------------------------------------------------------
    FormCard.FormHeader {
        visible: root.editor.type === root.typeSshKey
        title: i18n("SSH key")
    }

    FormCard.FormCard {
        visible: root.editor.type === root.typeSshKey

        FormCard.FormTextAreaDelegate {
            label: i18n("Private key")
            text: root.editor.sshPrivateKey
            onTextChanged: root.editor.sshPrivateKey = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextAreaDelegate {
            label: i18n("Public key")
            text: root.editor.sshPublicKey
            onTextChanged: root.editor.sshPublicKey = text
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormTextFieldDelegate {
            label: i18n("Fingerprint")
            text: root.editor.sshFingerprint
            onTextEdited: root.editor.sshFingerprint = text
        }
    }

    // ------------------------------------------------------------------
    // Notes and custom fields, shared by every type
    // ------------------------------------------------------------------
    FormCard.FormHeader {
        title: i18n("Notes")
    }

    FormCard.FormCard {
        FormCard.FormTextAreaDelegate {
            label: i18n("Notes")
            text: root.editor.notes
            onTextChanged: root.editor.notes = text
        }
    }

    FormCard.FormHeader {
        title: i18n("Custom fields")
    }

    FormCard.FormCard {
        Repeater {
            model: root.editor.fields

            FormCard.AbstractFormDelegate {
                required property var modelData
                required property int index

                Layout.fillWidth: true
                background: null

                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.TextField {
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 8
                        text: modelData.name
                        placeholderText: i18n("Name")
                        onEditingFinished: root.editor.setFieldName(index, text)
                    }

                    // Field types: 0 text, 1 hidden, 2 boolean.
                    QQC2.TextField {
                        Layout.fillWidth: true
                        visible: modelData.type !== 2
                        text: modelData.value
                        placeholderText: i18n("Value")
                        echoMode: modelData.type === 1 ? TextInput.Password : TextInput.Normal
                        onEditingFinished: root.editor.setFieldValue(index, text)
                    }

                    QQC2.CheckBox {
                        Layout.fillWidth: true
                        visible: modelData.type === 2
                        checked: modelData.value === "true"
                        onToggled: root.editor.setFieldValue(index, checked ? "true" : "false")
                    }

                    QQC2.ToolButton {
                        icon.name: "list-remove"
                        display: QQC2.AbstractButton.IconOnly
                        text: i18n("Remove field")
                        onClicked: root.editor.removeField(index)

                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }
            }
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: i18n("Add text field")
            icon.name: "list-add"
            onClicked: root.editor.addField(0)
        }

        FormCard.FormButtonDelegate {
            text: i18n("Add hidden field")
            icon.name: "list-add"
            onClicked: root.editor.addField(1)
        }

        FormCard.FormButtonDelegate {
            text: i18n("Add checkbox field")
            icon.name: "list-add"
            onClicked: root.editor.addField(2)
        }
    }

    Item {
        Layout.preferredHeight: Kirigami.Units.gridUnit
    }

    Kirigami.Dialog {
        id: generatorSheet

        title: i18n("Generate a password")
        preferredWidth: Kirigami.Units.gridUnit * 28
        standardButtons: Kirigami.Dialog.NoButton

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Use this password")
                icon.name: "dialog-ok-apply"
                onTriggered: {
                    passwordField.text = sheetGenerator.value;
                    root.editor.password = sheetGenerator.value;
                    generatorSheet.close();
                }
            },
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: generatorSheet.close()
            }
        ]

        PasswordGenerator {
            id: sheetGenerator
        }

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing

            QQC2.TextArea {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing
                text: sheetGenerator.value
                readOnly: true
                wrapMode: TextEdit.WrapAnywhere
                font.family: "monospace"
            }

            PasswordStrengthBar {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.largeSpacing
                Layout.rightMargin: Kirigami.Units.largeSpacing
                strength: sheetGenerator.strength
            }

            RowLayout {
                Layout.margins: Kirigami.Units.largeSpacing

                QQC2.ComboBox {
                    model: [i18n("Password"), i18n("Passphrase")]
                    currentIndex: sheetGenerator.mode
                    onActivated: sheetGenerator.mode = currentIndex
                }

                QQC2.SpinBox {
                    from: sheetGenerator.mode === PasswordGenerator.Passphrase ? 3 : 5
                    to: sheetGenerator.mode === PasswordGenerator.Passphrase ? 20 : 128
                    value: sheetGenerator.mode === PasswordGenerator.Passphrase ? sheetGenerator.wordCount : sheetGenerator.length
                    onValueModified: {
                        if (sheetGenerator.mode === PasswordGenerator.Passphrase) {
                            sheetGenerator.wordCount = value;
                        } else {
                            sheetGenerator.length = value;
                        }
                    }
                }

                QQC2.Button {
                    text: i18n("Regenerate")
                    icon.name: "view-refresh"
                    onClicked: sheetGenerator.regenerate()
                }
            }
        }
    }
}
