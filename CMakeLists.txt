cmake_minimum_required(VERSION 3.23)
project(Episteme VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS
        Core Gui Widgets PrintSupport Pdf PdfWidgets Network Multimedia Svg SvgWidgets Xml)
set(CMAKE_AUTORCC ON)
qt_standard_project_setup()

set(EPISTEME_NODE_BIN ""
        CACHE PATH "Directory containing Node.js command-line tools")

if(NOT EPISTEME_NODE_BIN)
    if(DEFINED ENV{NVM_BIN} AND EXISTS "$ENV{NVM_BIN}")
        set(EPISTEME_NODE_BIN "$ENV{NVM_BIN}")
    endif()
endif()

if(EPISTEME_NODE_BIN)
    list(PREPEND CMAKE_PROGRAM_PATH "${EPISTEME_NODE_BIN}")
endif()

find_program(TREE_SITTER_CLI
        NAMES tree-sitter
        REQUIRED)

find_program(MERMAID_CLI
        NAMES mmdc
        REQUIRED)

message(STATUS "TREE_SITTER_CLI=${TREE_SITTER_CLI}")
message(STATUS "MERMAID_CLI=${MERMAID_CLI}")

get_filename_component(EPISTEME_NODE_BIN_RESOLVED
        "${MERMAID_CLI}"
        DIRECTORY)

set(EPISTEME_EXTERNAL_BUILD_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake-build-libs"
        CACHE PATH "Directory where external libraries are built")

set(EPISTEME_EXTERNAL_PREFIX
        "${EPISTEME_EXTERNAL_BUILD_DIR}/install")

include(ExternalProject)

set(EPISTEME_EXT_CMAKE_ARGS
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${EPISTEME_EXTERNAL_PREFIX}
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
        -DCMAKE_PROGRAM_PATH=${EPISTEME_NODE_BIN_RESOLVED}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
)

function(episteme_external name dir)
    ExternalProject_Add(ext_${name}
            SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/${dir}"
            BINARY_DIR "${EPISTEME_EXTERNAL_BUILD_DIR}/${name}"
            STAMP_DIR "${EPISTEME_EXTERNAL_BUILD_DIR}/stamps/${name}"
            TMP_DIR "${EPISTEME_EXTERNAL_BUILD_DIR}/tmp/${name}"
            LOG_DIR "${EPISTEME_EXTERNAL_BUILD_DIR}/logs/${name}"
            INSTALL_DIR "${EPISTEME_EXTERNAL_PREFIX}"
            CMAKE_ARGS ${EPISTEME_EXT_CMAKE_ARGS} ${ARGN}
            UPDATE_DISCONNECTED ON
            BUILD_ALWAYS OFF
            EXCLUDE_FROM_ALL ON
    )
endfunction()

episteme_external(edlib edlib
        -DEDLIB_BUILD_EXAMPLES=OFF
        -DEDLIB_BUILD_UTILITIES=OFF
        -DEDLIB_ENABLE_INSTALL=ON)

episteme_external(tree-sitter tree-sitter
        -DBUILD_SHARED_LIBS=OFF)

episteme_external(tree-sitter-markdown tree-sitter-markdown
        -DBUILD_SHARED_LIBS=OFF
        -DTREE_SITTER_CLI=${TREE_SITTER_CLI})

episteme_external(lunasvg lunasvg
        -DBUILD_SHARED_LIBS=OFF
        -DLUNASVG_BUILD_EXAMPLES=OFF)

add_subdirectory(
        external/QF-ML
        ${EPISTEME_EXTERNAL_BUILD_DIR}/QF-ML
)

