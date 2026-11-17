# Text editor

A text editor prototype written in C.

## Layout

```text
src/text_editor.c        Editor implementation and program entry point
tests/undo_redo_test.c  Undo/redo regression test
tests/render_editor_test.c  Terminal rendering tests
CMakeLists.txt         Build and test configuration
build/                 Generated build output (ignored by Git)
```

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the prototype in an ANSI terminal with `./build/text_editor`. It draws an
empty editor once; keyboard input and file opening are not implemented yet.

`render_editor(file, cursor, output)` redraws the terminal and positions the cursor.
It uses the output terminal's dimensions (24 rows by 80 columns for a file stream),
clips long lines, and scrolls to keep the cursor visible. Tabs use eight-column
stops. Control characters and non-ASCII bytes display as `?`; Unicode editing
and display are not implemented. It returns 1 on success and 0 for invalid
arguments or output errors.
