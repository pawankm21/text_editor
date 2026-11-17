cmake_minimum_required(VERSION 3.16)

project(text_editor LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_executable(text_editor src/text_editor.c)

include(CTest)
if(BUILD_TESTING)
    add_executable(undo_redo_test tests/undo_redo_test.c)
    add_test(NAME undo_redo COMMAND undo_redo_test)
    add_executable(render_editor_test tests/render_editor_test.c)
    add_test(NAME render_editor COMMAND render_editor_test)
endif()