add_executable(Episteme
        src/main.cpp
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
        include/text/formats/TextFormatDelegate.h
        src/text/formats/MarkdownFormatDelegate.cpp
        include/text/formats/MarkdownFormatDelegate.h
        src/text/formats/HTMLFormatDelegate.cpp
        include/text/formats/HTMLFormatDelegate.h
        src/ai/chat/ChatWidget.cpp
        include/ai/chat/ChatWidget.h
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
        src/ai/edit/EditSessionWidget.cpp
        include/ai/edit/EditSessionWidget.h
        src/ai/edit/PendingEdit.cpp
        include/ai/edit/PendingEdit.h
        src/app/PayloadLogger.cpp
        include/app/PayloadLogger.h
        src/ai/edit/EditConflict.cpp
        include/ai/edit/EditConflict.h
        src/ai/edit/EditPlanner.cpp
        include/ai/edit/EditPlanner.h
        src/app/NotificationManager.cpp
        include/app/NotificationManager.h
        src/ai/chat/ChatWidgetLayout.cpp
        include/ai/chat/ChatWidgetLayout.h
        src/ai/chat/ChatWidgetSerialization.cpp
        include/ai/chat/ChatWidgetSerialization.h
        src/ai/chat/ChatWidgetTranscript.cpp
        include/ai/chat/ChatWidgetTranscript.h
        src/ai/chat/ChatWidgetEditFlow.cpp
        include/ai/chat/ChatWidgetEditFlow.h
        ${CMAKE_CURRENT_SOURCE_DIR}/styles/catppuccin-latte/resources.qrc
        ${CMAKE_CURRENT_SOURCE_DIR}/styles/catppuccin-frappe/resources.qrc
        ${CMAKE_CURRENT_SOURCE_DIR}/styles/catppuccin-macchiato/resources.qrc
        ${CMAKE_CURRENT_SOURCE_DIR}/styles/catppuccin-mocha/resources.qrc
        src/file/DirectoryExplorerSettings.cpp
        include/file/DirectoryExplorerSettings.h
        src/text/formats/DotSyntaxHighlighter.cpp
        include/text/formats/DotSyntaxHighlighter.h
        src/text/GraphvizRenderer.cpp
        include/text/GraphvizRenderer.h
        src/text/diagram/DiagramDocument.cpp
        include/text/diagram/DiagramDocument.h
        src/text/diagram/DiagramView.cpp
        include/text/diagram/DiagramView.h
        src/text/diagram/DiagramCanvas.cpp
        include/text/diagram/DiagramCanvas.h
        src/text/diagram/DiagramToolbar.cpp
        include/text/diagram/DiagramToolbar.h
        src/text/PlantUmlRenderer.cpp
        include/text/PlantUmlRenderer.h
        include/text/MermaidRenderer.h
        src/text/MermaidRenderer.cpp
        src/text/LunasvgRenderer.cpp
        include/text/LunasvgRenderer.h
        src/app/theme/ThemeRegistry.cpp
        include/app/theme/ThemeTokens.h
        src/app/theme/SvgThemer.cpp
        include/app/theme/SvgThemer.h
        include/app/theme/ThemeAware.h
        src/app/theme/ThemeManager.cpp
        include/app/theme/ThemeManager.h
        include/ai/context/ContextModel.cpp
        include/ai/context/ContextModel.h
        include/ai/context/ContextPanel.cpp
        include/ai/context/ContextPanel.h

)

add_dependencies(Episteme
        ext_edlib
        ext_tree-sitter
        ext_tree-sitter-markdown
        ext_lunasvg
)

target_compile_definitions(Episteme PRIVATE
        EPISTEME_MERMAID_CLI="${MERMAID_CLI}"
)

target_include_directories(Episteme PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/app
        ${CMAKE_CURRENT_SOURCE_DIR}/include/app/theme
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text/formats
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text/model
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text/diagram
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text/structure
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text/svg
        ${CMAKE_CURRENT_SOURCE_DIR}/include/file
        ${CMAKE_CURRENT_SOURCE_DIR}/include/file/model
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ai
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ai/context
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ai/chat
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ai/edit
        ${CMAKE_CURRENT_SOURCE_DIR}/external/QF-ML/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/edlib/edlib/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/tree-sitter/lib/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/tree-sitter-markdown/tree-sitter-markdown/bindings/c
        ${CMAKE_CURRENT_SOURCE_DIR}/external/lunasvg/include
        ${EPISTEME_EXTERNAL_PREFIX}/include
)

target_link_directories(Episteme PRIVATE
        ${EPISTEME_EXTERNAL_PREFIX}/lib
        ${EPISTEME_EXTERNAL_BUILD_DIR}/edlib
        ${EPISTEME_EXTERNAL_BUILD_DIR}/tree-sitter
        ${EPISTEME_EXTERNAL_BUILD_DIR}/tree-sitter-markdown
        ${EPISTEME_EXTERNAL_BUILD_DIR}/lunasvg
)

set_target_properties(Episteme PROPERTIES
        BUILD_RPATH "${EPISTEME_EXTERNAL_PREFIX}/lib"
        INSTALL_RPATH "${EPISTEME_EXTERNAL_PREFIX}/lib"
)

target_link_libraries(Episteme PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::PrintSupport
        Qt6::Pdf
        Qt6::PdfWidgets
        Qt6::Network
        Qt6::Multimedia
        Qt6::Svg
        Qt6::SvgWidgets
        Qt6::Xml
        qf-inference
        edlib
        tree-sitter
        tree-sitter-markdown
        ${EPISTEME_EXTERNAL_PREFIX}/lib/liblunasvg.a
        ${EPISTEME_EXTERNAL_PREFIX}/lib/libplutovg.a
)