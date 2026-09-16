
#ifndef HTTP_PARSER_H_
#define HTTP_PARSER_H_

#include "../common.h"
#include "../types.h"

#ifndef HTTP_BUFFER_SIZE
#define HTTP_BUFFER_SIZE (1 * 1024)
#endif

#ifndef HEADER_NAME_SIZE
#define HEADER_NAME_SIZE (128)
#endif

typedef enum {
    HTTP_STATE_METHOD,
    HTTP_STATE_SPACES_BEFORE_URI,
    HTTP_STATE_URI,
    HTTP_STATE_SPACES_BEFORE_VERSION,
    HTTP_STATE_HTTP_VERSION,
    HTTP_STATE_EXPECT_LF1,
    HTTP_STATE_HEADER_START,
    HTTP_STATE_HEADER_NAME,
    HTTP_STATE_HEADER_VALUE,
    HTTP_STATE_EXPECT_LF2,
    HTTP_STATE_EXPECT_FINAL_LF,
    HTTP_STATE_BODY,
    HTTP_STATE_COMPLETE,
    HTTP_STATE_ERROR
} HttpParserState;

typedef struct {
    HttpParserState state;
    i8 buffer[HTTP_BUFFER_SIZE];
    u32 buffer_index;
    i8 current_header_name[HEADER_NAME_SIZE];
    // Flag to know if the current header value
    // is being streamed as the Host header
    u8 is_host_header;

    u32 match_char_idx;

    // Content-Length header value
    i64 content_length;
    i64 bytes_read;
    i64 body_bytes_read;
} HttpParser;

void http_parse(HttpParser* parser);

#endif // HTTP_PARSER_H_
