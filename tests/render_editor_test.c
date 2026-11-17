#define main editor_main
#include "../src/text_editor.c"
#undef main

#undef NDEBUG
#include <assert.h>

static void expect_render(const struct File* file, const struct Cursor* cursor,
                          const char* expected) {
    FILE* output = tmpfile();
    assert(output);
    assert(render_editor(file, cursor, output));
    rewind(output);
    char actual[4096];
    size_t length = fread(actual, 1, sizeof(actual) - 1, output);
    actual[length] = '\0';
    assert(strcmp(actual, expected) == 0);
    assert(fclose(output) == 0);
}

int main(void) {
    struct File file = {0};
    struct Cursor cursor = {0};
    expect_render(&file, &cursor, "\033[H\033[2J\033[1;1H");

    struct Line second = {.content = "world"};
    struct Line first = {.content = "hi\t!\033", .next = &second};
    second.prev = &first;
    file.head = &first;
    file.tail = &second;
    file.line_count = 2;
    cursor.line = &first;
    cursor.column = 3;
    expect_render(&file, &cursor,
                  "\033[H\033[2J\033[1;1Hhi      !?\033[2;1Hworld\033[1;9H");
    assert(cursor.line == &first && cursor.column == 3);

    char long_line[83];
    memset(long_line, 'a', 81);
    long_line[81] = 'b';
    long_line[82] = '\0';
    first.content = long_line;
    cursor.column = 82;
    FILE* output = tmpfile();
    assert(output);
    assert(render_editor(&file, &cursor, output));
    rewind(output);
    char actual[256] = {0};
    size_t length = fread(actual, 1, sizeof(actual) - 1, output);
    assert(length > 0);
    char visible[81];
    memset(visible, 'a', 78);
    visible[78] = 'b';
    visible[79] = '\0';
    assert(strstr(actual, visible));
    assert(strstr(actual, "\033[1;80H"));
    assert(fclose(output) == 0);

    struct Line lines[25] = {0};
    for (size_t i = 0; i < 25; i++) {
        lines[i].content = i == 0 ? "hidden" : "";
        lines[i].next = i + 1 < 25 ? &lines[i + 1] : NULL;
    }
    file.head = lines;
    file.tail = &lines[24];
    file.line_count = 25;
    cursor.line = file.tail;
    cursor.column = 0;
    output = tmpfile();
    assert(output);
    assert(render_editor(&file, &cursor, output));
    rewind(output);
    memset(actual, 0, sizeof(actual));
    assert(fread(actual, 1, sizeof(actual) - 1, output) > 0);
    assert(!strstr(actual, "hidden"));
    assert(strstr(actual, "\033[24;1H"));
    assert(fclose(output) == 0);

    assert(!render_editor(NULL, &cursor, stdout));
    assert(!render_editor(&file, NULL, stdout));
    assert(!render_editor(&file, &cursor, NULL));
    cursor.line = &first;
    output = tmpfile();
    assert(output);
    assert(!render_editor(&file, &cursor, output));
    assert(ftell(output) == 0);
    assert(fclose(output) == 0);
    return 0;
}
