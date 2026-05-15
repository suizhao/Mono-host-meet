cmake_minimum_required(VERSION 3.16)

project(MultiLive VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(LiveKitSDK)

livekit_sdk_setup(
  VERSION latest #"${LIVEKIT_SDK_VERSION}"
  SDK_DIR "${CMAKE_BINARY_DIR}/_deps/livekit-sdk"
  GITHUB_TOKEN "$ENV{GITHUB_TOKEN}"
)

find_package(LiveKit CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Quick Qml Network Multimedia Gui)

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(appMultiLive
    src/main.cpp
    src/domain/services/backend_service.h
    src/domain/services/room_service.h
    src/app/app_context.cpp
    src/app/env_config.cpp
    src/presentation/controllers/home_controller.cpp
    src/presentation/controllers/prejoin_controller.cpp
    src/presentation/controllers/room_controller.cpp
    src/presentation/controllers/video_tile_model.cpp
    src/presentation/qml/frame_image_provider.cpp
    src/infra/http/backend_service_qt.cpp
    src/infra/rtc/livekit_room_service.cpp
    src/infra/device/device_service_qt.cpp
)

target_include_directories(appMultiLive PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

qt_add_qml_module(appMultiLive
    URI MultiLive
    QML_FILES
        src/presentation/qml/App.qml
        src/presentation/qml/pages/HomePage.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appMultiLive PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appMultiLive
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appMultiLive
    PRIVATE
    Qt6::Quick
    Qt6::Qml
    Qt6::Network
    Qt6::Multimedia
    LiveKit::livekit
)

if (WIN32)
    target_link_libraries(appMultiLive PRIVATE d3d11 dxgi)
endif()

qt_add_executable(multilive_device_probe
    src/tools/device_probe_main.cpp
)

set_target_properties(multilive_device_probe PROPERTIES
    WIN32_EXECUTABLE FALSE
)

target_link_libraries(multilive_device_probe
    PRIVATE
    Qt6::Gui
    Qt6::Multimedia
)

install(TARGETS appMultiLive
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS multilive_device_probe
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
