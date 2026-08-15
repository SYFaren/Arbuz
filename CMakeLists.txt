cmake_minimum_required(VERSION 3.16)
project(Arbuz VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets PrintSupport)
find_package(ZLIB REQUIRED)

qt_add_executable(arbuz
    src/app/main.cpp
    src/app/mainwindow.cpp
    src/core/cellref.cpp
    src/core/exceldate.cpp
    src/core/numformat.cpp
    src/core/workbook.cpp
    src/core/sheetmodel.cpp
    src/core/sheetview.cpp
    src/core/griddelegate.cpp
    src/formula/formulaengine.cpp
    src/io/fileio.cpp
    src/ui/i18n.cpp
    src/ui/appsettings.cpp
    src/ui/arbuzicon.cpp
    src/ui/theme.cpp
    src/ui/firstrunwizard.cpp
    src/ui/creditsdialog.cpp
    src/ui/settingsdialog.cpp
    src/ui/functiondialog.cpp
    src/plugins/pluginhost.cpp
    src/test/selftest.cpp
    src/test/uitest.cpp
    resources/arbuz.qrc
)

target_include_directories(arbuz PRIVATE
    src
    src/app
    src/core
    src/formula
    src/io
    src/ui
    src/plugins
    src/test
    third_party/xlfparser/include
)

target_link_libraries(arbuz PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    ZLIB::ZLIB
)

if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/arbuz.ico"
        "${CMAKE_CURRENT_BINARY_DIR}/arbuz.ico"
        COPYONLY
    )
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/arbuz_icon.rc" "IDI_ICON1 ICON \"arbuz.ico\"\n")
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/arbuz_icon.rc" PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/arbuz.ico"
    )
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I ${CMAKE_CURRENT_BINARY_DIR}")
    target_sources(arbuz PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/arbuz_icon.rc")
    set_target_properties(arbuz PROPERTIES WIN32_EXECUTABLE TRUE)

    # C stub: no libstdc++/libgcc DLLs next to Arbuz.exe. Engine finds them in runtime\.
    add_executable(arbuz-launcher WIN32
        src/app/launcher.c
        "${CMAKE_CURRENT_BINARY_DIR}/arbuz_icon.rc"
    )
    set_target_properties(arbuz-launcher PROPERTIES
        WIN32_EXECUTABLE TRUE
        AUTOMOC OFF
        AUTORCC OFF
        AUTOUIC OFF
    )
    if(MINGW)
        target_link_options(arbuz-launcher PRIVATE -static-libgcc -static)
    endif()
endif()

install(TARGETS arbuz RUNTIME DESTINATION bin)
