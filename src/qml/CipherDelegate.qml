import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.delegates as Delegates
import io.github.timpalpant.kvault


Delegates.RoundedItemDelegate {
    id: root

    required property string cipherId
    required property string name
    required property string subtitle
    required property int type
    required property bool favorite
    required property bool hasTotp
    required property bool inTrash
    required property int attachmentCount
    required property bool decryptionFailed
    required property string primaryUri

    signal activated

    onClicked: root.activated()

    Keys.onReturnPressed: root.activated()
    Keys.onEnterPressed: root.activated()

    readonly property bool showActions: !root.inTrash && (root.hovered || root.activeFocus)

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing
        // Reserve the action row's height at all times so rows do not shift as
        // the buttons appear and disappear on hover.
        implicitHeight: Math.max(Kirigami.Units.iconSizes.medium, Kirigami.Units.gridUnit * 2)

        CipherTypeIcon {
            name: root.name
            type: root.type
            opacity: root.inTrash ? 0.5 : 1
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    Layout.fillWidth: true
                    text: root.name.length > 0 ? root.name : i18n("(no name)")
                    elide: Text.ElideRight
                    opacity: root.inTrash ? 0.6 : 1
                }

                Kirigami.Icon {
                    visible: root.favorite
                    source: "favorite"
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: Kirigami.Units.iconSizes.small
                }

                Kirigami.Icon {
                    visible: root.attachmentCount > 0
                    source: "mail-attachment"
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: Kirigami.Units.iconSizes.small
                }

                Kirigami.Icon {
                    visible: root.decryptionFailed
                    source: "data-warning"
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: Kirigami.Units.iconSizes.small

                    QQC2.ToolTip.visible: warningHover.hovered
                    QQC2.ToolTip.text: i18n("Some fields could not be decrypted")
                    HoverHandler {
                        id: warningHover
                    }
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.subtitle.length > 0 ? root.subtitle : IconHelper.prettyHost(root.primaryUri)
                elide: Text.ElideRight
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
            }
        }

        // Quick copy, revealed on hover so the list stays calm at rest.
        QQC2.ToolButton {
            // The code is computed on demand rather than by a per-row timer,
            // which would mean one running timer for every visible item.
            visible: root.showActions && root.hasTotp
            Layout.alignment: Qt.AlignVCenter
            icon.name: "clock"
            display: QQC2.AbstractButton.IconOnly
            text: i18n("Copy verification code")
            onClicked: {
                const code = VaultManager.currentTotpCode(root.cipherId);
                if (code.length > 0) {
                    ClipboardHelper.copySecret(code);
                    applicationWindow().showMessage(i18n("Verification code copied"), Kirigami.MessageType.Positive);
                } else {
                    applicationWindow().showMessage(i18n("This item's verification code could not be generated."), Kirigami.MessageType.Error);
                }
            }

            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: text
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }

        QQC2.ToolButton {
            visible: root.showActions && root.type === 1
            Layout.alignment: Qt.AlignVCenter
            icon.name: "username-copy"
            display: QQC2.AbstractButton.IconOnly
            text: i18n("Copy username")
            onClicked: {
                const details = VaultManager.cipherDetails(root.cipherId);
                if (details.username) {
                    ClipboardHelper.copyPlain(details.username);
                    applicationWindow().showMessage(i18n("Username copied"), Kirigami.MessageType.Positive);
                }
            }

            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: text
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }

        QQC2.ToolButton {
            visible: root.showActions && root.type === 1
            Layout.alignment: Qt.AlignVCenter
            icon.name: "password-copy"
            display: QQC2.AbstractButton.IconOnly
            text: i18n("Copy password")
            onClicked: {
                const details = VaultManager.cipherDetails(root.cipherId);
                if (details.password) {
                    ClipboardHelper.copySecret(details.password);
                    applicationWindow().showMessage(i18n("Password copied"), Kirigami.MessageType.Positive);
                }
            }

            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: text
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
    }
}
