#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<stdbool.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>

static const size_t MAX_HISTORY = 10000;


struct Line {
    char* content;
    struct Line* next;
    struct Line* prev;
};

struct Cursor {
    struct Line* line;
    size_t column;
};

enum ActionType {
    ACTION_INSERT_CHAR,
    ACTION_DELETE_CHAR,
    ACTION_SPLIT_LINE,
    ACTION_JOIN_LINES
};

struct Action {
    enum ActionType type;
    size_t line_index;
    size_t column;
    char character;
    bool created_line;
    bool created_lines;
    bool before_cursor_valid;
    size_t before_cursor_line;
    size_t before_cursor_column;
    bool after_cursor_valid;
    size_t after_cursor_line;
    size_t after_cursor_column;
    struct Action* next;
    struct Action* prev;
};

struct File {
    struct Line* head;
    struct Line* tail;
    size_t line_count;
    struct Action* undo_top;
    struct Action* undo_bottom;
    struct Action* redo_top;
    size_t undo_count;
};


/* Writing a text editor in C++
functional requirements
1. We should be able to open a file. - display contents of a file.
2. move cursor - left,right, up, down
3. scroll the file up down to view whole file
4. Edit at cursor - write, delete
5. do a live search - searching something should highlight the text, and we should be able to go to next or previous occurrence of the text - for now exact match
6. save file/ not save file
7. Undo redo

Non functional requirements:
1. The search should be fast, each keystroke
2. The editor should be able to handle large files, up to 1GB in size
3


Core entities:
1. File
2. Cursor
3. Line
4. Editor
5. Action


*/
static size_t display_column(const char* text, size_t column) {
    size_t width = 0;
    for (size_t i = 0; i < column && text[i]; i++) {
        width += text[i] == '\t' ? 8 - width % 8 : 1;
    }
    return width;
}

int render_editor(const struct File* file, const struct Cursor* cursor, FILE* output) {
    if (!file || !cursor || !output) {
        return 0;
    }

    size_t cursor_row = 0;
    size_t cursor_column = 0;
    if (cursor->line) {
        const struct Line* line = file->head;
        while (line && line != cursor->line) {
            line = line->next;
            cursor_row++;
        }
        if (!line) {
            return 0;
        }
        cursor_column = display_column(line->content, cursor->column);
    } else if (file->head) {
        return 0;
    }

    struct winsize size = {0};
    ioctl(fileno(output), TIOCGWINSZ, &size);
    size_t rows = size.ws_row ? size.ws_row : 24;
    size_t columns = size.ws_col ? size.ws_col : 80;
    size_t top = cursor_row >= rows ? cursor_row - rows + 1 : 0;
    size_t left = cursor_column >= columns ? cursor_column - columns + 1 : 0;
    const struct Line* line = file->head;
    for (size_t i = 0; i < top && line; i++) {
        line = line->next;
    }

    if (fputs("\033[H\033[2J", output) == EOF) {
        return 0;
    }
    for (size_t row = 0; row < rows && line; row++, line = line->next) {
        if (fprintf(output, "\033[%zu;1H", row + 1) < 0) {
            return 0;
        }
        size_t column = 0;
        for (const unsigned char* p = (const unsigned char*)line->content;
             *p && (column < left || column - left < columns); p++) {
            size_t width = *p == '\t' ? 8 - column % 8 : 1;
            // shortcut: ASCII cells only; add Unicode width handling with Unicode editing.
            int character = *p == '\t' ? ' ' : (*p >= 32 && *p < 127 ? *p : '?');
            for (size_t i = 0; i < width; i++, column++) {
                if (column >= left && column - left < columns
                    && fputc(character, output) == EOF) {
                    return 0;
                }
            }
        }
    }
    if (fprintf(output, "\033[%zu;%zuH", cursor_row - top + 1,
                cursor_column - left + 1) < 0) {
        return 0;
    }
    return fflush(output) == 0;
}
// Displaying the contents of a file
void display_file_contents(const char* filename) {

}
static void add_line(struct File* file, struct Line* new_line, struct Line* after_line);

void add_line_pos(struct File* file, struct Line* new_line, long pos) {
    if (!file || !new_line || pos < 0 || (size_t)pos > file->line_count) {
        return;
    }
    if (pos == 0) {
        add_line(file, new_line, NULL);
    } else if ((size_t)pos == file->line_count) {
        add_line(file, new_line, file->tail);
    } else {
        struct Line* current = file->head;
        for (long i = 1; i < pos; i++) {
            current = current->next;
        }
        add_line(file, new_line, current);
    }
}

