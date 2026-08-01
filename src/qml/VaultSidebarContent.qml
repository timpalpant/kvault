import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.delegates as Delegates
import io.github.timpalpant.kvault

Item {
    id: root

    property var filter: VaultManager.filteredCiphers

    // The desktop sidebar and compact drawer decide how to reveal the list
    // after a selection. Keeping the navigation itself shared ensures their
    // folders, selection state, and context menus cannot drift apart.
    signal scopeSelected()

    implicitWidth: navigation.implicitWidth
    implicitHeight: navigation.implicitHeight
    height: implicitHeight
    Layout.fillWidth: true
    Layout.preferredHeight: implicitHeight

    function selectScope(scope, extra) {
        filter.scope = scope;
        if (scope === CipherFilterProxyModel.Folder) {
            filter.folderId = extra;
        } else if (scope === CipherFilterProxyModel.Type) {
            filter.cipherType = extra;
        }
        root.scopeSelected();
    }

    function isCurrent(scope, extra) {
        if (filter.scope !== scope) {
            return false;
        }
        if (scope === CipherFilterProxyModel.Folder) {
            return filter.folderId === extra;
        }
        if (scope === CipherFilterProxyModel.Type) {
            return filter.cipherType === extra;
        }
        return true;
    }

    ColumnLayout {
        id: navigation

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        spacing: 0

        // --- standard views ---
        Repeater {
            model: [
                {
                    label: i18n("All items"),
                    icon: "folder-open",
                    scope: CipherFilterProxyModel.AllItems,
                    extra: 0
                },
                {
                    label: i18n("Favorites"),
                    icon: "favorite",
                    scope: CipherFilterProxyModel.Favorites,
                    extra: 0
                }
            ]

            Delegates.RoundedItemDelegate {
                required property var modelData

                Layout.fillWidth: true
                text: modelData.label
                icon.name: modelData.icon
                highlighted: root.isCurrent(modelData.scope, modelData.extra)
                onClicked: root.selectScope(modelData.scope, modelData.extra)
            }
        }

        Kirigami.ListSectionHeader {
            Layout.fillWidth: true
            text: i18n("Types")
        }

        Repeater {
            // Cipher type ids as used by the server.
            model: [1, 2, 3, 4, 5]

            Delegates.RoundedItemDelegate {
                required property int modelData

                Layout.fillWidth: true
                text: IconHelper.labelForType(modelData)
                icon.name: IconHelper.iconForType(modelData)
                highlighted: root.isCurrent(CipherFilterProxyModel.Type, modelData)
                onClicked: root.selectScope(CipherFilterProxyModel.Type, modelData)
            }
        }

        Kirigami.ListSectionHeader {
            Layout.fillWidth: true
            text: i18n("Folders")

            QQC2.ToolButton {
                icon.name: "folder-new"
                text: i18n("New folder")
                display: QQC2.AbstractButton.IconOnly
                onClicked: {
                    folderNameField.text = "";
                    newFolderDialog.open();
                }

                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: text
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }

        Repeater {
            model: VaultManager.folders

            Delegates.RoundedItemDelegate {
                id: folderDelegate

                required property string folderId
                required property string name
                required property int itemCount

                Layout.fillWidth: true
                icon.name: "folder"
                highlighted: root.isCurrent(CipherFilterProxyModel.Folder, folderId)
                onClicked: root.selectScope(CipherFilterProxyModel.Folder, folderId)

                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: "folder"
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: Kirigami.Units.iconSizes.small
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: folderDelegate.name
                        elide: Text.ElideRight
                    }

                    QQC2.Label {
                        text: folderDelegate.itemCount
                        color: Kirigami.Theme.disabledTextColor
                        font: Kirigami.Theme.smallFont
                    }
                }

                // Right-click to rename or delete, as in other KDE list views.
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: folderMenu.popup()
                }

                QQC2.Menu {
                    id: folderMenu

                    QQC2.MenuItem {
                        text: i18n("Rename…")
                        icon.name: "edit-rename"
                        onTriggered: {
                            renameDialog.folderId = folderDelegate.folderId;
                            renameField.text = folderDelegate.name;
                            renameDialog.open();
                        }
                    }

                    QQC2.MenuItem {
                        text: i18n("Delete…")
                        icon.name: "edit-delete"
                        onTriggered: {
                            deleteFolderDialog.folderId = folderDelegate.folderId;
                            deleteFolderDialog.folderName = folderDelegate.name;
                            deleteFolderDialog.open();
                        }
                    }
                }
            }
        }

        Delegates.RoundedItemDelegate {
            Layout.fillWidth: true
            text: i18n("No folder")
            icon.name: "folder-open"
            highlighted: root.isCurrent(CipherFilterProxyModel.NoFolder, 0)
            onClicked: root.selectScope(CipherFilterProxyModel.NoFolder, 0)
        }

        Kirigami.ListSectionHeader {
            Layout.fillWidth: true
            text: i18n("Other")
        }

        Delegates.RoundedItemDelegate {
            Layout.fillWidth: true
            text: i18n("Trash")
            icon.name: "user-trash"
            highlighted: root.isCurrent(CipherFilterProxyModel.Trash, 0)
            onClicked: root.selectScope(CipherFilterProxyModel.Trash, 0)
        }
    }

    Kirigami.PromptDialog {
        id: newFolderDialog

        title: i18n("New folder")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel

        onAccepted: VaultManager.createFolder(folderNameField.text)
        onOpened: folderNameField.forceActiveFocus()

        QQC2.TextField {
            id: folderNameField
            placeholderText: i18n("Folder name")
            onAccepted: newFolderDialog.accept()
        }
    }

    Kirigami.PromptDialog {
        id: renameDialog

        property string folderId: ""

        title: i18n("Rename folder")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel

        onAccepted: VaultManager.renameFolder(folderId, renameField.text)
        onOpened: renameField.forceActiveFocus()

        QQC2.TextField {
            id: renameField
            onAccepted: renameDialog.accept()
        }
    }

    Kirigami.PromptDialog {
        id: deleteFolderDialog

        property string folderId: ""
        property string folderName: ""

        title: i18n("Delete folder?")
        subtitle: i18n("“%1” will be removed. The items inside it are kept and become unfiled.", folderName)
        standardButtons: Kirigami.Dialog.NoButton

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Delete")
                icon.name: "edit-delete"
                onTriggered: {
                    deleteFolderDialog.close();
                    VaultManager.deleteFolder(deleteFolderDialog.folderId);
                }
            },
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: deleteFolderDialog.close()
            }
        ]
    }
}
