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
    src/main.cpp
    src/i18n.cpp
    src/appsettings.cpp
    src/arbuzicon.cpp
    src/cellref.cpp
    src/exceldate.cpp
    src/numformat.cpp
    src/workbook.cpp
    src/formulaengine.cpp
    src/sheetmodel.cpp
    src/sheetview.cpp
    src/pluginhost.cpp
    src/fileio.cpp
    src/theme.cpp
    src/firstrunwizard.cpp
    src/creditsdialog.cpp
    src/settingsdialog.cpp
    src/functiondialog.cpp
    src/griddelegate.cpp
    src/mainwindow.cpp
    src/selftest.cpp
    src/uitest.cpp
    resources/arbuz.qrc
)

target_include_directories(arbuz PRIVATE
    src
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
        src/launcher.c
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
