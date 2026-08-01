import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import io.github.timpalpant.kvault


FormCard.FormCardPage {
    id: root

    /// Entries as returned by VaultManager.cipherDetails().passwordHistory.
    required property var entries

    title: i18n("Password history")

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        Repeater {
            model: root.entries

            FormCard.AbstractFormDelegate {
                required property var modelData

                Layout.fillWidth: true
                background: null

                contentItem: ColumnLayout {
                    spacing: 0

                    SecretField {
                        Layout.fillWidth: true
                        label: {
                            const date = modelData.lastUsedDate;
                            if (date && date.getTime && !isNaN(date.getTime())) {
                                return i18n("Replaced %1", date.toLocaleString(Qt.locale(), Locale.ShortFormat));
                            }
                            return i18n("Previous password");
                        }
                        value: modelData.password
                    }
                }
            }
        }
    }

    Kirigami.PlaceholderMessage {
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.gridUnit * 4
        visible: root.entries.length === 0
        icon.name: "view-history"
        text: i18n("No previous passwords are recorded for this item")
    }

    Item {
        Layout.preferredHeight: Kirigami.Units.gridUnit
    }
}
