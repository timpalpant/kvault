import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import io.github.timpalpant.kvault

/// A labelled, non-secret value with a copy button, and optionally a link.
RowLayout {
    id: root

    required property string label
    required property string value

    /// When set, the value is shown as a clickable link that opens externally.
    property bool isLink: false
    property bool monospace: false
    property bool multiline: false

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
            id: valueLabel

            Layout.fillWidth: true
            // Always plain text. Qt cannot elide rich text, so a long URL would
            // spill out of a narrow column, and interpolating a server-supplied
            // value into markup would let a crafted item inject HTML.
            text: root.value
            textFormat: Text.PlainText
            color: root.isLink ? Kirigami.Theme.linkColor : Kirigami.Theme.textColor
            font.family: root.monospace ? "monospace" : Kirigami.Theme.defaultFont.family
            wrapMode: root.multiline ? Text.Wrap : Text.NoWrap
            elide: root.multiline ? Text.ElideNone : Text.ElideRight
            maximumLineCount: root.multiline ? 20 : 1

            HoverHandler {
                id: linkHover
                enabled: root.isLink
                cursorShape: Qt.PointingHandCursor
            }

            TapHandler {
                enabled: root.isLink
                onTapped: Qt.openUrlExternally(root.value)
            }

            QQC2.ToolTip.visible: root.isLink && linkHover.hovered && valueLabel.truncated
            QQC2.ToolTip.text: root.value
        }
    }

    QQC2.ToolButton {
        visible: root.isLink
        icon.name: "internet-services"
        display: QQC2.AbstractButton.IconOnly
        text: i18n("Open in browser")
        onClicked: Qt.openUrlExternally(root.value)

        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.text: text
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
    }

    QQC2.ToolButton {
        icon.name: "edit-copy"
        display: QQC2.AbstractButton.IconOnly
        text: i18n("Copy")
        onClicked: {
            ClipboardHelper.copyPlain(root.value);
            applicationWindow().showMessage(i18n("%1 copied", root.label), Kirigami.MessageType.Positive);
        }

        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.text: text
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
    }
}
