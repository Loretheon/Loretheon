cmake_minimum_required(VERSION 3.23)
project(Lore VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QuaZip-Qt6 REQUIRED)
find_package(OpenMP REQUIRED)
find_package(faiss REQUIRED)
find_package(Qt6 6.4 REQUIRED COMPONENTS
        Core Gui Widgets PrintSupport Pdf PdfWidgets Network Multimedia
        MultimediaWidgets Svg SvgWidgets Xml Concurrent)
set(CMAKE_AUTORCC ON)
qt_standard_project_setup()

set(EPISTEME_NODE_BIN ""
        CACHE PATH "Directory containing Node.js command-line tools")

if (NOT EPISTEME_NODE_BIN)
    if (DEFINED ENV{NVM_BIN} AND EXISTS "$ENV{NVM_BIN}")
        set(EPISTEME_NODE_BIN "$ENV{NVM_BIN}")
    endif ()
endif ()

if (EPISTEME_NODE_BIN)
    list(PREPEND CMAKE_PROGRAM_PATH "${EPISTEME_NODE_BIN}")
endif ()

find_program(TREE_SITTER_CLI
        NAMES tree-sitter
        REQUIRED)

find_program(MERMAID_CLI
        NAMES mmdc
        REQUIRED)

find_program(GRAPHVIZ_DOT
        NAMES dot
        REQUIRED)

message(STATUS "TREE_SITTER_CLI=${TREE_SITTER_CLI}")
message(STATUS "MERMAID_CLI=${MERMAID_CLI}")
message(STATUS "GRAPHVIZ_DOT=${GRAPHVIZ_DOT}")

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

function(lore_external name dir)
    cmake_parse_arguments(EE "" "" "BYPRODUCTS;EXTRA_CMAKE_ARGS" ${ARGN})

    set(_build_byproducts "")
    foreach (_byproduct ${EE_BYPRODUCTS})
        list(APPEND _build_byproducts "${EPISTEME_EXTERNAL_PREFIX}/${_byproduct}")
    endforeach ()

    ExternalProject_Add(ext_${name}
            SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/${dir}"
            BINARY_DIR "${EPISTEME_EXTERNAL_BUILD_DIR}/${name}"
            STAMP_DIR "${EPISTEME_EXTERNAL_BUILD_DIR}/stamps/${name}"
            TMP_DIR "${EPISTEME_EXTERNAL_BUILD_DIR}/tmp/${name}"
            LOG_DIR "${EPISTEME_EXTERNAL_BUILD_DIR}/logs/${name}"
            INSTALL_DIR "${EPISTEME_EXTERNAL_PREFIX}"
            CMAKE_ARGS ${EPISTEME_EXT_CMAKE_ARGS} ${EE_EXTRA_CMAKE_ARGS}
            BUILD_BYPRODUCTS ${_build_byproducts}
            UPDATE_DISCONNECTED ON
            BUILD_ALWAYS OFF
            EXCLUDE_FROM_ALL ON
    )
endfunction()

function(lore_import_static target ep_target relative_path)
    set(_abs "${EPISTEME_EXTERNAL_PREFIX}/${relative_path}")

    if (NOT TARGET ${target})
        add_library(${target} STATIC IMPORTED GLOBAL)
        set_target_properties(${target} PROPERTIES IMPORTED_LOCATION "${_abs}")
        add_dependencies(${target} ${ep_target})
    endif ()
endfunction()

