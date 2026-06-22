
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "http_parser.h"
#include "types.h"
#include "common.h"

/**
 * Disqualifies routes that do not match the byte currently being parsed for a given field.
 */
void filter_by_char(HttpParserState *parser, char c, size_t offset, int field_type) {
    // field_type: 0 = Method, 1 = URI, 2 = Host
    for (size_t i = 0; i < MAX_ROUTES; i++) {
        // Skip route if already disqualified
        if (!parser->candidates[i]) continue;

        // Determine target from field type
        const char *target = NULL;
        if (field_type == 0) target = ROUTE_TABLE[i].method;
        else if (field_type == 1) target = ROUTE_TABLE[i].uri;
        else if (field_type == 2) target = ROUTE_TABLE[i].host;

        if (target == NULL || target[offset] != c) {
            parser->candidates[i] = 0; // Disqualified!
        }
    }
}

/**
 * Disqualifies routes that expected a longer string than what was actually sent.
 * Called when a delimiter (like a space or newline) finishes a token field.
 */
void filter_finalize(HttpParserState *parser, size_t length, int field_type) {
    for (size_t i = 0; i < MAX_ROUTES; i++) {
        // Skip route if already disqualified
        if (!parser->candidates[i]) continue;

        // Determine target from field type
        const char *target = NULL;
        if (field_type == 0) target = ROUTE_TABLE[i].method;
        else if (field_type == 1) target = ROUTE_TABLE[i].uri;
        else if (field_type == 2) target = ROUTE_TABLE[i].host;

        // If the route's string is longer than the parsed token length, it's a mismatch
        if (target != NULL && target[length] != '\0') {
            parser->candidates[i] = 0;
        }
    }
}

