
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "http_parser.h"

void http_parse(HttpParser* parser) {
        for (i32 i = 0; i < parser->bytes_read; i++) {
            i8 c = parser->buffer[i];

            switch (parser->state) {
                case HTTP_STATE_METHOD:
                    // Filter routes by char until space then HTTP_STATE_SPACES_BEFORE_URI
                    if (c == ' ') {
                        filter_finalize(parser, parser->match_char_idx, 0);
                        parser->match_char_idx = 0;
                        parser->state = HTTP_STATE_SPACES_BEFORE_URI;
                    } else {
                        filter_by_char(parser, c, parser->match_char_idx++, 0);
                    }
                    break;

                case HTTP_STATE_SPACES_BEFORE_URI:
                    // Skip spaces until HTTP_STATE_URI
                    if (c == ' ') {}
                    else {
                        parser->match_char_idx = 0;
                        filter_by_char(parser, c, parser->match_char_idx++, 1);
                        parser->state = HTTP_STATE_URI;
                    }
                    break;

                case HTTP_STATE_URI:
                    // Filter routes by char until space then HTTP_STATE_SPACES_BEFORE_VERSION
                    if (c == ' ') {
                        filter_finalize(parser, parser->match_char_idx, 1);
                        parser->match_char_idx = 0;
                        parser->state = HTTP_STATE_SPACES_BEFORE_VERSION;
                    } else {
                        filter_by_char(parser, c, parser->match_char_idx++, 1);
                    }
                    break;

                case HTTP_STATE_SPACES_BEFORE_VERSION:
                    // Skip spaces then HTTP_STATE_HTTP_VERSION
                    if (c == ' ') {}
                    else {
                        parser->state = HTTP_STATE_HTTP_VERSION;
                    }
                    break;

                case HTTP_STATE_HTTP_VERSION:
                    // Either:
                    // 1. \r\n -> HTTP_STATE_HEADER_START
                    // 2. \rX  -> HTTP_STATE_ERROR
                    // 3. \n   -> HTTP_STATE_HEADER_START
                    if (c == '\r') parser->state = HTTP_STATE_EXPECT_LF1;
                    else if (c == '\n') {
                        parser->buffer_index = 0;
                        parser->state = HTTP_STATE_HEADER_START;
                    }
                    break;

                case HTTP_STATE_EXPECT_LF1:
                    // Only comes from HTTP_STATE_HTTP_VERSION
                    if (c == '\n') {
                        parser->buffer_index = 0;
                        parser->state = HTTP_STATE_HEADER_START;
                    }
                    else {
                        parser->state = HTTP_STATE_ERROR;
                    }
                    break;

                case HTTP_STATE_HEADER_START:
                    // End of headers
                    if (c == '\r') parser->state = HTTP_STATE_EXPECT_FINAL_LF;
                    else if (c == '\n') parser->state = HTTP_STATE_EXPECT_FINAL_LF;
                    // Next HTTP_STATE_HEADER_NAME
                    else {
                        parser->buffer[parser->buffer_index++] = c;
                        parser->state = HTTP_STATE_HEADER_NAME;
                    }
                    break;

                case HTTP_STATE_HEADER_NAME:
                    if (c == ':') {
                        // Set colon to null term
                        parser->buffer[parser->buffer_index] = '\0';
                        // Copy from start of header to cursor
                        strncpy(
                            parser->current_header_name,
                            parser->buffer,
                            sizeof(parser->current_header_name) - 1
                        );
                        parser->buffer_index = 0;
                        parser->match_char_idx = 0;

                        // If header is Host
                        if (strcasecmp(parser->current_header_name, "Host") == 0) {
                            parser->is_host_header = 1;
                        } else {
                            parser->is_host_header = 0;
                        }

                        // Continue to HTTP_STATE_HEADER_VALUE
                        parser->state = HTTP_STATE_HEADER_VALUE;
                    } else {
                        if (parser->buffer_index >= sizeof(parser->current_header_name) - 1) {
                            parser->state = HTTP_STATE_ERROR;
                            break;
                        }
                        // Continue through until colon
                        parser->buffer[parser->buffer_index++] = c;
                    }
                    break;

                case HTTP_STATE_HEADER_VALUE:
                    if (c == '\r' || c == '\n') {
                        // Set \r or \n to null term
                        parser->buffer[parser->buffer_index] = '\0';
                        i8* value = parser->buffer;

                        // Trim leading whitespace for Content-Length parsing
                        while (*value == ' ') value++;
                        if (strcasecmp(parser->current_header_name, "Content-Length") == 0) {
                            parser->content_length = atol(value);
                        }

                        // If header is Host
                        if (parser->is_host_header) {
                            filter_finalize(parser, parser->match_char_idx, 2);

                            // Host header complete, now assign callbacks
                            int match_idx = -1;
                            for (size_t idx = 0; idx < MAX_ROUTES; idx++) {
                                if (parser->candidates[idx]) {
                                    match_idx = (int)idx;
                                    break;
                                }
                            }

                            if (match_idx != -1) {
                                // Extract custom route behavior context
                                parser->matched_on_header   = ROUTE_TABLE[match_idx].on_header;
                                parser->matched_on_body     = ROUTE_TABLE[match_idx].on_body;
                                parser->matched_on_complete = ROUTE_TABLE[match_idx].on_complete;
                            } else {
                                // No route matched criteria
                                parser->state = HTTP_STATE_ERROR;
                                break;
                            }
                        }

                        // Run callback if found
                        // TODO: This will never be called because matched_on_header
                        //       runs after all headers are parsed.
                        if (parser->matched_on_header) {
                            parser->matched_on_header(
                                client_fd,
                                parser->current_header_name,
                                value
                            );
                        }

                        // Reset
                        parser->buffer_index = 0;
                        parser->is_host_header = 0;

                        // Either:
                        // 1. \r\n -> HTTP_STATE_HEADER_START
                        // 2. \rX  -> HTTP_STATE_ERROR
                        // 3. X    -> HTTP_STATE_HEADER_START
                        parser->state = (c == '\r') ? HTTP_STATE_EXPECT_LF2 : HTTP_STATE_HEADER_START;
                    } else {
                        // Dynamically filter Host header characters as they stream by
                        if (parser->is_host_header) {
                            // Skip leading spaces before streaming characters
                            // to the route filter
                            if (!(parser->match_char_idx == 0 && c == ' ')) {
                                filter_by_char(parser, c, parser->match_char_idx++, 2);
                            }
                        }
                        // Continue through until \r or \n
                        if (parser->buffer_index < HTTP_BUFFER_SIZE - 1) {
                            parser->buffer[parser->buffer_index++] = c;
                        }
                    }
                    break;

                case HTTP_STATE_EXPECT_LF2:
                    if (c == '\n') parser->state = HTTP_STATE_HEADER_START;
                    else parser->state = HTTP_STATE_ERROR;
                    break;

                case HTTP_STATE_EXPECT_FINAL_LF:
                    if (c == '\n') {
                        if (parser->content_length > 0) {
                            parser->state = HTTP_STATE_BODY;
                        } else {
                            parser->state = HTTP_STATE_COMPLETE;
                        }
                    } else {
                        parser->state = HTTP_STATE_ERROR;
                    }
                    break;

                case HTTP_STATE_BODY: {
                    usize remaining_chunk = bytes_read - i;
                    i64 remaining_body = parser->content_length - parser->body_bytes_read;
                    usize consume = (remaining_chunk < (usize)remaining_body) ? remaining_chunk : (usize)remaining_body;

                    // Stream directly into the matched route's body callback
                    if (parser->matched_on_body) {
                        parser->matched_on_body(client_fd, &read_buf[i], consume);
                    }

                    parser->body_bytes_read += consume;
                    // Fast forward loop past processed payload chunk
                    i += (consume - 1);

                    if (parser->body_bytes_read >= parser->content_length) {
                        parser->state = HTTP_STATE_COMPLETE;
                    }
                    break;
                }

                default:
                    parser->state = HTTP_STATE_ERROR;
                    break;
            }

            if (parser->state == HTTP_STATE_COMPLETE || parser->state == HTTP_STATE_ERROR) break;
        }
    }
}