static void add_line(struct File* file, struct Line* new_line, struct Line* after_line) {
    if (!file || !new_line) {
        return;
    }
    if (after_line == NULL) {
        new_line->next = file->head;
        new_line->prev = NULL;
        if (file->head) {
            file->head->prev = new_line;
        } else {
            file->tail = new_line;
        }
        file->head = new_line;
    }
    else {
        new_line->next = after_line->next;
        new_line->prev = after_line;
        if (after_line->next) {
            after_line->next->prev = new_line;
        } else {
            file->tail = new_line;
        }
        after_line->next = new_line;
    }
    file->line_count++;
}
int load_file(long buffer_size, const char* filename, struct File* file) {
    if (!file || buffer_size <= 0) {
        return 0;
    }
    FILE* fp = fopen(filename, "r+");
    if (!fp) {
        perror("Failed to open file");
        return 0;
    }
    file->head = NULL;
    file->tail = NULL;
    file->line_count = 0;
    file->undo_top = NULL;
    file->undo_bottom = NULL;
    file->redo_top = NULL;
    file->undo_count = 0;
    while (!feof(fp)) {
        char* buffer = malloc(buffer_size);
        if (fread(buffer, sizeof(char), buffer_size - 1, fp) > 0) {
            buffer[buffer_size - 1] = '\0'; // Null-terminate the buffer
            char* line_start = buffer;
            while (*line_start != '\0') {
                char* line_end = strchr(line_start, '\n');
                if (line_end) {
                    *line_end = '\0'; // Null-terminate the line
                }
                struct Line* new_line = malloc(sizeof(struct Line));
                new_line->content = strdup(line_start);
                new_line->next = NULL;
                new_line->prev = file->tail;
                if (file->tail) {
                    file->tail->next = new_line;
                }
                else {
                    file->head = new_line;
                }
                file->tail = new_line;
                file->line_count++;
                if (!line_end) {
                    break;
                }
                line_start = line_end + 1;

            }
            free(buffer);
        }
        fclose(fp);
        return 1;
    }
    return 0;
}

static struct Line* create_line(const char* content) {
    struct Line* line = malloc(sizeof(*line));
    if (!line) {
        return NULL;
    }

    size_t len = strlen(content);
    line->content = malloc(len + 1);
    if (!line->content) {
        free(line);
        return NULL;
    }
    memcpy(line->content, content, len + 1);
    line->next = NULL;
    line->prev = NULL;
    return line;
}

static struct Line* line_at(struct File* file, size_t index) {
    struct Line* line = file->head;
    while (line && index > 0) {
        line = line->next;
        index--;
    }
    return line;
}

static bool line_index_of(const struct File* file, const struct Line* target, size_t* index) {
    size_t current_index = 0;
    for (const struct Line* line = file->head; line; line = line->next, current_index++) {
        if (line == target) {
            *index = current_index;
            return true;
        }
    }
    return false;
}

static bool add_line_at(struct File* file, struct Line* line, size_t index) {
    if (index > file->line_count) {
        return false;
    }
    if (index == 0) {
        add_line(file, line, NULL);
    } else {
        add_line(file, line, line_at(file, index - 1));
    }
    return true;
}

static bool remove_line_at(struct File* file, size_t index) {
    struct Line* line = line_at(file, index);
    if (!line) {
        return false;
    }
    if (line->prev) {
        line->prev->next = line->next;
    } else {
        file->head = line->next;
    }
    if (line->next) {
        line->next->prev = line->prev;
    } else {
        file->tail = line->prev;
    }
    file->line_count--;
    free(line->content);
    free(line);
    return true;
}

static struct Action* create_action(struct File* file, struct Cursor* cursor,
                                    enum ActionType type, size_t line_index, size_t column) {
    struct Action* action = calloc(1, sizeof(*action));
    if (!action) {
        return NULL;
    }
    action->type = type;
    action->line_index = line_index;
    action->column = column;
    if (cursor->line) {
        if (!line_index_of(file, cursor->line, &action->before_cursor_line)) {
            free(action);
            return NULL;
        }
        action->before_cursor_valid = true;
        action->before_cursor_column = cursor->column;
    }
    return action;
}

