import QtQuick
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import io.github.timpalpant.kvault

/**
 * An item's avatar: a coloured disc with its initial, plus a small badge
 * marking the item type.
 *
 * Deliberately generated locally. Fetching real favicons would tell a third
 * party which sites are in the vault.
 */
Item {
    id: root

    required property string name
    required property int type

    implicitWidth: Kirigami.Units.iconSizes.medium
    implicitHeight: Kirigami.Units.iconSizes.medium

    Rectangle {
        id: disc
        anchors.fill: parent
        radius: height / 2
        color: IconHelper.avatarColor(root.name)

        QQC2.Label {
            anchors.centerIn: parent
            text: IconHelper.avatarLetter(root.name)
            color: "white"
            font.weight: Font.DemiBold
            font.pointSize: Kirigami.Theme.defaultFont.pointSize
        }
    }

    Kirigami.Icon {
        source: IconHelper.iconForType(root.type)
        width: Kirigami.Units.iconSizes.small * 0.8
        height: width

        anchors.right: disc.right
        anchors.bottom: disc.bottom
        anchors.rightMargin: -width * 0.15
        anchors.bottomMargin: -height * 0.15

        // A contrasting plate keeps the badge legible over any avatar colour.
        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 3
            height: parent.height + 3
            radius: height / 2
            color: Kirigami.Theme.backgroundColor
            z: -1
        }
    }
}
