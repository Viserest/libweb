
#ifndef JSON_PARSER_H_
#define JSON_PARSER_H_

#include "../types.h"

#ifndef JSON_BUFFER_SIZE
#define JSON_BUFFER_SIZE (1 * 1024)
#endif

#ifndef MAX_DEPTH
#define MAX_DEPTH (32)
#endif

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN (256)
#endif

#ifndef MAX_TOKEN_LEN
#define MAX_TOKEN_LEN (128)
#endif

typedef enum {
    JSON_STATE_START,
    JSON_STATE_KEY,
    JSON_STATE_VALUE_STRING,
    JSON_STATE_VALUE_NUMBER,
    JSON_STATE_COMPLETE,
    JSON_STATE_ERROR
} JsonParserState;

// Event callbacks executed natively during the active stream
void on_match(const i8 *key, const i8 *value);

typedef struct {
    JsonParserState state;
    i8 buffer[JSON_BUFFER_SIZE];
    u64 buffer_size;
    i32 top;
    i8 path_stack[MAX_DEPTH][MAX_TOKEN_LEN];

    i8 current_key[MAX_TOKEN_LEN];
    i8 full_path_str[MAX_PATH_LEN];

    i64 body_bytes_read;
} JsonParser;

void parse_json(JsonParser* parser);
void parse_json_stream(i32 client_fd);

#endif // JSON_PARSER_H_
