import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: root

    title: i18n("Vault")

    // A fixed-width column with a draggable separator, as in Dolphin. The
    // starting width comes from pageStack.defaultColumnWidth (preferredWidth is
    // overridden by columnWidth, so setting it here would be misleading), and
    // the width the user drags to is kept in the ColumnView's savedState.
    Kirigami.ColumnView.fillWidth: false
    Kirigami.ColumnView.preventStealing: true
    Kirigami.ColumnView.interactiveResizeEnabled: true
    Kirigami.ColumnView.minimumWidth: Kirigami.Units.gridUnit * 9
    Kirigami.ColumnView.maximumWidth: Kirigami.Units.gridUnit * 28

    // The rounded delegates already carry their own insets, so page padding on
    // top of them reads as too much air for a sidebar.
    topPadding: 0
    bottomPadding: 0
    leftPadding: 0
    rightPadding: 0

    VaultSidebarContent {
        onScopeSelected: applicationWindow().pageStack.currentIndex = 1
    }

    footer: VaultSidebarFooter {}
}
