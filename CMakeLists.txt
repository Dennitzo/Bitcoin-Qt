cmake_minimum_required(VERSION 3.21)

project(BitcoinQt
    VERSION 0.1.0
    DESCRIPTION "Native Bitcoin full node desktop application"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Concurrent
    Network
    WebChannel
    WebEngineWidgets
    Widgets
)

qt_standard_project_setup()

qt_add_executable(BitcoinNodeDesktop
    resources/resources.qrc
    src/main.cpp
    src/core/AppTypes.h
    src/core/ConfigManager.cpp
    src/core/ConfigManager.h
    src/core/LogManager.cpp
    src/core/LogManager.h
    src/core/ManagedService.cpp
    src/core/ManagedService.h
    src/core/RpcClient.cpp
    src/core/RpcClient.h
    src/core/RuntimePaths.cpp
    src/core/RuntimePaths.h
    src/core/ServiceManager.cpp
    src/core/ServiceManager.h
    src/services/BitcoinCoreService.cpp
    src/services/BitcoinCoreService.h
    src/services/ElectrsService.cpp
    src/services/ElectrsService.h
    src/services/MempoolDatabaseService.cpp
    src/services/MempoolDatabaseService.h
    src/services/MempoolService.cpp
    src/services/MempoolService.h
    src/services/PublicPoolService.cpp
    src/services/PublicPoolService.h
    src/ui/DashboardPage.cpp
    src/ui/DashboardPage.h
    src/ui/FirstRunDialog.cpp
    src/ui/FirstRunDialog.h
    src/ui/LogsPage.cpp
    src/ui/LogsPage.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/NodePage.cpp
    src/ui/NodePage.h
    src/ui/SettingsPage.cpp
    src/ui/SettingsPage.h
    src/web/LocalUrlInterceptor.cpp
    src/web/LocalUrlInterceptor.h
    src/web/WebBridge.cpp
    src/web/WebBridge.h
)

if(WIN32)
    target_sources(BitcoinNodeDesktop PRIVATE resources/windows.rc)
endif()

target_link_libraries(BitcoinNodeDesktop PRIVATE
    Qt6::Concurrent
    Qt6::Network
    Qt6::WebChannel
    Qt6::WebEngineWidgets
    Qt6::Widgets
)

target_compile_definitions(BitcoinNodeDesktop PRIVATE
    QT_NO_KEYWORDS
    APP_VERSION="${PROJECT_VERSION}"
)

set_target_properties(BitcoinNodeDesktop PROPERTIES
    OUTPUT_NAME "Bitcoin-Qt"
)

set_target_properties(BitcoinNodeDesktop PROPERTIES
    QT_QML_MODULE_NO_IMPORT_SCAN TRUE
)

set(BITCOIN_QT_RUNTIME_SOURCE_DIR "${CMAKE_SOURCE_DIR}/runtime" CACHE PATH "Runtime directory copied beside the app")
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/windows/runtime")
    set(BITCOIN_QT_RUNTIME_SOURCE_DIR "${CMAKE_SOURCE_DIR}/windows/runtime")
endif()

if(WIN32)
    set_target_properties(BitcoinNodeDesktop PROPERTIES
        WIN32_EXECUTABLE TRUE
    )

    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt6
        HINTS "${Qt6_DIR}/../../../bin"
        PATH_SUFFIXES bin
    )

    add_custom_command(TARGET BitcoinNodeDesktop POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${BITCOIN_QT_RUNTIME_SOURCE_DIR}"
            "$<TARGET_FILE_DIR:BitcoinNodeDesktop>/runtime"
        COMMENT "Copying owned service runtime beside Windows executable"
    )

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET BitcoinNodeDesktop POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                "$<TARGET_FILE:BitcoinNodeDesktop>"
            COMMENT "Deploying Qt runtime with windeployqt"
        )
    else()
        message(WARNING "windeployqt was not found. The Windows executable will need Qt DLLs deployed manually.")
    endif()
endif()

if(APPLE)
    set_source_files_properties(resources/AppLogo.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(BitcoinNodeDesktop PRIVATE resources/AppLogo.icns)
    set_target_properties(BitcoinNodeDesktop PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "Bitcoin-Qt"
        MACOSX_BUNDLE_BUNDLE_NAME "Bitcoin-Qt"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.bitcoin.qt"
        MACOSX_BUNDLE_ICON_FILE "AppLogo.icns"
    )

    add_custom_command(TARGET BitcoinNodeDesktop POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:BitcoinNodeDesktop>/Resources"
        COMMAND /usr/bin/rsync -a --delete "${BITCOIN_QT_RUNTIME_SOURCE_DIR}/" "$<TARGET_BUNDLE_CONTENT_DIR:BitcoinNodeDesktop>/Resources/runtime/"
        COMMENT "Copying owned service runtime into app bundle"
    )
endif()

install(TARGETS BitcoinNodeDesktop
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
