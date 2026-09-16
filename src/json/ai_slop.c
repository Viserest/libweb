#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024
#define MAX_DEPTH 32
#define MAX_PATH_LEN 256
#define MAX_TOKEN_LEN 128

// Parser States
typedef enum {
    STATE_WANT_START,   // Looking for initial '{'
    STATE_WANT_KEY,     // Expecting a key string or '}'
    STATE_IN_KEY,       // Inside a key string
    STATE_WANT_COLON,   // Expecting ':'
    STATE_WANT_VAL,     // Expecting a value (string, number, or '{')
    STATE_IN_VAL_STR,   // Inside a string value
    STATE_IN_VAL_NUM    // Inside a number value
} ParserState;

// Simple structure to hold path context
typedef struct {
    char path_stack[MAX_DEPTH][MAX_TOKEN_LEN];
    int top;
} PathTracker;

// Functions to maintain our dot-notation path
void path_push(PathTracker *pt, const char *key) {
    if (pt->top < MAX_DEPTH - 1) {
        pt->top++;
        strncpy(pt->path_stack[pt->top], key, MAX_TOKEN_LEN - 1);
        pt->path_stack[pt->top][MAX_TOKEN_LEN - 1] = '\0';
    }
}

void path_pop(PathTracker *pt) {
    if (pt->top >= 0) {
        pt->top--;
    }
}

void path_get_dot_string(PathTracker *pt, char *buffer, int max_len) {
    buffer[0] = '\0';
    for (int i = 0; i <= pt->top; i++) {
        strncat(buffer, pt->path_stack[i], max_len - strlen(buffer) - 1);
        if (i < pt->top) {
            strncat(buffer, ".", max_len - strlen(buffer) - 1);
        }
    }
}

// User-defined callback fired whenever a target value matches
void on_match(const char *current_path, const char *value) {
    printf("[MATCH] Path '%s' => Value: %s\n", current_path, value);
}

// Main streaming JSON parser function
void parse_json_stream(int client_fd, const char *target_path) {
    char buf[BUFFER_SIZE];
    ParserState state = STATE_WANT_START;
    PathTracker pt = { .top = -1 };

    char current_key[MAX_TOKEN_LEN] = {0};
    int key_idx = 0;

    char current_val[MAX_TOKEN_LEN] = {0};
    int val_idx = 0;

    char full_path_str[MAX_PATH_LEN];
    ssize_t bytes_read;

    // Stream chunks from the socket descriptor
    while ((bytes_read = recv(client_fd, buf, sizeof(buf), 0)) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char c = buf[i];

            // Skip whitespace characters outside of raw string literals
            if (state != STATE_IN_KEY && state != STATE_IN_VAL_STR) {
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    continue;
                }
            }

            switch (state) {
                case STATE_WANT_START:
                    if (c == '{') state = STATE_WANT_KEY;
                    break;

                case STATE_WANT_KEY:
                    if (c == '"') {
                        state = STATE_IN_KEY;
                        key_idx = 0;
                    } else if (c == '}') {
                        // Level finished, pop path context
                        path_pop(&pt);
                    }
                    break;

                case STATE_IN_KEY:
                    if (c == '"') {
                        current_key[key_idx] = '\0';
                        path_push(&pt, current_key);
                        state = STATE_WANT_COLON;
                    } else if (key_idx < MAX_TOKEN_LEN - 1) {
                        current_key[key_idx++] = c;
                    }
                    break;

                case STATE_WANT_COLON:
                    if (c == ':') state = STATE_WANT_VAL;
                    break;

                case STATE_WANT_VAL:
                    if (c == '{') {
                        // Nested object encountered! State resets back to waiting for key
                        state = STATE_WANT_KEY;
                    } else if (c == '"') {
                        state = STATE_IN_VAL_STR;
                        val_idx = 0;
                    } else if ((c >= '0' && c <= '9') || c == '-') {
                        state = STATE_IN_VAL_NUM;
                        val_idx = 0;
                        current_val[val_idx++] = c;
                    }
                    break;

                case STATE_IN_VAL_STR:
                    if (c == '"') {
                        current_val[val_idx] = '\0';
                        path_get_dot_string(&pt, full_path_str, sizeof(full_path_str));

                        if (strcmp(full_path_str, target_path) == 0) {
                            on_match(full_path_str, current_val);
                        }

                        path_pop(&pt); // Finished processing this key's value
                        state = STATE_WANT_KEY; // A comma or '}' will be next, handles comma silently here
                    } else if (val_idx < MAX_TOKEN_LEN - 1) {
                        current_val[val_idx++] = c;
                    }
                    break;

                case STATE_IN_VAL_NUM:
                    if (c == ',' || c == '}') {
                        current_val[val_idx] = '\0';
                        path_get_dot_string(&pt, full_path_str, sizeof(full_path_str));

                        if (strcmp(full_path_str, target_path) == 0) {
                            on_match(full_path_str, current_val);
                        }

                        path_pop(&pt);
                        if (c == '}') {
                            path_pop(&pt); // Nested scope closed
                        }
                        state = STATE_WANT_KEY;
                    } else if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E') {
                        if (val_idx < MAX_TOKEN_LEN - 1) {
                            current_val[val_idx++] = c;
                        }
                    }
                    break;
            }
        }
    }
}