void handle_client(i32 client_fd) {
    HttpParserState parser;
    memset(&parser, 0, sizeof(HttpParserState));
    parser.state = STATE_METHOD;
    parser.content_length = -1;

    // Assume all routes are candidates initially
    memset(parser.candidates, 1, MAX_ROUTES);

    // Micro chunk buffer to maintain slow data streaming compatibility
    i8 read_buf[256];
    i32 bytes_read;

    while (parser.state != STATE_COMPLETE && parser.state != STATE_ERROR) {
        bytes_read = recv(client_fd, read_buf, sizeof(read_buf), 0);

        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Back-off briefly
                usleep(10000);
                continue;
            }
            parser.state = STATE_ERROR;
            break;
        } else if (bytes_read == 0) {
            parser.state = (
                parser.state == STATE_BODY &&
                parser.body_bytes_read >= parser.content_length
            ) ? STATE_COMPLETE : STATE_ERROR;
            break;
        }

        for (i32 i = 0; i < bytes_read; i++) {
            i8 c = read_buf[i];

            switch (parser.state) {
                case STATE_METHOD:
                    // Filter routes by char until space then STATE_SPACES_BEFORE_URI
                    if (c == ' ') {
                        filter_finalize(&parser, parser.match_char_idx, 0);
                        parser.match_char_idx = 0;
                        parser.state = STATE_SPACES_BEFORE_URI;
                    } else {
                        filter_by_char(&parser, c, parser.match_char_idx++, 0);
                    }
                    break;

                case STATE_SPACES_BEFORE_URI:
                    // Skip spaces until STATE_URI
                    if (c == ' ') {}
                    else {
                        parser.match_char_idx = 0;
                        filter_by_char(&parser, c, parser.match_char_idx++, 1);
                        parser.state = STATE_URI;
                    }
                    break;

                case STATE_URI:
                    // Filter routes by char until space then STATE_SPACES_BEFORE_VERSION
                    if (c == ' ') {
                        filter_finalize(&parser, parser.match_char_idx, 1);
                        parser.match_char_idx = 0;
                        parser.state = STATE_SPACES_BEFORE_VERSION;
                    } else {
                        filter_by_char(&parser, c, parser.match_char_idx++, 1);
                    }
                    break;

                case STATE_SPACES_BEFORE_VERSION:
                    // Skip spaces then STATE_HTTP_VERSION
                    if (c == ' ') {}
                    else {
                        parser.state = STATE_HTTP_VERSION;
                    }
                    break;

                case STATE_HTTP_VERSION:
                    // Either:
                    // 1. \r\n -> STATE_HEADER_START
                    // 2. \rX  -> STATE_ERROR
                    // 3. \n   -> STATE_HEADER_START
                    if (c == '\r') parser.state = STATE_EXPECT_LF1;
                    else if (c == '\n') {
                        parser.buffer_index = 0;
                        parser.state = STATE_HEADER_START;
                    }
                    break;

                case STATE_EXPECT_LF1:
                    // Only comes from STATE_HTTP_VERSION
                    if (c == '\n') {
                        parser.buffer_index = 0;
                        parser.state = STATE_HEADER_START;
                    }
                    else {
                        parser.state = STATE_ERROR;
                    }
                    break;

                case STATE_HEADER_START:
                    // End of headers
                    if (c == '\r') parser.state = STATE_EXPECT_FINAL_LF;
                    else if (c == '\n') parser.state = STATE_EXPECT_FINAL_LF;
                    // Next STATE_HEADER_NAME
                    else {
                        parser.buffer[parser.buffer_index++] = c;
                        parser.state = STATE_HEADER_NAME;
                    }
                    break;

                case STATE_HEADER_NAME:
                    if (c == ':') {
                        // Set colon to null term
                        parser.buffer[parser.buffer_index] = '\0';
                        // Copy from start of header to cursor
                        strncpy(
                            parser.current_header_name,
                            parser.buffer,
                            sizeof(parser.current_header_name) - 1
                        );
                        parser.buffer_index = 0;
                        parser.match_char_idx = 0;

                        // If header is Host
                        if (strcasecmp(parser.current_header_name, "Host") == 0) {
                            parser.is_host_header = 1;
                        } else {
                            parser.is_host_header = 0;
                        }

                        // Continue to STATE_HEADER_VALUE
                        parser.state = STATE_HEADER_VALUE;
                    } else {
                        if (parser.buffer_index >= sizeof(parser.current_header_name) - 1) {
                            parser.state = STATE_ERROR;
                            break;
                        }
                        // Continue through until colon
                        parser.buffer[parser.buffer_index++] = c;
                    }
                    break;

                case STATE_HEADER_VALUE:
                    if (c == '\r' || c == '\n') {
                        // Set \r or \n to null term
                        parser.buffer[parser.buffer_index] = '\0';
                        i8* value = parser.buffer;

                        // Trim leading whitespace for Content-Length parsing
                        while (*value == ' ') value++;
                        if (strcasecmp(parser.current_header_name, "Content-Length") == 0) {
                            parser.content_length = atol(value);
                        }

                        // If header is Host
                        if (parser.is_host_header) {
                            filter_finalize(&parser, parser.match_char_idx, 2);

                            // Host header complete, now assign callbacks
                            int match_idx = -1;
                            for (size_t idx = 0; idx < MAX_ROUTES; idx++) {
                                if (parser.candidates[idx]) {
                                    match_idx = (int)idx;
                                    break;
                                }
                            }

                            if (match_idx != -1) {
                                // Extract custom route behavior context
                                parser.matched_on_header   = ROUTE_TABLE[match_idx].on_header;
                                parser.matched_on_body     = ROUTE_TABLE[match_idx].on_body;
                                parser.matched_on_complete = ROUTE_TABLE[match_idx].on_complete;
                            } else {
                                // No route matched criteria
                                parser.state = STATE_ERROR;
                                break;
                            }
                        }

                        // Run callback if found
                        // TODO: This will never be called because matched_on_header
                        //       runs after all headers are parsed.
                        if (parser.matched_on_header) {
                            parser.matched_on_header(
                                client_fd,
                                parser.current_header_name,
                                value
                            );
                        }

                        // Reset
                        parser.buffer_index = 0;
                        parser.is_host_header = 0;

                        // Either:
                        // 1. \r\n -> STATE_HEADER_START
                        // 2. \rX  -> STATE_ERROR
                        // 3. X    -> STATE_HEADER_START
                        parser.state = (c == '\r') ? STATE_EXPECT_LF2 : STATE_HEADER_START;
                    } else {
                        // Dynamically filter Host header characters as they stream by
                        if (parser.is_host_header) {
                            // Skip leading spaces before streaming characters
                            // to the route filter
                            if (!(parser.match_char_idx == 0 && c == ' ')) {
                                filter_by_char(&parser, c, parser.match_char_idx++, 2);
                            }
                        }
                        // Continue through until \r or \n
                        if (parser.buffer_index < BUFFER_SIZE - 1) {
                            parser.buffer[parser.buffer_index++] = c;
                        }
                    }
                    break;

                case STATE_EXPECT_LF2:
                    if (c == '\n') parser.state = STATE_HEADER_START;
                    else parser.state = STATE_ERROR;
                    break;

                case STATE_EXPECT_FINAL_LF:
                    if (c == '\n') {
                        if (parser.content_length > 0) {
                            parser.state = STATE_BODY;
                        } else {
                            parser.state = STATE_COMPLETE;
                        }
                    } else {
                        parser.state = STATE_ERROR;
                    }
                    break;

                case STATE_BODY: {
                    usize remaining_chunk = bytes_read - i;
                    i64 remaining_body = parser.content_length - parser.body_bytes_read;
                    usize consume = (remaining_chunk < (usize)remaining_body) ? remaining_chunk : (usize)remaining_body;

                    // Stream directly into the matched route's body callback
                    if (parser.matched_on_body) {
                        parser.matched_on_body(client_fd, &read_buf[i], consume);
                    }

                    parser.body_bytes_read += consume;
                    // Fast forward loop past processed payload chunk
                    i += (consume - 1);

                    if (parser.body_bytes_read >= parser.content_length) {
                        parser.state = STATE_COMPLETE;
                    }
                    break;
                }

                default:
                    parser.state = STATE_ERROR;
                    break;
            }

            if (parser.state == STATE_COMPLETE || parser.state == STATE_ERROR) break;
        }
    }

    // Final execution
    if (parser.state == STATE_COMPLETE) {
        if (parser.matched_on_complete) {
            parser.matched_on_complete(client_fd);
        } else {
            // Default response if the route didn't supply custom lifecycle functions
            const char *res = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
            send(client_fd, res, strlen(res), 0);
        }
    } else {
        const char *bad = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, bad, strlen(bad), 0);
    }

    close(client_fd);
}

void router_serve() {
    printf("Starting router...\n");
    i32 server_fd, client_fd;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed\n");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind to server address
    if (bind(server_fd, (struct sockaddr*)&server_addr, addr_len) < 0) {
        perror("bind failed\n");
        return;
    }

    // Listen to host:port
    if (listen(server_fd, 10) < 0) {
        perror("listen failed\n");
        return;
    }
    printf("Router listening at %s:%d\n", HOST, PORT);

    // Accept connections in a loop
    while ((client_fd = accept(server_fd, (struct sockaddr*)&server_addr, &addr_len))) {
        printf("Accepting new connection...\n");
        handle_client(client_fd);
    }
    close(server_fd);
    return;
}
