import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import io.github.timpalpant.kvault

Kirigami.ScrollablePage {
    id: root

    // Resizable like the sidebar; the detail column takes what is left.
    Kirigami.ColumnView.fillWidth: false
    Kirigami.ColumnView.interactiveResizeEnabled: true
    Kirigami.ColumnView.minimumWidth: Kirigami.Units.gridUnit * 12
    Kirigami.ColumnView.maximumWidth: Kirigami.Units.gridUnit * 40

    readonly property var filter: VaultManager.filteredCiphers

    // In compact mode the sidebar lives in the global drawer, rather than as
    // a page before this list. Back from the list therefore opens that drawer
    // instead of exposing the desktop-only sidebar column.
    onBackRequested: event => {
        if (applicationWindow().compactSidebar) {
            event.accepted = true;
            applicationWindow().openCompactSidebar();
        }
    }

    // Back only changes PageRow.currentIndex; it deliberately leaves the
    // inactive list page in the stack. Always make this page current before
    // pushing one of its children, otherwise PageRow inserts after the sidebar
    // and replaces the list itself.
    function pushChildPage(source, properties) {
        const window = applicationWindow();
        const stack = window.pageStack;
        const listIndex = Kirigami.ColumnView.index;
        stack.currentIndex = listIndex;
        stack.pop(root);
        const childPage = stack.push(source, properties);
        window.pendingChildPage = childPage;

        // The first child is initially laid out while PageRow is still
        // anchored to the sidebar. Re-selecting it after layout positions the
        // viewport at this list/detail pair, so its contents are centered in
        // the visible detail pane.
        Qt.callLater(() => {
            if (window.pendingChildPage !== childPage) {
                return;
            }
            if (stack.currentItem === childPage) {
                stack.currentIndex = listIndex;
                stack.currentIndex = childPage.Kirigami.ColumnView.index;
            }
            // Main clears pendingChildPage when the sidebar has actually
            // finished scrolling away, so the first detail layout stays
            // centered throughout the transition.
        });
    }

    title: {
        switch (filter.scope) {
        case CipherFilterProxyModel.Favorites:
            return i18n("Favorites");
        case CipherFilterProxyModel.Trash:
            return i18n("Trash");
        case CipherFilterProxyModel.Folder:
            return VaultManager.folders.folderName(filter.folderId);
        case CipherFilterProxyModel.NoFolder:
            return i18n("No folder");
        case CipherFilterProxyModel.Type:
            return IconHelper.labelForType(filter.cipherType);
        default:
            return i18n("All items");
        }
    }

    function openCipher(cipherId) {
        // Replace anything beyond this column so detail pages do not stack up.
        root.pushChildPage(Qt.resolvedUrl("CipherDetailPage.qml"), {
            cipherId: cipherId
        });
    }

    function createItem(type) {
        VaultManager.beginCreate(type, filter.scope === CipherFilterProxyModel.Folder ? filter.folderId : "");
        root.pushChildPage(Qt.resolvedUrl("CipherEditPage.qml"));
    }

    titleDelegate: Kirigami.SearchField {
        Layout.fillWidth: true
        Layout.maximumWidth: Kirigami.Units.gridUnit * 30
        placeholderText: i18n("Search vault…")
        text: root.filter.searchText
        onTextChanged: root.filter.searchText = text
        // Ctrl+F and plain typing both land here.
        focusSequence: "Ctrl+F"
    }

    actions: [
        Kirigami.Action {
            text: i18n("New item")
            icon.name: "list-add"
            visible: root.filter.scope !== CipherFilterProxyModel.Trash

            Kirigami.Action {
                text: i18n("Login")
                icon.name: IconHelper.iconForType(1)
                onTriggered: root.createItem(1)
            }
            Kirigami.Action {
                text: i18n("Secure note")
                icon.name: IconHelper.iconForType(2)
                onTriggered: root.createItem(2)
            }
            Kirigami.Action {
                text: i18n("Card")
                icon.name: IconHelper.iconForType(3)
                onTriggered: root.createItem(3)
            }
            Kirigami.Action {
                text: i18n("Identity")
                icon.name: IconHelper.iconForType(4)
                onTriggered: root.createItem(4)
            }
            Kirigami.Action {
                text: i18n("SSH key")
                icon.name: IconHelper.iconForType(5)
                onTriggered: root.createItem(5)
            }
        },
        Kirigami.Action {
            text: VaultManager.syncing ? i18n("Syncing…") : i18n("Sync")
            icon.name: "view-refresh"
            enabled: !VaultManager.syncing
            shortcut: "Ctrl+R"
            onTriggered: VaultManager.sync()
        },
        Kirigami.Action {
            text: i18n("Generator")
            icon.name: "roll"
            shortcut: "Ctrl+G"
            onTriggered: root.pushChildPage(Qt.resolvedUrl("GeneratorPage.qml"))
        },
        Kirigami.Action {
            text: i18n("Settings")
            icon.name: "settings-configure"
            onTriggered: root.pushChildPage(Qt.resolvedUrl("SettingsPage.qml"))
        },
        Kirigami.Action {
            text: i18n("Lock")
            icon.name: "lock"
            onTriggered: VaultManager.lock()
        }
    ]

    ListView {
        id: listView

        model: root.filter
        currentIndex: -1
        clip: true
        reuseItems: true

        delegate: CipherDelegate {
            width: ListView.view.width
            onActivated: root.openCipher(cipherId)
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: listView.count === 0

            icon.name: root.filter.searchText.length > 0 ? "system-search" : "folder-open"
            text: {
                if (root.filter.searchText.length > 0) {
                    return i18n("No items match “%1”", root.filter.searchText);
                }
                if (root.filter.scope === CipherFilterProxyModel.Trash) {
                    return i18n("The trash is empty");
                }
                return i18n("Nothing here yet");
            }

            helpfulAction: Kirigami.Action {
                visible: root.filter.searchText.length === 0 && root.filter.scope !== CipherFilterProxyModel.Trash
                text: i18n("Add a login")
                icon.name: "list-add"
                onTriggered: root.createItem(1)
            }
        }
    }

    footer: QQC2.ToolBar {
        visible: VaultManager.offline || ClipboardHelper.secondsUntilClear > 0

        contentItem: RowLayout {
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                visible: VaultManager.offline
                source: "network-disconnect"
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: Kirigami.Units.iconSizes.small
            }

            QQC2.Label {
                visible: VaultManager.offline
                text: VaultManager.lastSync.getTime && !isNaN(VaultManager.lastSync.getTime()) ? i18n("Offline — showing the copy from %1", VaultManager.lastSync.toLocaleString(Qt.locale(), Locale.ShortFormat)) : i18n("Offline")
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Item {
                Layout.fillWidth: !VaultManager.offline
            }

            QQC2.Label {
                visible: ClipboardHelper.secondsUntilClear > 0
                text: i18np("Clipboard clears in %1 second", "Clipboard clears in %1 seconds", ClipboardHelper.secondsUntilClear)
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
            }

            QQC2.ToolButton {
                visible: ClipboardHelper.secondsUntilClear > 0
                text: i18n("Clear now")
                display: QQC2.AbstractButton.IconOnly
                icon.name: "edit-clear"
                onClicked: ClipboardHelper.clearNow()

                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: text
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }
    }
}
