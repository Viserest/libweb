
#include <string.h>

#include "router.h"
#include "http/http_parser.h"

/**
 * Disqualifies routes that do not match the byte currently being parsed for a given field.
 */
void filter_by_char(HttpParser *parser, i8 c, usize offset, i32 field_type) {
    // field_type: 0 = Method, 1 = URI, 2 = Host
    for (size_t i = 0; i < MAX_ROUTES; i++) {
        // Skip route if already disqualified
        if (!parser->candidates[i]) continue;

        // Determine target from field type
        const i8 *target = NULL;
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
void filter_finalize(HttpParser *parser, usize length, i32 field_type) {
    for (usize i = 0; i < MAX_ROUTES; i++) {
        // Skip route if already disqualified
        if (!parser->candidates[i]) continue;

        // Determine target from field type
        const i8 *target = NULL;
        if (field_type == 0) target = ROUTE_TABLE[i].method;
        else if (field_type == 1) target = ROUTE_TABLE[i].uri;
        else if (field_type == 2) target = ROUTE_TABLE[i].host;

        // If the route's string is longer than the parsed token length, it's a mismatch
        if (target != NULL && target[length] != '\0') {
            parser->candidates[i] = 0;
        }
    }
}

void router_handle_client(i32 client_fd) {
    HttpParser parser;
    memset(&parser, 0, sizeof(HttpParser));
    parser.state = HTTP_STATE_METHOD;
    parser.content_length = -1;

    // Assume all routes are candidates initially
    memset(parser.candidates, 1, MAX_ROUTES);

    while (parser.state != HTTP_STATE_COMPLETE && parser.state != HTTP_STATE_ERROR) {
        parser.bytes_read = recv(client_fd, parser.buffer, sizeof(parser.buffer), 0);

        if (parser.bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Back-off briefly
                usleep(10000);
                continue;
            }
            parser.state = HTTP_STATE_ERROR;
            break;
        } else if (parser.bytes_read == 0) {
            parser.state = (
                parser.state == HTTP_STATE_BODY &&
                parser.body_bytes_read >= parser.content_length
            ) ? HTTP_STATE_COMPLETE : HTTP_STATE_ERROR;
            break;
        }

        http_parse(&parser);
    }

    // Final execution
    if (parser.state == HTTP_STATE_COMPLETE) {
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
        router_handle_client(client_fd);
    }
    close(server_fd);
    return;
}
