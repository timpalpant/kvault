import QtQuick
import QtQuick.Window
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import io.github.timpalpant.kvault

Kirigami.ApplicationWindow {
    id: root

    title: i18n("KVault")

    minimumWidth: Kirigami.Units.gridUnit * 26
    minimumHeight: Kirigami.Units.gridUnit * 22
    width: Kirigami.Units.gridUnit * 62
    height: Kirigami.Units.gridUnit * 40

    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.ToolBar
    // Lets each page state its own width, which is what makes the sidebar
    // separator draggable instead of every column being the same size.
    pageStack.columnView.columnResizeMode: root.compactSidebar
        ? Kirigami.ColumnView.SingleColumn
        : Kirigami.ColumnView.DynamicColumns
    // columnWidth is both the width of a non-filling column and the minimum a
    // filling one will shrink to. The default (gridUnit * 20) stopped the
    // detail pane at 360px, after which it overflowed the window instead of
    // shrinking. This is also the sidebar's starting width.
    pageStack.defaultColumnWidth: Kirigami.Units.gridUnit * 13

    readonly property bool unlocked: VaultManager.state === VaultManager.Unlocked
    // Below this width, three panes no longer leave enough room for useful
    // content. The sidebar becomes the standard Kirigami navigation drawer.
    readonly property bool compactSidebar: width < Kirigami.Units.gridUnit * 48
    // Set only while a child page is being inserted. It keeps that page at its
    // full detail width during PageRow's deferred navigation update.
    property var pendingChildPage: null

    Timer {
        interval: 16
        repeat: true
        running: root.pendingChildPage !== null
        onTriggered: {
            const sidebar = root.pageStack.get(0);
            // Once the ColumnView has reached the child viewport, its scroll
            // position is the single source of truth for the visible sidebar
            // width. Abandon a pending page if the user navigates away first.
            if (root.compactSidebar || !sidebar || root.pageStack.currentItem !== root.pendingChildPage
                    || root.pageStack.columnView.contentX >= sidebar.width - 0.5) {
                root.pendingChildPage = null;
            }
        }
    }

    function childPageWidth(page) {
        const stack = pageStack;
        if (compactSidebar) {
            return stack.width;
        }
        const sidebar = stack.get(0);
        const list = stack.get(1);
        if (!sidebar || !list) {
            return stack.width;
        }

        // contentX tracks exactly how much of the sidebar PageRow has moved
        // out of view. Unlike currentIndex, it remains correct while a form
        // control changes focus or causes a relayout.
        const hiddenSidebarWidth = pendingChildPage === page
            ? sidebar.width
            : Math.max(0, Math.min(sidebar.width, stack.columnView.contentX));
        return Math.max(0, stack.width - list.width - sidebar.width + hiddenSidebarWidth);
    }

    // Remember how the user sized the columns between runs.
    Connections {
        target: root.pageStack.columnView

        function onSavedStateChanged() {
            AppSettings.columnState = root.pageStack.columnView.savedState;
        }
    }

    function showMessage(text, type) {
        inlineBanner.text = text;
        inlineBanner.type = type ?? Kirigami.MessageType.Information;
        inlineBanner.visible = true;
        bannerTimer.restart();
    }

    function openCompactSidebar() {
        if (compactSidebar && unlocked) {
            compactSidebarDrawer.open();
        }
    }

    onCompactSidebarChanged: {
        // A desktop Back action can leave the sidebar as the current page.
        // When becoming compact, return to the list—the drawer remains the
        // navigation surface and is available from the hamburger button.
        if (compactSidebar && unlocked && pageStack.depth > 1 && pageStack.currentIndex === 0) {
            pageStack.currentIndex = 1;
        }
    }

    // Routing follows the vault state rather than being pushed ad hoc, so
    // locking from anywhere always lands somewhere sensible.
    function rebuildStack() {
        root.pendingChildPage = null;
        root.pageStack.clear();
        switch (VaultManager.state) {
        case VaultManager.LoggedOut:
            root.pageStack.push(Qt.resolvedUrl("LoginPage.qml"));
            break;
        case VaultManager.Locked:
            root.pageStack.push(Qt.resolvedUrl("UnlockPage.qml"));
            break;
        case VaultManager.Unlocked:
            root.pageStack.push(Qt.resolvedUrl("VaultSidebar.qml"));
            root.pageStack.push(Qt.resolvedUrl("VaultPage.qml"));
            break;
        }
    }

    Component.onCompleted: {
        if (AppSettings.columnState.length > 0) {
            pageStack.columnView.savedState = AppSettings.columnState;
        }
        rebuildStack();
    }

    Connections {
        target: VaultManager

        function onStateChanged() {
            root.rebuildStack();
        }

        function onTwoFactorRequired(providers) {
            root.pageStack.clear();
            root.pageStack.push(Qt.resolvedUrl("TwoFactorPage.qml"), {
                providers: providers
            });
        }

        function onNewDeviceVerificationRequired(message) {
            root.pageStack.clear();
            root.pageStack.push(Qt.resolvedUrl("NewDevicePage.qml"), {
                message: message
            });
        }

        function onErrorOccurred(message) {
            root.showMessage(message, Kirigami.MessageType.Error);
        }

        function onOperationSucceeded(message) {
            root.showMessage(message, Kirigami.MessageType.Positive);
        }

        function onSyncFinished(success, message) {
            // A successful sync is routine; only speak up when it is not.
            if (!success) {
                root.showMessage(message, Kirigami.MessageType.Warning);
            }
        }
    }

    // A banner rather than a passive notification, so errors do not disappear
    // before they have been read.
    header: Kirigami.InlineMessage {
        id: inlineBanner

        position: Kirigami.InlineMessage.Position.Header
        showCloseButton: true
        visible: false

        Timer {
            id: bannerTimer
            interval: 8000
            onTriggered: inlineBanner.visible = false
        }
    }

    globalDrawer: Kirigami.GlobalDrawer {
        id: compactSidebarDrawer

        title: i18n("Vault")
        modal: true
        enabled: root.unlocked && root.compactSidebar
        handleVisible: root.unlocked && root.compactSidebar
        width: Math.min(Kirigami.Units.gridUnit * 24, root.width - Kirigami.Units.gridUnit * 2)

        // This is the same component used by the desktop sidebar, including
        // dynamic folders and their management actions.
        VaultSidebarContent {
            onScopeSelected: {
                root.pageStack.currentIndex = 1;
                compactSidebarDrawer.close();
            }
        }

        footer: VaultSidebarFooter {}

        onEnabledChanged: {
            if (!enabled) {
                close();
            }
        }
    }

    // Idle detection. A coarse heartbeat is enough: the point is to notice
    // that the user is still here, not to track every event precisely.
    Timer {
        running: root.unlocked
        interval: 30000
        repeat: true
        onTriggered: {
            if (root.active) {
                VaultManager.noteActivity();
            }
        }
    }

    onActiveChanged: {
        if (active) {
            VaultManager.noteActivity();
        }
    }

    onClosing: close => {
        if (AppSettings.closeToTray && TrayController.hideMainWindow()) {
            close.accepted = false;
        }
    }

    onVisibilityChanged: newVisibility => {
        if (newVisibility === Window.Minimized && AppSettings.lockOnMinimize && root.unlocked) {
            VaultManager.lock();
        }
    }

    // Ctrl+L locks from anywhere, which is the shortcut muscle memory expects.
    QQC2.Action {
        id: lockAction
        shortcut: "Ctrl+L"
        enabled: root.unlocked
        onTriggered: VaultManager.lock()
    }
}
