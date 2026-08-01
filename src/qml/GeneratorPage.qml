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

    title: i18n("Generator")

    PasswordGenerator {
        id: generator
    }

    readonly property bool passphraseMode: generator.mode === PasswordGenerator.Passphrase

    actions: [
        Kirigami.Action {
            text: i18n("Regenerate")
            icon.name: "view-refresh"
            shortcut: "Ctrl+R"
            onTriggered: generator.regenerate()
        },
        Kirigami.Action {
            text: i18n("Copy")
            icon.name: "edit-copy"
            shortcut: "Ctrl+C"
            onTriggered: {
                ClipboardHelper.copySecret(generator.value);
                applicationWindow().showMessage(i18n("Password copied"), Kirigami.MessageType.Positive);
            }
        }
    ]

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.AbstractFormDelegate {
            Layout.fillWidth: true
            background: null

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.largeSpacing

                QQC2.TextArea {
                    Layout.fillWidth: true
                    text: generator.value
                    readOnly: true
                    wrapMode: TextEdit.WrapAnywhere
                    font.family: "monospace"
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.2
                    horizontalAlignment: TextEdit.AlignHCenter
                }

                PasswordStrengthBar {
                    Layout.fillWidth: true
                    strength: generator.strength
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Kirigami.Units.largeSpacing

                    QQC2.Button {
                        text: i18n("Regenerate")
                        icon.name: "view-refresh"
                        onClicked: generator.regenerate()
                    }

                    QQC2.Button {
                        text: i18n("Copy")
                        icon.name: "edit-copy"
                        onClicked: {
                            ClipboardHelper.copySecret(generator.value);
                            applicationWindow().showMessage(i18n("Password copied"), Kirigami.MessageType.Positive);
                        }
                    }
                }
            }
        }
    }

    FormCard.FormHeader {
        title: i18n("Type")
    }

    FormCard.FormCard {
        FormCard.FormRadioDelegate {
            text: i18n("Password")
            description: i18n("A random string of characters.")
            checked: !root.passphraseMode
            onToggled: {
                if (checked) {
                    generator.mode = PasswordGenerator.Password;
                }
            }
        }

        FormCard.FormRadioDelegate {
            text: i18n("Passphrase")
            description: i18n("Several random words, easier to type and remember.")
            checked: root.passphraseMode
            onToggled: {
                if (checked) {
                    generator.mode = PasswordGenerator.Passphrase;
                }
            }
        }
    }

    // ------------------------------------------------------------------
    FormCard.FormHeader {
        visible: !root.passphraseMode
        title: i18n("Password options")
    }

    FormCard.FormCard {
        visible: !root.passphraseMode

        FormCard.FormSpinBoxDelegate {
            label: i18n("Length")
            from: 5
            to: 128
            value: generator.length
            onValueChanged: generator.length = value
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormCheckDelegate {
            text: i18n("Uppercase letters (A-Z)")
            checked: generator.useUppercase
            onToggled: generator.useUppercase = checked
        }

        FormCard.FormCheckDelegate {
            text: i18n("Lowercase letters (a-z)")
            checked: generator.useLowercase
            onToggled: generator.useLowercase = checked
        }

        FormCard.FormCheckDelegate {
            text: i18n("Digits (0-9)")
            checked: generator.useDigits
            onToggled: generator.useDigits = checked
        }

        FormCard.FormCheckDelegate {
            text: i18n("Symbols (!@#$%^&*)")
            checked: generator.useSpecial
            onToggled: generator.useSpecial = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormCheckDelegate {
            text: i18n("Avoid ambiguous characters")
            description: i18n("Leaves out characters that are easy to confuse, such as I, l, O, 0 and 1.")
            checked: generator.avoidAmbiguous
            onToggled: generator.avoidAmbiguous = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSpinBoxDelegate {
            label: i18n("Minimum digits")
            from: 0
            to: 9
            value: generator.minDigits
            enabled: generator.useDigits
            onValueChanged: generator.minDigits = value
        }

        FormCard.FormSpinBoxDelegate {
            label: i18n("Minimum symbols")
            from: 0
            to: 9
            value: generator.minSpecial
            enabled: generator.useSpecial
            onValueChanged: generator.minSpecial = value
        }
    }

    // ------------------------------------------------------------------
    FormCard.FormHeader {
        visible: root.passphraseMode
        title: i18n("Passphrase options")
    }

    FormCard.FormCard {
        visible: root.passphraseMode

        FormCard.FormSpinBoxDelegate {
            label: i18n("Number of words")
            from: 3
            to: 20
            value: generator.wordCount
            onValueChanged: generator.wordCount = value
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextFieldDelegate {
            label: i18n("Word separator")
            text: generator.separator
            maximumLength: 3
            onTextChanged: generator.separator = text
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormCheckDelegate {
            text: i18n("Capitalise each word")
            checked: generator.capitalise
            onToggled: generator.capitalise = checked
        }

        FormCard.FormCheckDelegate {
            text: i18n("Include a number")
            checked: generator.includeNumber
            onToggled: generator.includeNumber = checked
        }
    }

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.FormTextDelegate {
            text: i18n("Passphrases use the EFF long wordlist")
            description: i18np("%1 word to choose from, giving about 12.9 bits of entropy per word.", "%1 words to choose from, giving about 12.9 bits of entropy per word.", 7776)
        }
    }

    Item {
        Layout.preferredHeight: Kirigami.Units.gridUnit
    }
}
