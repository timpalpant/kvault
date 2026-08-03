function(kvault_add_test name)
    qt_add_executable(${name} ${name}.cpp ${ARGN})
    target_link_libraries(${name} PRIVATE kvault_core Qt6::Test)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

kvault_add_test(cryptotest)
kvault_add_test(totptest)

if(KVAULT_BUILD_APP)
    # These exercise QML-facing types, so they only build with the app.
    qt_add_executable(generatortest generatortest.cpp ${CMAKE_SOURCE_DIR}/src/app/passwordgenerator.cpp)
    target_link_libraries(generatortest PRIVATE kvault_core Qt6::Test Qt6::Qml)
    target_include_directories(generatortest PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME generatortest COMMAND generatortest)
    set_tests_properties(generatortest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # Checks what the client actually puts on the wire, against a local server.
    qt_add_executable(apitest apitest.cpp ${CMAKE_SOURCE_DIR}/src/api/apiclient.cpp)
    target_link_libraries(apitest PRIVATE kvault_core Qt6::Test Qt6::Network Qt6::Gui)
    target_include_directories(apitest PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME apitest COMMAND apitest)
    set_tests_properties(apitest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # End-to-end cover of the unlock path, against a synthetic on-disk vault.
    qt_add_executable(vaulttest
        vaulttest.cpp
        ${CMAKE_SOURCE_DIR}/src/api/apiclient.cpp
        ${CMAKE_SOURCE_DIR}/src/app/appsettings.cpp
        ${CMAKE_SOURCE_DIR}/src/vault/attachmentmanager.cpp
        ${CMAKE_SOURCE_DIR}/src/vault/ciphereditor.cpp
        ${CMAKE_SOURCE_DIR}/src/vault/cipherfilterproxymodel.cpp
        ${CMAKE_SOURCE_DIR}/src/vault/cipherlistmodel.cpp
        ${CMAKE_SOURCE_DIR}/src/vault/foldermodel.cpp
        ${CMAKE_SOURCE_DIR}/src/vault/localstore.cpp
        ${CMAKE_SOURCE_DIR}/src/vault/vaultkeys.cpp
        ${CMAKE_SOURCE_DIR}/src/vault/vaultmanager.cpp
    )
    target_link_libraries(vaulttest PRIVATE
        kvault_core
        Qt6::Test
        Qt6::Qml
        Qt6::Network
        Qt6::Concurrent
        Qt6Keychain::Qt6Keychain
        KF6::I18n
    )
    target_include_directories(vaulttest PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME vaulttest COMMAND vaulttest)
    # QStandardPaths test mode writes below $HOME/.qttest. Keep that state in
    # the build tree so the test is isolated from a developer's home and works
    # in sandboxes that make the real home directory read-only.
    set(KVAULT_TEST_HOME "${CMAKE_BINARY_DIR}/test-home")
    file(MAKE_DIRECTORY "${KVAULT_TEST_HOME}")
    set_tests_properties(vaulttest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HOME=${KVAULT_TEST_HOME}")
endif()
