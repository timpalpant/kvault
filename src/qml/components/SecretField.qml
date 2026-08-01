import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import io.github.timpalpant.kvault

/**
 * A labelled value that starts concealed, with reveal and copy actions.
 * Used for passwords, card codes, SSH keys and hidden custom fields.
 */
RowLayout {
    id: root

    required property string label
    required property string value

    property bool monospace: true
    property bool multiline: false
    /// Copies of secrets go through the auto-clearing clipboard path.
    property bool secret: true

    property bool revealed: !AppSettings.concealPasswords

    visible: value.length > 0
    spacing: Kirigami.Units.smallSpacing

    ColumnLayout {
        Layout.fillWidth: true
        // Lets the row shrink below the natural width of its text
        // instead of overflowing the pane.
        Layout.preferredWidth: 1
        spacing: 0

        QQC2.Label {
            text: root.label
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        QQC2.Label {
            Layout.fillWidth: true
            // A fixed run of bullets avoids leaking the secret's length.
            text: root.revealed ? root.value : "••••••••••••"
            font.family: root.monospace && root.revealed ? "monospace" : Kirigami.Theme.defaultFont.family
            wrapMode: root.multiline && root.revealed ? Text.WrapAnywhere : Text.NoWrap
            elide: root.multiline && root.revealed ? Text.ElideNone : Text.ElideRight
            maximumLineCount: root.multiline && root.revealed ? 16 : 1
            textFormat: Text.PlainText
        }
    }

    QQC2.ToolButton {
        icon.name: root.revealed ? "password-show-off" : "password-show-on"
        display: QQC2.AbstractButton.IconOnly
        text: root.revealed ? i18n("Hide") : i18n("Reveal")
        onClicked: root.revealed = !root.revealed

        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.text: text
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
    }

    QQC2.ToolButton {
        icon.name: "edit-copy"
        display: QQC2.AbstractButton.IconOnly
        text: i18n("Copy")
        onClicked: {
            if (root.secret) {
                ClipboardHelper.copySecret(root.value);
            } else {
                ClipboardHelper.copyPlain(root.value);
            }
            applicationWindow().showMessage(i18n("%1 copied", root.label), Kirigami.MessageType.Positive);
        }

        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.text: text
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
    }
}
