cmake_minimum_required(VERSION 4.3)
project(Episteme)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6 REQUIRED COMPONENTS Core Gui Widgets
        OPTIONAL_COMPONENTS PrintSupport Pdf PdfWidgets)



qt_standard_project_setup()

add_executable(Episteme src/main.cpp
        src/app/MainWindow.cpp
        include/app/MainWindow.h
        src/text/model/TextDocument.cpp
        include/text/model/TextDocument.h
        src/text/TextBrowser.cpp
        include/text/TextBrowser.h
        src/text/TextEdit.cpp
        include/text/TextEdit.h
        src/text/TextWidget.cpp
        include/text/TextWidget.h
        src/file/model/FileSystemModel.cpp
        include/file/model/FileSystemModel.h
        src/file/FileSystemView.cpp
        include/file/FileSystemView.h
        src/app/Settings.cpp
        include/app/Settings.h
        src/file/FileWidget.cpp
        include/file/FileWidget.h
        src/app/DocumentManager.cpp
        include/app/DocumentManager.h
)

target_include_directories(Episteme PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/include/app/
${CMAKE_CURRENT_SOURCE_DIR}/include/text/
${CMAKE_CURRENT_SOURCE_DIR}/include/text/model
${CMAKE_CURRENT_SOURCE_DIR}/include/file/
${CMAKE_CURRENT_SOURCE_DIR}/include/file/model
)

target_link_libraries(Episteme PRIVATE
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport Qt6::Pdf Qt6::PdfWidgets )
