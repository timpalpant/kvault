import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import io.github.timpalpant.kvault

QQC2.ToolBar {
    contentItem: RowLayout {
        QQC2.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            text: VaultManager.email
            elide: Text.ElideMiddle
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }

        QQC2.ToolButton {
            icon.name: "lock"
            text: i18n("Lock vault")
            display: QQC2.AbstractButton.IconOnly
            onClicked: VaultManager.lock()

            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: i18n("Lock vault (Ctrl+L)")
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
    }
}