static void free_action_chain(struct Action* action) {
    while (action) {
        struct Action* next = action->next;
        free(action);
        action = next;
    }
}

static void clear_redo_history(struct File* file) {
    free_action_chain(file->redo_top);
    file->redo_top = NULL;
}

static void push_undo_history(struct File* file, struct Action* action) {
    action->prev = NULL;
    action->next = file->undo_top;
    if (file->undo_top) {
        file->undo_top->prev = action;
    } else {
        file->undo_bottom = action;
    }
    file->undo_top = action;
    file->undo_count++;

    if (file->undo_count > MAX_HISTORY) {
        struct Action* oldest = file->undo_bottom;
        file->undo_bottom = oldest->prev;
        if (file->undo_bottom) {
            file->undo_bottom->next = NULL;
        } else {
            file->undo_top = NULL;
        }
        file->undo_count--;
        free(oldest);
    }
}

static void record_action(struct File* file, struct Cursor* cursor, struct Action* action) {
    if (cursor->line) {
        action->after_cursor_valid = true;
        line_index_of(file, cursor->line, &action->after_cursor_line);
        action->after_cursor_column = cursor->column;
    }
    clear_redo_history(file);
    push_undo_history(file, action);
}

static bool insert_char_at(struct File* file, size_t line_index, size_t column, char c) {
    struct Line* line = line_at(file, line_index);
    if (!line) {
        return false;
    }
    size_t len = strlen(line->content);
    if (column > len) {
        return false;
    }
    char* content = realloc(line->content, len + 2);
    if (!content) {
        return false;
    }
    line->content = content;
    memmove(content + column + 1, content + column, len - column + 1);
    content[column] = c;
    return true;
}

static bool delete_char_at(struct File* file, size_t line_index, size_t column) {
    struct Line* line = line_at(file, line_index);
    if (!line) {
        return false;
    }
    size_t len = strlen(line->content);
    if (column >= len) {
        return false;
    }
    memmove(line->content + column, line->content + column + 1, len - column);
    return true;
}

static bool split_line_at(struct File* file, size_t line_index, size_t column) {
    struct Line* line = line_at(file, line_index);
    if (!line) {
        return false;
    }
    size_t len = strlen(line->content);
    if (column > len) {
        return false;
    }
    struct Line* new_line = create_line(line->content + column);
    if (!new_line) {
        return false;
    }
    line->content[column] = '\0';
    add_line(file, new_line, line);
    return true;
}

static bool join_lines_at(struct File* file, size_t line_index, size_t column) {
    struct Line* line = line_at(file, line_index);
    if (!line || !line->next) {
        return false;
    }
    size_t len = strlen(line->content);
    size_t next_len = strlen(line->next->content);
    if (column != len || len == SIZE_MAX || next_len > SIZE_MAX - len - 1) {
        return false;
    }
    char* content = realloc(line->content, len + next_len + 1);
    if (!content) {
        return false;
    }
    line->content = content;
    memcpy(content + len, line->next->content, next_len + 1);
    return remove_line_at(file, line_index + 1);
}

static bool restore_cursor(struct File* file, struct Cursor* cursor, bool valid,
                           size_t line_index, size_t column) {
    if (!valid) {
        cursor->line = NULL;
        cursor->column = 0;
        return true;
    }
    struct Line* line = line_at(file, line_index);
    if (!line) {
        return false;
    }
    cursor->line = line;
    cursor->column = column;
    return true;
}

