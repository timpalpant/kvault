cmake_minimum_required(VERSION 3.24)

project(kvault VERSION 0.1.3 DESCRIPTION "Native Kirigami client for Bitwarden" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CTest)

# Turning this off builds only the vault/crypto core and its tests, which needs
# no GUI stack. Handy for CI and for iterating on the protocol code.
option(KVAULT_BUILD_APP "Build the Kirigami application" ON)

set(KVAULT_QT_COMPONENTS Core Test)
if(KVAULT_BUILD_APP)
    list(APPEND KVAULT_QT_COMPONENTS Gui Qml Quick QuickControls2 Network Concurrent DBus Svg Widgets)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS ${KVAULT_QT_COMPONENTS})
if(KVAULT_BUILD_APP)
    # Found individually rather than through the KF6 umbrella package, which is
    # only present when extra-cmake-modules is installed.
    find_package(KF6Kirigami 6.0 REQUIRED)
    find_package(KF6I18n 6.8 REQUIRED)
    find_package(KF6CoreAddons 6.0 REQUIRED)
    find_package(KF6DBusAddons 6.0 REQUIRED)
    find_package(KF6StatusNotifierItem 6.0 REQUIRED)
    find_package(Qt6Keychain REQUIRED)
endif()
find_package(OpenSSL 3.0 REQUIRED)

