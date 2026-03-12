cmake_minimum_required(VERSION 3.21)

project(QtMultiPlayer VERSION 0.1 LANGUAGES CXX)

include(CTest)

option(QT_MULTIPLAYER_SMOOTH_WINDOW_DRAG "Use manual title-bar dragging instead of native HTCAPTION dragging." ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.6.3/msvc2019_64")

find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets)

qt_standard_project_setup()

find_program(WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt windeployqt.exe
    HINTS "C:/Qt/6.6.3/msvc2019_64/bin"
)

qt_add_executable(QtMultiPlayer
    src/main.cpp
    src/app_config.cpp
    src/app_config.h
    src/clickable_slider.cpp
    src/clickable_slider.h
    src/screen_player_window.cpp
    src/screen_player_window.h
    src/video_player_widget.cpp
    src/video_player_widget.h
    resources.qrc
    app.rc
)

# Keep console for Debug, hide console window for non-Debug builds on Windows.
set_target_properties(QtMultiPlayer PROPERTIES
    WIN32_EXECUTABLE "$<NOT:$<CONFIG:Debug>>"
)

target_link_libraries(QtMultiPlayer
    PRIVATE
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::MultimediaWidgets
)

target_compile_definitions(QtMultiPlayer PRIVATE
    QT_DEPRECATED_WARNINGS
)

if(QT_MULTIPLAYER_SMOOTH_WINDOW_DRAG)
    target_compile_definitions(QtMultiPlayer PRIVATE QT_MULTIPLAYER_SMOOTH_WINDOW_DRAG=1)
endif()

install(TARGETS QtMultiPlayer RUNTIME DESTINATION .)
install(FILES README.md DESTINATION .)
install(FILES config.json DESTINATION .)

if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    install(CODE "
        execute_process(
            COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --release \"\${CMAKE_INSTALL_PREFIX}/QtMultiPlayer.exe\"
            RESULT_VARIABLE deploy_result
        )
        if(NOT deploy_result EQUAL 0)
            message(FATAL_ERROR \"windeployqt failed with code \${deploy_result}\")
        endif()
    ")

    add_custom_target(package_release
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_SOURCE_DIR}/dist/release"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist/release"
        COMMAND ${CMAKE_COMMAND}
                -DCMAKE_INSTALL_CONFIG_NAME=Release
                -P "${CMAKE_BINARY_DIR}/cmake_install.cmake"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_INSTALL_PREFIX}"
                "${CMAKE_SOURCE_DIR}/dist/release"
        COMMENT "Build deployable release bundle in dist/release"
        VERBATIM
    )
endif()

if(BUILD_TESTING)
    set(QT_RUNTIME_BIN_DIR "C:/Qt/6.6.3/msvc2019_64/bin")
    set(QT_RUNTIME_PLUGIN_DIR "C:/Qt/6.6.3/msvc2019_64/plugins")

    add_test(
        NAME config_test
        COMMAND QtMultiPlayer
                --validate-config
                -f
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/test_config.json
    )

    add_test(
        NAME app_smoke_test
        COMMAND QtMultiPlayer
                --test-mode
                --quit-after-ms 1200
                -platform offscreen
                -f ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/test_config.json
    )

    set(_test_environment
        "PATH=${QT_RUNTIME_BIN_DIR};$ENV{PATH}"
        "QT_PLUGIN_PATH=${QT_RUNTIME_PLUGIN_DIR}"
        "QT_QPA_PLATFORM_PLUGIN_PATH=${QT_RUNTIME_PLUGIN_DIR}/platforms"
    )

    set_tests_properties(config_test PROPERTIES ENVIRONMENT "${_test_environment}")
    set_tests_properties(app_smoke_test PROPERTIES ENVIRONMENT "${_test_environment}")
endif()