static bool apply_action(struct File* file, struct Cursor* cursor,
                         const struct Action* action, bool forward) {
    bool succeeded = false;
    switch (action->type) {
    case ACTION_INSERT_CHAR:
        if (forward) {
            if (action->created_line) {
                char text[] = { action->character, '\0' };
                struct Line* line = create_line(text);
                if (!line) {
                    return false;
                }
                succeeded = add_line_at(file, line, action->line_index);
                if (!succeeded) {
                    free(line->content);
                    free(line);
                    return false;
                }
            } else {
                succeeded = insert_char_at(file, action->line_index, action->column,
                                            action->character);
            }
        } else if (action->created_line) {
            succeeded = remove_line_at(file, action->line_index);
        } else {
            succeeded = delete_char_at(file, action->line_index, action->column);
        }
        break;
    case ACTION_DELETE_CHAR:
        succeeded = forward
            ? delete_char_at(file, action->line_index, action->column)
            : insert_char_at(file, action->line_index, action->column, action->character);
        break;
    case ACTION_SPLIT_LINE:
        if (action->created_lines) {
            if (forward) {
                struct Line* first = create_line("");
                struct Line* second = create_line("");
                if (!first || !second) {
                    if (first) {
                        free(first->content);
                        free(first);
                    }
                    if (second) {
                        free(second->content);
                        free(second);
                    }
                    return false;
                }
                if (!add_line_at(file, first, action->line_index)
                    || !add_line_at(file, second, action->line_index + 1)) {
                    free(second->content);
                    free(second);
                    return false;
                }
                succeeded = true;
            } else {
                succeeded = remove_line_at(file, action->line_index + 1)
                    && remove_line_at(file, action->line_index);
            }
        } else {
            succeeded = forward
                ? split_line_at(file, action->line_index, action->column)
                : join_lines_at(file, action->line_index, action->column);
        }
        break;
    case ACTION_JOIN_LINES:
        succeeded = forward
            ? join_lines_at(file, action->line_index, action->column)
            : split_line_at(file, action->line_index, action->column);
        break;
    }
    if (!succeeded) {
        return false;
    }
    return forward
        ? restore_cursor(file, cursor, action->after_cursor_valid,
                         action->after_cursor_line, action->after_cursor_column)
        : restore_cursor(file, cursor, action->before_cursor_valid,
                         action->before_cursor_line, action->before_cursor_column);
}

static struct Action* pop_undo_history(struct File* file) {
    struct Action* action = file->undo_top;
    if (!action) {
        return NULL;
    }
    file->undo_top = action->next;
    if (file->undo_top) {
        file->undo_top->prev = NULL;
    } else {
        file->undo_bottom = NULL;
    }
    file->undo_count--;
    action->next = NULL;
    action->prev = NULL;
    return action;
}

static struct Action* pop_redo_history(struct File* file) {
    struct Action* action = file->redo_top;
    if (action) {
        file->redo_top = action->next;
        action->next = NULL;
        action->prev = NULL;
    }
    return action;
}

static void push_redo_history(struct File* file, struct Action* action) {
    action->next = file->redo_top;
    action->prev = NULL;
    if (file->redo_top) {
        file->redo_top->prev = action;
    }
    file->redo_top = action;
}

int undo_edit(struct File* file, struct Cursor* cursor) {
    if (!file || !cursor || !file->undo_top) {
        return 0;
    }
    struct Action* action = file->undo_top;
    if (!apply_action(file, cursor, action, false)) {
        return 0;
    }
    action = pop_undo_history(file);
    push_redo_history(file, action);
    return 1;
}

int redo_edit(struct File* file, struct Cursor* cursor) {
    if (!file || !cursor || !file->redo_top) {
        return 0;
    }
    struct Action* action = file->redo_top;
    if (!apply_action(file, cursor, action, true)) {
        return 0;
    }
    action = pop_redo_history(file);
    push_undo_history(file, action);
    return 1;
}

