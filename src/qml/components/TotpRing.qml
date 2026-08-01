import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Shapes

import org.kde.kirigami as Kirigami
import io.github.timpalpant.kvault

/// The current one-time code with a ring that drains as it expires.
RowLayout {
    id: root

    required property string seed

    visible: totp.valid
    spacing: Kirigami.Units.largeSpacing

    TotpController {
        id: totp
        seed: root.seed
    }

    ColumnLayout {
        Layout.fillWidth: true
        // Without this the code's own width drives the row and the whole card
        // refuses to shrink when the column gets narrower.
        Layout.preferredWidth: 1
        spacing: 0

        QQC2.Label {
            Layout.fillWidth: true
            text: i18n("Verification code")
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: totp.formattedCode
            font.family: "monospace"
            font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.4
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            textFormat: Text.PlainText
            // Turn red for the last few seconds so a code is not copied just as
            // it expires.
            color: totp.secondsRemaining <= 5 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor

            Behavior on color {
                ColorAnimation {
                    duration: Kirigami.Units.shortDuration
                }
            }
        }
    }

    Item {
        id: ring

        // Matches the adjacent tool button so the row reads as a single line.
        readonly property int diameter: Kirigami.Units.iconSizes.medium
        readonly property real stroke: 2.5
        readonly property real ringRadius: (diameter - stroke) / 2

        Layout.preferredWidth: diameter
        Layout.preferredHeight: diameter
        Layout.alignment: Qt.AlignVCenter
        implicitWidth: diameter
        implicitHeight: diameter

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            // Geometry is taken from `ring` by id. Reaching for it through
            // parent.parent resolved to the wrong element and blew the ring up
            // to the width of the whole row.
            ShapePath {
                strokeWidth: ring.stroke
                strokeColor: Kirigami.Theme.alternateBackgroundColor
                fillColor: "transparent"

                PathAngleArc {
                    centerX: ring.diameter / 2
                    centerY: ring.diameter / 2
                    radiusX: ring.ringRadius
                    radiusY: ring.ringRadius
                    startAngle: -90
                    sweepAngle: 360
                }
            }

            ShapePath {
                strokeWidth: ring.stroke
                strokeColor: totp.secondsRemaining <= 5 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.highlightColor
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap

                PathAngleArc {
                    centerX: ring.diameter / 2
                    centerY: ring.diameter / 2
                    radiusX: ring.ringRadius
                    radiusY: ring.ringRadius
                    startAngle: -90
                    sweepAngle: 360 * totp.progress
                }
            }
        }

        HoverHandler {
            id: ringHover
        }
        QQC2.ToolTip.visible: ringHover.hovered
        QQC2.ToolTip.text: i18np("Expires in %1 second", "Expires in %1 seconds", totp.secondsRemaining)
    }

    QQC2.ToolButton {
        icon.name: "edit-copy"
        display: QQC2.AbstractButton.IconOnly
        text: i18n("Copy code")
        Layout.alignment: Qt.AlignVCenter
        onClicked: {
            ClipboardHelper.copySecret(totp.code);
            applicationWindow().showMessage(i18n("Verification code copied"), Kirigami.MessageType.Positive);
        }

        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.text: text
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
    }
}
