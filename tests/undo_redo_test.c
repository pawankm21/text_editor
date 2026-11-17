#define main editor_main
#include "../src/text_editor.c"
#undef main

#undef NDEBUG
#include <assert.h>

int main(void) {
    struct File file = {0};
    struct Cursor cursor = {0};

    assert(append(&cursor, 'a', &file));
    assert(file.line_count == 1);
    assert(strcmp(file.head->content, "a") == 0);
    assert(cursor.line == file.head && cursor.column == 1);

    assert(undo_edit(&file, &cursor));
    assert(file.line_count == 0 && !file.head && !file.tail);
    assert(!cursor.line && cursor.column == 0);

    assert(redo_edit(&file, &cursor));
    assert(file.line_count == 1);
    assert(strcmp(file.head->content, "a") == 0);
    assert(cursor.line == file.head && cursor.column == 1);

    assert(undo_edit(&file, &cursor));
    assert(!undo_edit(&file, &cursor));
    assert(redo_edit(&file, &cursor));
    assert(!redo_edit(&file, &cursor));
    assert(strcmp(file.head->content, "a") == 0);

    assert(remove_line_at(&file, 0));
    free_action_chain(file.undo_top);
    free_action_chain(file.redo_top);
    return 0;
}