int append(struct Cursor* cursor, char c, struct File* file) {
    if (!cursor || !file || c == '\0') {
        return 0;
    }

    struct Line* current_line = cursor->line;
    if (!current_line) {
        if (c == '\n') {
            struct Action* action = create_action(file, cursor, ACTION_SPLIT_LINE, 0, 0);
            struct Line* first_line = create_line("");
            struct Line* second_line = create_line("");
            if (!action || !first_line || !second_line) {
                free(action);
                if (first_line) {
                    free(first_line->content);
                    free(first_line);
                }
                if (second_line) {
                    free(second_line->content);
                    free(second_line);
                }
                return 0;
            }
            action->created_lines = true;
            add_line_at(file, first_line, 0);
            add_line_at(file, second_line, 1);
            cursor->line = second_line;
            cursor->column = 0;
            record_action(file, cursor, action);
            return 1;
        }

        struct Action* action = create_action(file, cursor, ACTION_INSERT_CHAR, 0, 0);
        char text[] = { c, '\0' };
        struct Line* first_line = create_line(text);
        if (!action || !first_line) {
            free(action);
            if (first_line) {
                free(first_line->content);
                free(first_line);
            }
            return 0;
        }
        action->created_line = true;
        action->character = c;
        if (!add_line_at(file, first_line, 0)) {
            free(action);
            free(first_line->content);
            free(first_line);
            return 0;
        }
        cursor->line = first_line;
        cursor->column = 1;
        record_action(file, cursor, action);
        return 1;
    }

    size_t len = strlen(current_line->content);
    if (cursor->column > len) {
        cursor->column = len;
    }
    size_t line_index;
    if (!line_index_of(file, current_line, &line_index)) {
        return 0;
    }

    if (c == '\n') {
        struct Action* action = create_action(file, cursor, ACTION_SPLIT_LINE,
                                              line_index, cursor->column);
        struct Line* new_line = create_line(current_line->content + cursor->column);
        if (!action || !new_line) {
            free(action);
            if (new_line) {
                free(new_line->content);
                free(new_line);
            }
            return 0;
        }

        current_line->content[cursor->column] = '\0';
        add_line(file, new_line, current_line);
        cursor->line = new_line;
        cursor->column = 0;
        record_action(file, cursor, action);
        return 1;
    }

    struct Action* action = create_action(file, cursor, ACTION_INSERT_CHAR,
                                          line_index, cursor->column);
    if (!action) {
        return 0;
    }
    char* content = realloc(current_line->content, len + 2);
    if (!content) {
        free(action);
        return 0;
    }
    current_line->content = content;
    memmove(content + cursor->column + 1,
            content + cursor->column,
            len - cursor->column + 1);
    content[cursor->column] = c;
    cursor->column++;
    action->character = c;
    record_action(file, cursor, action);
    return 1;
}

int backspace(struct Cursor* cursor, struct File* file) {
    if (!cursor || !file || !cursor->line) {
        return 0;
    }

    struct Line* current_line = cursor->line;
    size_t current_len = strlen(current_line->content);
    if (cursor->column > current_len) {
        cursor->column = current_len;
    }
    size_t current_index;
    if (!line_index_of(file, current_line, &current_index)) {
        return 0;
    }

    if (cursor->column > 0) {
        size_t deleted_column = cursor->column - 1;
        struct Action* action = create_action(file, cursor, ACTION_DELETE_CHAR,
                                              current_index, deleted_column);
        if (!action) {
            return 0;
        }
        action->character = current_line->content[deleted_column];
        memmove(current_line->content + deleted_column,
                current_line->content + cursor->column,
                current_len - cursor->column + 1);
        cursor->column--;
        record_action(file, cursor, action);
        return 1;
    }

    struct Line* previous_line = current_line->prev;
    if (!previous_line) {
        return 1;
    }
    size_t previous_index;
    if (!line_index_of(file, previous_line, &previous_index)) {
        return 0;
    }
    size_t previous_len = strlen(previous_line->content);
    struct Action* action = create_action(file, cursor, ACTION_JOIN_LINES,
                                          previous_index, previous_len);
    if (!action || !join_lines_at(file, previous_index, previous_len)) {
        free(action);
        return 0;
    }
    cursor->line = previous_line;
    cursor->column = previous_len;
    record_action(file, cursor, action);
    return 1;
}

int delete(struct Cursor* cursor, struct File* file) {
    if (!cursor || !file || !cursor->line) {
        return 0;
    }

    struct Line* current_line = cursor->line;
    size_t current_len = strlen(current_line->content);
    if (cursor->column > current_len) {
        cursor->column = current_len;
    }
    size_t current_index;
    if (!line_index_of(file, current_line, &current_index)) {
        return 0;
    }

    if (cursor->column < current_len) {
        struct Action* action = create_action(file, cursor, ACTION_DELETE_CHAR,
                                              current_index, cursor->column);
        if (!action) {
            return 0;
        }
        action->character = current_line->content[cursor->column];
        memmove(current_line->content + cursor->column,
                current_line->content + cursor->column + 1,
                current_len - cursor->column);
        record_action(file, cursor, action);
        return 1;
    }

    if (!current_line->next) {
        return 1;
    }
    struct Action* action = create_action(file, cursor, ACTION_JOIN_LINES,
                                          current_index, current_len);
    if (!action || !join_lines_at(file, current_index, current_len)) {
        free(action);
        return 0;
    }
    record_action(file, cursor, action);
    return 1;
}
// 1 2 3 _ 5 6 7
// 3
void run_editor() {
    struct File file = {0};
    struct Cursor cursor = {0};
    render_editor(&file, &cursor, stdout);
}
int main() {
    run_editor();
    return 0;
}