find_library(ARGON2_LIBRARY NAMES argon2 REQUIRED)
find_path(ARGON2_INCLUDE_DIR NAMES argon2.h REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# Core library: everything that does not touch QML, so tests can link it.
# ---------------------------------------------------------------------------
set(KVAULT_CORE_SOURCES
    src/crypto/securebytes.cpp
    src/crypto/crypto.cpp
    src/crypto/symmetrickey.cpp
    src/crypto/encstring.cpp
    src/crypto/kdf.cpp
    src/crypto/totp.cpp
    src/crypto/wordlist.cpp
    src/model/cipher.cpp
    src/model/folder.cpp
)

add_library(kvault_core STATIC ${KVAULT_CORE_SOURCES})

target_include_directories(kvault_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${ARGON2_INCLUDE_DIR}
)
target_link_libraries(kvault_core
    PUBLIC Qt6::Core
    PRIVATE OpenSSL::Crypto ${ARGON2_LIBRARY}
)
set_target_properties(kvault_core PROPERTIES
    AUTOMOC ON
    POSITION_INDEPENDENT_CODE ON
)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
if(KVAULT_BUILD_APP)
qt_add_executable(kvault
    src/main.cpp
    src/app/traycontroller.cpp
    src/app/traycontroller.h
)

qt_add_qml_module(kvault
    URI io.github.timpalpant.kvault
    VERSION 1.0
    SOURCES
        src/api/apiclient.cpp
        src/api/apiclient.h
        src/vault/vaultmanager.cpp
        src/vault/vaultmanager.h
        src/vault/vaultkeys.cpp
        src/vault/vaultkeys.h
        src/vault/localstore.cpp
        src/vault/localstore.h
        src/vault/cipherlistmodel.cpp
        src/vault/cipherlistmodel.h
        src/vault/cipherfilterproxymodel.cpp
        src/vault/cipherfilterproxymodel.h
        src/vault/foldermodel.cpp
        src/vault/foldermodel.h
        src/vault/ciphereditor.cpp
        src/vault/ciphereditor.h
        src/vault/attachmentmanager.cpp
        src/vault/attachmentmanager.h
        src/app/appsettings.cpp
        src/app/appsettings.h
        src/app/clipboardhelper.cpp
        src/app/clipboardhelper.h
        src/app/passwordgenerator.cpp
        src/app/passwordgenerator.h
        src/app/totpcontroller.cpp
        src/app/totpcontroller.h
        src/app/iconhelper.cpp
        src/app/iconhelper.h
    QML_FILES
        src/qml/Main.qml
        src/qml/LoginPage.qml
        src/qml/TwoFactorPage.qml
        src/qml/NewDevicePage.qml
        src/qml/UnlockPage.qml
        src/qml/VaultPage.qml
        src/qml/VaultSidebar.qml
        src/qml/VaultSidebarContent.qml
        src/qml/VaultSidebarFooter.qml
        src/qml/CipherDelegate.qml
        src/qml/CipherDetailPage.qml
        src/qml/CipherEditPage.qml
        src/qml/GeneratorPage.qml
        src/qml/SettingsPage.qml
        src/qml/PasswordHistoryPage.qml
        src/qml/components/SecretField.qml
        src/qml/components/CopyableRow.qml
        src/qml/components/TotpRing.qml
        src/qml/components/CipherTypeIcon.qml
        src/qml/components/PasswordStrengthBar.qml
    RESOURCES
        src/qml/icons/kvault.svg
)

# qmltyperegistrar emits `#include <foo.h>` using bare header names, so each
# directory holding a QML-exposed type has to be on the include path too.
target_include_directories(kvault PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vault
)

target_link_libraries(kvault PRIVATE
    kvault_core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Concurrent
    Qt6::DBus
    Qt6::Svg
    Qt6::Widgets
    KF6::Kirigami
    KF6::I18n
    KF6::I18nQml
    KF6::CoreAddons
    KF6::DBusAddons
    KF6::StatusNotifierItem
    Qt6Keychain::Qt6Keychain
    OpenSSL::Crypto
)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
if(KVAULT_BUILD_APP)
install(TARGETS kvault RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/io.github.timpalpant.kvault.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES data/io.github.timpalpant.kvault.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
install(FILES src/qml/icons/kvault.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
    RENAME io.github.timpalpant.kvault.svg)

# The CI and release workflows build native DEB and RPM packages from the
# normal install tree, so keep their metadata next to the install rules.
set(CPACK_PACKAGE_NAME "kvault")
set(CPACK_PACKAGE_VENDOR "Timothy Palpant")
set(CPACK_PACKAGE_CONTACT "Timothy Palpant <tim@palpant.us>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native Bitwarden client for KDE Plasma")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
# qml6-module-org-kde-desktop (the "org.kde.desktop" QQC2 style main.cpp
# always selects) is deliberately NOT listed here: on Debian it is already a
# hard transitive Depends of qml6-module-org-kde-kirigamiaddons-delegates (via
# -treeview), confirmed by installing this .deb in a bare debian:trixie
# container with `apt-get install --no-install-recommends` and stracing the
# launch -- org/kde/desktop/qmldir resolved without it ever being declared
# here. Do not remove qml6-module-org-kde-kirigamiaddons-delegates without
# re-checking this.
#
# qt6-svg-plugins is the QImageReader plugin LoginPage.qml's SVG logo
# (src/qml/icons/kvault.svg, shown on every launch) needs to decode -- it is
# only a Recommends of libqt6gui6, not a hard Depends, so `apt-get install
# --no-install-recommends` (the flag CI's own dependency steps use, and what
# minimal/server images set by default) leaves it out; confirmed by stracing
# a launch in that state and finding libqsvg.so absent from the imageformats
# plugin directory entirely.
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-dialogs, qml6-module-qtquick-layouts, qml6-module-qtquick-shapes, qml6-module-qtquick-window, qml6-module-org-kde-kirigami, qml6-module-org-kde-kirigamiaddons-delegates, qml6-module-org-kde-kirigamiaddons-formcard, qt6-svg-plugins")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Utility")
# kf6-qqc2-desktop-style is the Fedora name for the "org.kde.desktop" QQC2
# style main.cpp always selects -- unlike on Debian, nothing kvault depends on
# pulls it in transitively here, so without it QQuickStyle::setStyle() cannot
# find the style directory at all and the QML engine aborts before showing a
# window (exit -1, no console output); confirmed by installing this .rpm in a
# bare Fedora container and stracing the launch: statx() on
# qt6/qml/org/kde/desktop returned ENOENT and the process exited via
# QCoreApplication::exit(-1).
#
# qt6-qtsvg is NOT listed here: kvault links Qt6::Svg directly (unlike an app
# that only reaches SVG through the QML image plugin), so RPM's automatic
# Requires scanning already adds it for libQt6Svg.so.6, and that package
# bundles the imageformats SVG decoder plugin itself -- confirmed installed
# and resolving via strace without adding anything here.
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtdeclarative, kf6-kirigami, kf6-kirigami-addons, kf6-qqc2-desktop-style")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
include(CPack)
endif()
