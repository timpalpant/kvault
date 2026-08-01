import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import io.github.timpalpant.kvault

/// A four-segment strength meter with a label.
ColumnLayout {
    id: root

    /// 0 (weakest) to 4 (strongest).
    required property int strength

    property bool showLabel: true

    readonly property var _labels: [i18n("Very weak"), i18n("Weak"), i18n("Fair"), i18n("Strong"), i18n("Very strong")]

    readonly property color _color: {
        switch (root.strength) {
        case 0:
        case 1:
            return Kirigami.Theme.negativeTextColor;
        case 2:
            return Kirigami.Theme.neutralTextColor;
        default:
            return Kirigami.Theme.positiveTextColor;
        }
    }

    spacing: Kirigami.Units.smallSpacing

    RowLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        Repeater {
            model: 4

            Rectangle {
                required property int index

                Layout.fillWidth: true
                implicitHeight: Kirigami.Units.smallSpacing
                radius: height / 2
                color: index < root.strength ? root._color : Kirigami.Theme.alternateBackgroundColor

                Behavior on color {
                    ColorAnimation {
                        duration: Kirigami.Units.shortDuration
                    }
                }
            }
        }
    }

    QQC2.Label {
        visible: root.showLabel
        text: root._labels[Math.max(0, Math.min(4, root.strength))]
        font: Kirigami.Theme.smallFont
        color: root._color
    }
}
