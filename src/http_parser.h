
#ifndef HTTP_PARSER_H_
#define HTTP_PARSER_H_

#include "types.h"

#ifndef BUFFER_SIZE
#define BUFFER_SIZE (1 * 1024)
#endif

#ifndef HEADER_NAME_SIZE
#define HEADER_NAME_SIZE (128)
#endif

#ifndef MAX_ROUTES
#define MAX_ROUTES (32)
#endif

typedef enum {
    STATE_METHOD,
    STATE_SPACES_BEFORE_URI,
    STATE_URI,
    STATE_SPACES_BEFORE_VERSION,
    STATE_HTTP_VERSION,
    STATE_EXPECT_LF1,
    STATE_HEADER_START,
    STATE_HEADER_NAME,
    STATE_HEADER_VALUE,
    STATE_EXPECT_LF2,
    STATE_EXPECT_FINAL_LF,
    STATE_BODY,
    STATE_COMPLETE,
    STATE_ERROR
} ParserState;

// Event callbacks executed natively during the active stream
typedef void (*HeaderCallback)(i32 fd, const i8 *name, const i8 *value);
typedef void (*BodyCallback)(i32 fd, const i8 *chunk, u64 len);
typedef void (*CompleteCallback)(i32 fd);

typedef struct {
    const i8 *method;
    const i8 *uri;
    const i8 *host;
    HeaderCallback   on_header;
    BodyCallback     on_body;
    CompleteCallback on_complete;
} HttpRoute;

extern const HttpRoute ROUTE_TABLE[MAX_ROUTES];

typedef struct {
    ParserState state;
    i8 buffer[BUFFER_SIZE];
    u32 buffer_index;
    i8 current_header_name[HEADER_NAME_SIZE];
    // Stores list of potentially matching routes.
    //
    // 0 = disqualified
    // 1 = still potentially matching
    u8 candidates[MAX_ROUTES];
    // Flag to know if the current header value
    // is being streamed as the Host header
    u8 is_host_header;

    u32 match_char_idx;

    // Content-Length header value
    i64 content_length;
    // Length of body read
    i64 body_bytes_read;

    // Found dynamic route callbacks
    HeaderCallback   matched_on_header;
    BodyCallback     matched_on_body;
    CompleteCallback matched_on_complete;
} HttpParserState;

void router_serve();

#endif // HTTP_PARSER_H_
