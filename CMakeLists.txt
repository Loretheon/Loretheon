cmake_minimum_required(VERSION 4.3)
project(Episteme)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6 REQUIRED COMPONENTS Core Gui Widgets
        OPTIONAL_COMPONENTS PrintSupport Pdf PdfWidgets)

# Make both Node.js and the tree-sitter CLI available to CMake
# when using an NVM-managed Node installation.
set(ENV{PATH}
        "/home/sujan/.nvm/versions/node/v22.21.0/bin:$ENV{PATH}"
)

find_program(TREE_SITTER_CLI
        NAMES tree-sitter
        REQUIRED
)

message(STATUS "TREE_SITTER_CLI=${TREE_SITTER_CLI}")

add_subdirectory(external/QF-ML)
add_subdirectory(external/edlib EXCLUDE_FROM_ALL)
add_subdirectory(external/tree-sitter)
add_subdirectory(external/tree-sitter-markdown)

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
        src/text/Toolbar.cpp
        include/text/Toolbar.h
        include/text/Toolbar.h
        include/text/TextFormatDelegate.h
        src/text/MarkdownFormatDelegate.cpp
        include/text/MarkdownFormatDelegate.h
        src/text/HTMLFormatDelegate.cpp
        include/text/HTMLFormatDelegate.h
        src/ai/ChatWidget.cpp
        include/ai/ChatWidget.h
        src/ai/edit/EditApplier.cpp
        include/ai/edit/EditApplier.h
        include/ai/edit/EditGrammar.h
        src/ai/edit/EditMatcher.cpp
        include/ai/edit/EditMatcher.h
        src/ai/edit/EditCommand.cpp
        include/ai/edit/EditCommand.h
        src/ai/edit/EditCandidateView.cpp
        include/ai/edit/EditCandidateView.h
        src/ai/edit/EditSession.cpp
        include/ai/edit/EditSession.h
        include/ai/edit/EditMatch.h
        src/text/structure/DocumentStructure.cpp
        include/text/structure/DocumentStructure.h
        src/text/structure/MarkdownStructureParser.cpp
        include/text/structure/MarkdownStructureParser.h
        src/text/structure/DocumentNode.cpp
        include/text/structure/DocumentNode.h
        src/text/structure/PlainTextStructureParser.cpp
        include/text/structure/PlainTextStructureParser.h
        include/ai/EditSessionWidget.cpp
        include/ai/EditSessionWidget.h
        include/ai/edit/PendingEdit.cpp
        include/ai/edit/PendingEdit.h
        src/app/PayloadLogger.cpp
        include/app/PayloadLogger.h
        src/ai/edit/EditConflict.cpp
        include/ai/edit/EditConflict.h
        src/ai/edit/EditPlanner.cpp
        include/ai/edit/EditPlanner.h
        src/app/NotificationManager.cpp
        include/app/NotificationManager.h
)

target_include_directories(Episteme PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/app/
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text/
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text/model
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text/structure
        ${CMAKE_CURRENT_SOURCE_DIR}/include/file/
        ${CMAKE_CURRENT_SOURCE_DIR}/include/file/model
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ai
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ai/edit
        ${CMAKE_CURRENT_SOURCE_DIR}/external/QF-ML/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/tree-sitter/lib/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/tree-sitter-markdown/tree-sitter-markdown/bindings/c
)

target_link_libraries(Episteme PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::PrintSupport
        Qt6::Pdf
        Qt6::PdfWidgets
        qf-inference
        edlib
        tree-sitter
        tree-sitter-markdown
)