function(lore_ts_grammar name subdir header_subdir)
    set(_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/external/${subdir}")
    set(_gen_parser "${_src_dir}/src/parser.c")
    set(_gen_scanner "${_src_dir}/src/scanner.c")
    set(_header "${_src_dir}/${header_subdir}/tree-sitter-${name}.h")

    if (NOT EXISTS "${_gen_parser}")
        add_custom_command(
                OUTPUT "${_gen_parser}"
                COMMAND "${TREE_SITTER_CLI}" generate
                WORKING_DIRECTORY "${_src_dir}"
                DEPENDS "${_src_dir}/grammar.js"
                COMMENT "Generating tree-sitter-${name} parser"
                VERBATIM
        )
    endif ()

    set(_sources "${_gen_parser}")

    if (EXISTS "${_gen_scanner}")
        list(APPEND _sources "${_gen_scanner}")
    endif ()

    add_library(tree-sitter-${name} STATIC ${_sources})

    target_include_directories(tree-sitter-${name}
            PRIVATE "${_src_dir}/src")

    set_target_properties(tree-sitter-${name} PROPERTIES
            C_STANDARD 11
            POSITION_INDEPENDENT_CODE ON
            DEFINE_SYMBOL "")

    if (EXISTS "${_header}")
        set(_header_dest
                "${EPISTEME_EXTERNAL_PREFIX}/include/tree_sitter/tree-sitter-${name}.h")

        add_custom_command(
                TARGET tree-sitter-${name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "${EPISTEME_EXTERNAL_PREFIX}/include/tree_sitter"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_header}" "${_header_dest}"
                COMMENT "Installing tree-sitter-${name} header"
                VERBATIM
        )
    endif ()
endfunction()

lore_external(edlib edlib
        BYPRODUCTS
        lib/libedlib.a
        EXTRA_CMAKE_ARGS
        -DEDLIB_BUILD_EXAMPLES=OFF
        -DEDLIB_BUILD_UTILITIES=OFF
        -DEDLIB_ENABLE_INSTALL=ON)

lore_external(tree-sitter tree-sitter
        BYPRODUCTS
        lib/libtree-sitter.a
        EXTRA_CMAKE_ARGS
        -DBUILD_SHARED_LIBS=OFF)

lore_external(tree-sitter-markdown tree-sitter-markdown
        BYPRODUCTS
        lib/libtree-sitter-markdown.a
        lib/libtree-sitter-markdown-inline.a
        EXTRA_CMAKE_ARGS
        -DBUILD_SHARED_LIBS=OFF
        -DTREE_SITTER_CLI=${TREE_SITTER_CLI})

lore_external(lunasvg lunasvg
        BYPRODUCTS
        lib/liblunasvg.a
        lib/libplutovg.a
        EXTRA_CMAKE_ARGS
        -DBUILD_SHARED_LIBS=OFF
        -DLUNASVG_BUILD_EXAMPLES=OFF)

lore_import_static(ep_edlib ext_edlib lib/libedlib.a)
lore_import_static(ep_tree_sitter ext_tree-sitter lib/libtree-sitter.a)
lore_import_static(ep_tree_sitter_markdown ext_tree-sitter-markdown lib/libtree-sitter-markdown.a)
lore_import_static(ep_tree_sitter_md_inline ext_tree-sitter-markdown lib/libtree-sitter-markdown-inline.a)
lore_import_static(ep_lunasvg ext_lunasvg lib/liblunasvg.a)
lore_import_static(ep_plutovg ext_lunasvg lib/libplutovg.a)

lore_ts_grammar(dot tree-sitter-dot bindings/c)
lore_ts_grammar(mermaid tree-sitter-mermaid bindings/c)

add_subdirectory(
        external/QF-ML
        ${EPISTEME_EXTERNAL_BUILD_DIR}/QF-ML
)

add_executable(Lore
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
        src/text/preview/PreviewPane.cpp
        include/text/preview/PreviewPane.h
        src/text/preview/PreviewController.cpp
        include/text/preview/PreviewController.h
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
        src/ai/history/HistoryModel.cpp
        include/ai/history/HistoryModel.h
        src/ai/history/HistoryPanel.cpp
        include/ai/history/HistoryPanel.h
        src/text/structure/DocumentStructure.cpp
        include/text/structure/DocumentStructure.h
        src/text/structure/MarkdownStructureParser.cpp
        include/text/structure/MarkdownStructureParser.h
        src/text/structure/PlainTextStructureParser.cpp
        include/text/structure/PlainTextStructureParser.h
        src/text/structure/DotStructureParser.cpp
        include/text/structure/DotStructureParser.h
        src/text/structure/MermaidStructureParser.cpp
        include/text/structure/MermaidStructureParser.h
        src/text/structure/PlantUmlStructureParser.cpp
        include/text/structure/PlantUmlStructureParser.h
        src/text/structure/DocumentNode.cpp
        include/text/structure/DocumentNode.h
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
        resources/themes/resources.qrc
        resources/icons/resources.qrc
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
        src/ai/context/ContextModel.cpp
        include/ai/context/ContextModel.h
        src/ai/context/ContextPanel.cpp
        include/ai/context/ContextPanel.h
        src/overseer/OverseerSession.cpp
        include/overseer/OverseerSession.h
        src/overseer/OverseerStorage.cpp
        include/overseer/OverseerStorage.h
        src/overseer/OverseerWidget.cpp
        include/overseer/OverseerWidget.h
        src/agent/Tool.cpp
        include/agent/Tool.h
        src/agent/ToolRegistry.cpp
        include/agent/ToolRegistry.h
        src/agent/tools/Tools.cpp
        include/agent/tools/Tools.h
        src/overseer/OverseerOverviewEditor.cpp
        include/overseer/OverseerOverviewEditor.h
        src/text/DocumentArea.cpp
        include/text/DocumentArea.h
        src/overseer/ProposeMemoryFactTool.cpp
        include/overseer/ProposeMemoryFactTool.h
        src/ui/LlmSettingsPanel.cpp
        include/ui/LlmSettingsPanel.h
        src/overseer/MemoryProposalCard.cpp
        include/overseer/MemoryProposalCard.h
        src/overseer/ToastStack.cpp
        include/overseer/ToastStack.h
        src/overseer/EditNoteTool.cpp
        include/overseer/EditNoteTool.h
        src/overseer/EditNoteReviewDialog.cpp
        include/overseer/EditNoteReviewDialog.h
        src/overseer/WorkstationBodyFactory.cpp
        include/overseer/WorkstationBodyFactory.h
        src/overseer/WorkstationWindow.cpp
        include/overseer/WorkstationWindow.h
        src/overseer/Workstation.cpp
        include/overseer/Workstation.h
        src/overseer/OverseerSessionList.cpp
        include/overseer/OverseerSessionList.h
        src/overseer/OverseerSidePanel.cpp
        include/overseer/OverseerSidePanel.h
        src/overseer/OverseerPage.cpp
        include/overseer/OverseerPage.h
        src/overseer/TranscriptEvent.cpp
        include/overseer/TranscriptEvent.h
        src/overseer/TranscriptStore.cpp
        include/overseer/TranscriptStore.h
        src/overseer/CardWidget.cpp
        include/overseer/CardWidget.h
        src/overseer/TranscriptEventCard.cpp
        include/overseer/TranscriptEventCard.h
        src/overseer/TranscriptRibbon.cpp
        include/overseer/TranscriptRibbon.h
        src/overseer/TranscriptPanel.cpp
        include/overseer/TranscriptPanel.h
        src/overseer/PathUtils.cpp
        include/overseer/PathUtils.h
        src/overseer/MemoryFactCard.cpp
        include/overseer/MemoryFactCard.h
        src/overseer/MemoryPanel.cpp
        include/overseer/MemoryPanel.h
        src/overseer/OverseerReferenceCard.cpp
        include/overseer/OverseerReferenceCard.h
        src/overseer/OverviewPanel.cpp
        include/overseer/OverviewPanel.h
        src/overseer/MarkdownView.cpp
        include/overseer/MarkdownView.h
        src/overseer/WorkstationBar.cpp
        include/overseer/WorkstationBar.h
        src/overseer/AutoHideDock.cpp
        include/overseer/AutoHideDock.h
        src/overseer/DockReservation.cpp
        include/overseer/DockReservation.h
        src/overseer/OpenFileTool.cpp
        include/overseer/OpenFileTool.h
        src/overseer/CloseFileTool.cpp
        include/overseer/CloseFileTool.h
        src/overseer/EditWorkstationFileTool.cpp
        include/overseer/EditWorkstationFileTool.h
        src/overseer/TranscriptEditPlanCard.cpp
        include/overseer/TranscriptEditPlanCard.h
        src/overseer/SessionSettings.cpp
        include/overseer/SessionSettings.h
        src/overseer/AutomationStrip.cpp
        include/overseer/AutomationStrip.h
        src/app/NotificationService.cpp
        include/app/NotificationService.h
        src/app/NotificationToast.cpp
        include/app/NotificationToast.h
        include/app/theme/ThemeRegistry.h
        src/ui/SettingsDialog.cpp
        include/ui/SettingsDialog.h
        src/overseer/ConductorTypes.cpp
        include/overseer/ConductorTypes.h
        src/overseer/ConductorQueue.cpp
        include/overseer/ConductorQueue.h
        src/overseer/ConductorRoster.cpp
        include/overseer/ConductorRoster.h
        src/overseer/ConductorBoard.cpp
        include/overseer/ConductorBoard.h
        src/overseer/FileAgent.cpp
        include/overseer/FileAgent.h
        src/overseer/DependencyGraph.cpp
        include/overseer/DependencyGraph.h
        src/overseer/ConductorDock.cpp
        include/overseer/ConductorDock.h
        include/ingest/ExtractedDocument.h
        include/ingest/IngestOptions.h
        src/ingest/Extractor.cpp
        include/ingest/Extractor.h
        src/ingest/IngestRegistry.cpp
        include/ingest/IngestRegistry.h
        src/ingest/ZipReader.cpp
        include/ingest/ZipReader.h
        src/ingest/NoteWriter.cpp
        include/ingest/NoteWriter.h
        src/ingest/IngestService.cpp
        include/ingest/IngestService.h
        src/ingest/PdfExtractor.cpp
        include/ingest/PdfExtractor.h
        src/ingest/HtmlExtractor.cpp
        include/ingest/HtmlExtractor.h
        src/ingest/DocxExtractor.cpp
        include/ingest/DocxExtractor.h
        src/ingest/PptxExtractor.cpp
        include/ingest/PptxExtractor.h
        src/ingest/EpubExtractor.cpp
        include/ingest/EpubExtractor.h
        include/ingest/Extractors.h
        include/text/media/MediaKind.h
        include/text/media/MediaPane.h
        src/text/media/MediaPane.cpp
        src/text/media/MediaSettings.cpp
        include/text/media/MediaSettings.h
        include/voice/VoiceCommand.cpp
        include/voice/VoiceCommand.h
        include/voice/VoiceCommandRegistry.cpp
        include/voice/VoiceCommandRegistry.h
        include/voice/SpeechController.cpp
        include/voice/SpeechController.h
        include/voice/DictateCommand.cpp
        include/voice/DictateCommand.h
        include/voice/ReadAloudCommand.cpp
        include/voice/ReadAloudCommand.h
        include/voice/SpeechPanel.cpp
        include/voice/SpeechPanel.h
        include/voice/LiveDictateCommand.cpp
        include/voice/LiveDictateCommand.h
        src/search/VectorIndex.cpp
        include/search/VectorIndex.h
        include/search/SearchHit.h
        src/search/ScopeIndex.cpp
        include/search/ScopeIndex.h
        src/search/SearchService.cpp
        include/search/SearchService.h
        src/search/SearchDialog.cpp
        include/search/SearchDialog.h
        src/search/SearchPage.cpp
        include/search/SearchPage.h
        src/search/RetrievalLoop.cpp
        include/search/RetrievalLoop.h
        src/text/LoreTrigger.cpp
        include/text/LoreTrigger.h
        src/search/LoreInputDialog.cpp
        include/text/LoreInputDialog.h
        src/agent/StructuredSchema.cpp
        include/agent/StructuredSchema.h
        src/agent/StructuredSchemaRegistry.cpp
        include/agent/StructuredSchemaRegistry.h
        src/agent/StructuredCall.cpp
        include/agent/StructuredCall.h
        src/agent/schemas/SufficiencySchema.cpp
        include/agent/schemas/SufficiencySchema.h
)

add_dependencies(Lore
        ext_edlib
        ext_tree-sitter
        ext_tree-sitter-markdown
        ext_lunasvg
)

target_compile_definitions(Lore PRIVATE
        EPISTEME_MERMAID_CLI="${MERMAID_CLI}"
        EPISTEME_GRAPHVIZ_DOT="${GRAPHVIZ_DOT}"
)

target_include_directories(Lore PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/app
        ${CMAKE_CURRENT_SOURCE_DIR}/include/overseer
        ${CMAKE_CURRENT_SOURCE_DIR}/include/app/theme
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text/formats
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text/model
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text/diagram
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text/preview
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text/structure
        ${CMAKE_CURRENT_SOURCE_DIR}/include/text/svg
        ${CMAKE_CURRENT_SOURCE_DIR}/include/file
        ${CMAKE_CURRENT_SOURCE_DIR}/include/file/model
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ai
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ai/context
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ai/history
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ai/chat
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ai/edit
        ${CMAKE_CURRENT_SOURCE_DIR}/include/search
        ${CMAKE_CURRENT_SOURCE_DIR}/include/agent
        ${CMAKE_CURRENT_SOURCE_DIR}/include/agent/tools
        ${CMAKE_CURRENT_SOURCE_DIR}/include/agent/schemas
        ${CMAKE_CURRENT_SOURCE_DIR}/include/voice
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ingest
        ${CMAKE_CURRENT_SOURCE_DIR}/external/QF-ML/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/edlib/edlib/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/tree-sitter/lib/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/tree-sitter-markdown/tree-sitter-markdown/bindings/c
        ${CMAKE_CURRENT_SOURCE_DIR}/external/tree-sitter-markdown/tree-sitter-markdown-inline/bindings/c
        ${CMAKE_CURRENT_SOURCE_DIR}/external/lunasvg/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/lunasvg/plutovg/include
        ${EPISTEME_EXTERNAL_PREFIX}/include
        ${EPISTEME_EXTERNAL_PREFIX}/include/tree_sitter
        ${EPISTEME_EXTERNAL_PREFIX}/include/lunasvg
        ${EPISTEME_EXTERNAL_PREFIX}/include/plutovg
)

set_target_properties(Lore PROPERTIES
        BUILD_RPATH "${EPISTEME_EXTERNAL_PREFIX}/lib"
        INSTALL_RPATH "${EPISTEME_EXTERNAL_PREFIX}/lib"
)

target_link_libraries(Lore PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::PrintSupport
        Qt6::Pdf
        Qt6::PdfWidgets
        Qt6::Network
        Qt6::Multimedia
        Qt6::MultimediaWidgets
        Qt6::Svg
        Qt6::SvgWidgets
        Qt6::Xml
        Qt6::Concurrent
        qf-inference
        ep_edlib
        ep_tree_sitter
        ep_tree_sitter_markdown
        ep_tree_sitter_md_inline
        tree-sitter-dot
        tree-sitter-mermaid
        ep_lunasvg
        ep_plutovg
        QuaZip::QuaZip
        faiss
        OpenMP::OpenMP_CXX
)