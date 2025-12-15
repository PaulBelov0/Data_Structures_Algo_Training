cmake_minimum_required(VERSION 3.16)

project(Data_Structures_Algo_Training VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        src/main.cpp
        src/ui/main_window.cpp
        src/ui/main_window.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Network)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)

    qt_add_executable(Data_Structures_Algo_Training
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}

        src/ui/widgets/visualization/binary_tree_visualization.h src/ui/widgets/visualization/binary_tree_visualization.cpp
        src/core/internal/binary_tree/binary_tree.h src/core/internal/binary_tree/binary_tree.cpp
        src/core/internal/binary_tree/tree_node.h src/core/internal/binary_tree/tree_node.cpp
        src/core/generators/binary_tree_generator.h src/core/generators/binary_tree_generator.cpp
        src/ui/widgets/visualization/base/visualizer_base.h src/ui/widgets/visualization/base/visualizer_base.cpp
        src/ui/widgets/visualization/base/graphics_node.h src/ui/widgets/visualization/base/graphics_node.cpp
        src/ui/widgets/visualization/base/graphics_edge.h src/ui/widgets/visualization/base/graphics_edge.cpp
        src/ui/widgets/intelli_sense_widget/LSP/LSP_client.h src/ui/widgets/intelli_sense_widget/LSP/LSP_client.cpp
        src/ui/widgets/intelli_sense_widget/LSP/cpp_lsp_client.h src/ui/widgets/intelli_sense_widget/LSP/cpp_lsp_client.cpp
        src/ui/widgets/intelli_sense_widget/LSP/java_lsp_client.h src/ui/widgets/intelli_sense_widget/LSP/java_lsp_client.cpp
        src/ui/widgets/intelli_sense_widget/Editor/code_editor_widget.h src/ui/widgets/intelli_sense_widget/Editor/code_editor_widget.cpp
        src/ui/widgets/intelli_sense_widget/Editor/syntax_highlighter.h src/ui/widgets/intelli_sense_widget/Editor/syntax_highlighter.cpp
        src/ui/widgets/intelli_sense_widget/Editor/auto_complete_widget.h src/ui/widgets/intelli_sense_widget/Editor/auto_complete_widget.cpp
        src/ui/widgets/intelli_sense_widget/integration/ide_integrator.h src/ui/widgets/intelli_sense_widget/integration/ide_integrator.cpp
        src/ui/widgets/intelli_sense_widget/integration/settings_manager.h src/ui/widgets/intelli_sense_widget/integration/settings_manager.cpp
        src/ui/widgets/intelli_sense_widget/LSP/LSP_manager.h src/ui/widgets/intelli_sense_widget/LSP/LSP_manager.cpp
        src/ui/widgets/intelli_sense_widget/LSP/service/jdtls_installer.h src/ui/widgets/intelli_sense_widget/LSP/service/jdtls_installer.cpp
        src/ui/widgets/intelli_sense_widget/LSP/LSP.hpp
        src/ui/styles.h
        src/ui/widgets/intelli_sense_widget/LSP/service/lsp_installer.h src/ui/widgets/intelli_sense_widget/LSP/service/lsp_installer.cpp
        src/ui/widgets/intelli_sense_widget/LSP/service/clangd_installer.h src/ui/widgets/intelli_sense_widget/LSP/service/clangd_installer.cpp
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET Data_Structures_Algo_Training APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(Data_Structures_Algo_Training SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(Data_Structures_Algo_Training
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(Data_Structures_Algo_Training PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(Data_Structures_Algo_Training PRIVATE Qt6::Widgets)
target_link_libraries(Data_Structures_Algo_Training PRIVATE Qt6::Core)
target_link_libraries(Data_Structures_Algo_Training PRIVATE Qt6::Network)
target_link_libraries(Data_Structures_Algo_Training PRIVATE Qt6::Core)
target_link_libraries(Data_Structures_Algo_Training PRIVATE Qt6::Core)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Data_Structures_Algo_Training)
endif()
set_target_properties(Data_Structures_Algo_Training PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Data_Structures_Algo_Training
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Data_Structures_Algo_Training)
endif()
