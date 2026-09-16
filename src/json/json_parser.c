
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#include "json_parser.h"

void parse_json(JsonParser* parser) {
    while (parser->state != JSON_STATE_COMPLETE && parser->state != JSON_STATE_ERROR) {
        for (i32 i = 0; i < parser->buffer_size; i++) {
            i8 c = parser->buffer[i];

            // Skip whitespace characters outside of raw string literals
            if (parser->state != JSON_STATE_KEY && parser->state != JSON_STATE_VALUE_STRING) {
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    continue;
                }
            }

            printf("%c", c);

            /*
            switch (parser.state) {
                case JSON_STATE_START:
                    if (c == '[') {
                        // Array start
                        parser.nested_level++;
                        parser.state = JSON_STATE_BEFORE_VALUE;
                    } else if (c == '{') {
                        // Object start
                        parser.nested_level++;
                        parser.state = JSON_STATE_BEFORE_KEY;
                    } else if (c == '"') {
                        // Value start
                        parser.state = JSON_STATE_VALUE;
                    }
                    break;

                case JSON_STATE_BEFORE_KEY:
                    if (c == ',') {}
                    else if (c == ' ') {}
                    else if (c == '"') {
                        parser.state = JSON_STATE_KEY;
                    }
                    else {
                        parser.state = JSON_STATE_ERROR;
                    }
                    break;

                case JSON_STATE_KEY:
                    if (c == '"') {
                        // Set quote to null term
                        parser.buffer[parser.buffer_index] = '\0';

                        // Copy from start of header to cursor
                        strncpy(
                            parser.current_key,
                            parser.buffer,
                            sizeof(parser.current_key) - 1
                        );
                        parser.buffer_index = 0;

                        // Continue to JSON_STATE_BEFORE_VALUE
                        parser.state = JSON_STATE_BEFORE_VALUE;
                    } else {
                        // Continue through until quote
                        if (parser.buffer_index < JSON_BUFFER_SIZE - 1) {
                            parser.buffer[parser.buffer_index++] = c;
                        }
                    }
                    break;

                case JSON_STATE_BEFORE_VALUE:
                    if (c == ':') {}
                    else if (c == ' ') {}
                    else if (c == '"') {
                        parser.state = JSON_STATE_VALUE;
                    }
                    else if (c == '[' || c == '{') {
                        parser.nested_level++;
                        parser.state = JSON_STATE_BEFORE_VALUE;
                    }
                    else {
                        parser.state = JSON_STATE_ERROR;
                    }
                    break;

                case JSON_STATE_VALUE:
                    if (c == ']' || c == '}') {
                        parser.nested_level--;
                        parser.state = JSON_STATE_NEXT;
                    }
                    if (c == '"') {
                        // Set quote to null term
                        parser.buffer[parser.buffer_index] = '\0';
                        i8* value = parser.buffer;
                        // Run callback
                        parser.on_pair(parser.current_key, value);
                        // Reset
                        parser.buffer_index = 0;
                        parser.state = JSON_STATE_NEXT;
                    } else {
                        // Continue through until quote
                        if (parser.buffer_index < JSON_BUFFER_SIZE - 1) {
                            parser.buffer[parser.buffer_index++] = c;
                        }
                    }
                    break;

                case JSON_STATE_NEXT:
                    if (c == ' ') {}
                    else if (c == ',') {
                        parser.state = JSON_STATE_BEFORE_KEY;
                    }
                    else if (c == ']') {
                        parser.nested_level--;
                        parser.state = JSON_
                    }
                    else if (c == '}') {
                        parser.nested_level--;
                        parser.state = JSON_
                    }
                    else {
                        parser.state = JSON_STATE_ERROR;
                    }
                    break;

                default:
                    parser.state = JSON_STATE_ERROR;
                    break;
            }
            */
        }
    }
}

void parse_json_stream(i32 client_fd) {
    JsonParser parser;
    memset(&parser, 0, sizeof(JsonParser));
    parser.state = JSON_STATE_START;

    i32 bytes_read;

    while (parser.state != JSON_STATE_COMPLETE && parser.state != JSON_STATE_ERROR) {
        bytes_read = recv(client_fd, parser.buffer, sizeof(parser.buffer), 0);

        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Back-off briefly
                usleep(10000);
                continue;
            }
            parser.state = JSON_STATE_ERROR;
            break;
        } else if (bytes_read == 0) {
            parser.state = JSON_STATE_COMPLETE;
            break;
        }

        parse_json(&parser);
    }
}
