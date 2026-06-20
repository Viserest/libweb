#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "router.h"

static char buffer[BUFFER_CAPACITY] = {0};

void _route_add(Router* router, char* path, Method method, void* handler) {
    Route route = {
        .path = path,
        .method = method,
        .handler = handler,
    };
    router_append(router, route);
}

// Finds request method by comparing the raw bytes
// Default method is HEAD
Method _route_handle_request_method(char* buffer, u32* i) {
    // TODO: Check if buffer is long enough at beginning of if statement
    if (buffer[*i++] == 'P' && buffer[*i++] == 'O' && buffer[*i++] == 'S' && buffer[*i++] == 'T' && buffer[*i++] == ' ') return METHOD_POST;
    *i = 0;
    if (buffer[*i++] == 'G' && buffer[*i++] == 'E' && buffer[*i++] == 'T' && buffer[*i++] == ' ') return METHOD_GET;
    *i = 0;
    if (buffer[*i++] == 'H' && buffer[*i++] == 'E' && buffer[*i++] == 'A' && buffer[*i++] == 'D' && buffer[*i++] == ' ') return METHOD_HEAD;
    *i = 0;
    if (buffer[*i++] == 'O' && buffer[*i++] == 'P' && buffer[*i++] == 'T' && buffer[*i++] == 'I' && buffer[*i++] == 'O' && buffer[*i++] == 'N' && buffer[*i++] == 'S' && buffer[*i++] == ' ') return METHOD_OPTIONS;
    *i = 0;
    if (buffer[*i++] == 'P' && buffer[*i++] == 'U' && buffer[*i++] == 'T' && buffer[*i++] == ' ') return METHOD_PUT;
    *i = 0;
    if (buffer[*i++] == 'P' && buffer[*i++] == 'A' && buffer[*i++] == 'T' && buffer[*i++] == 'C' && buffer[*i++] == 'H' && buffer[*i++] == ' ') return METHOD_PATCH;
    *i = 0;
    if (buffer[*i++] == 'D' && buffer[*i++] == 'E' && buffer[*i++] == 'L' && buffer[*i++] == 'E' && buffer[*i++] == 'T' && buffer[*i++] == 'E' && buffer[7] == ' ') return METHOD_DELETE;
    *i = 0;

    return METHOD_HEAD;
}

void _route_handle_request_line(Request* req, u32* i) {
    // Find method
    req->method = _route_handle_request_method(buffer, i);
    printf("Cursor after method: %d\n", *i);

    // Find uri by storing the current location,
    // then skipping to the next space,
    // and setting it to a null terminator
    req->uri = &buffer[*i];
    while (buffer[*i] != '\0' && buffer[*i] != ' ') {
        i++;
    }
    if (buffer[*i] == ' ') {
        buffer[*i] = '\0';
    }
    printf("Cursor after uri: %d\n", *i);

    // Find http version by storing current location,
    // then skipping to the next CRLF,
    // and setting it to a null terminator
    req->version = &buffer[*i];
    while (buffer[*i] != '\0' && buffer[*i-1] != '\r' && buffer[*i] != '\n') {
        i++;
    }
    if (buffer[*i-1] == '\r' && buffer[*i] == '\n') {
        buffer[*i-1] = '\0';
        buffer[*i] = '\0';
    }
    printf("Cursor after version: %d\n", *i);
}

void _route_handle_request_headers(Request* req, u32* i) {
    // Find as many headers as allowed
    while (req->headers_length < MAX_HEADERS) {
        // First check if \r\n\r\n
        // TODO: Check if next two bytes exist
        if (buffer[*i+1] == '\r' && buffer[*i+2] == '\n') {
            i += 2;
            break;
        }
        // Find key
        req->headers[req->headers_length]->key = &buffer[*i];
        while (buffer[*i] != '\0' && buffer[*i-1] != ':' && buffer[*i] != ' ') {
            i++;
        }
        if (buffer[*i-1] == ':' && buffer[*i] == ' ') {
            buffer[*i-1] = '\0';
            buffer[*i] = '\0';
        }
        STR_TO_LOWER(req->headers[req->headers_length]->key);
        // Find value
        req->headers[req->headers_length]->value = &buffer[*i];
        while (buffer[*i] != '\0' && buffer[*i-1] != '\r' && buffer[*i] != '\n') {
            i++;
        }
        if (buffer[*i-1] == '\r' && buffer[*i] == '\n') {
            buffer[*i-1] = '\0';
            buffer[*i] = '\0';
        }
        req->headers_length++;
        printf("Cursor after header: %d\n", *i);
    }
    printf("Cursor after all headers: %d\n", *i);
}

void _route_handle_client(Router* router, i32 client_fd) {
    // Parsed state
    Request req = {0};
    u64 total_read = 0;
    // This state describes what is currently being parsed
    // - 0 = Request line
    // - 1 = Request headers
    // - 2 = Request body
    // - 3 = Request completed
    u8 parse_state = 0;

    // Start reading from client
    for (;;) {
        i64 chunk_size = recv(client_fd, buffer, BUFFER_CAPACITY, 0);
        if (chunk_size < 0) {
            printf("Error reading from client\n");
            return;
        }
        total_read += chunk_size;

        u32 i = 0;
        if (parse_state == 0) {
            // Parsing the request line
            _route_handle_request_line(&req, &i);
            parse_state += 1;
        }
        if (parse_state == 1) {
            // Parsing the headers
            _route_handle_request_headers(&req, &i);
            parse_state += 1;
        }
        if (parse_state == 2) {
            // Parsing the body
            u64 content_length = 0;
            header_foreach(req, Header*, header) {
                // Get content-length header
                if (strcmp((*header)->key, "content-length") == 0) {
                    STR_TO_NUM((*header)->value, content_length);
                }
            }
            // Body size which is in chunk
            u64 body_have = chunk_size - i;
            // Body size which still needs to be read
            u64 body_need = content_length - total_read;
            printf(
                "Total Read: %ld\nChunk Size: %ld\n\nCursor: %d\nContent-Length: %ld\nBody have:%ld\nBody need:%ld\n",
                total_read, chunk_size, i, content_length, body_have, body_need
            );
            req.body = &buffer[i];
            parse_state += 1;
        }
        if (parse_state == 3) {
            // Parsing has stopped
            break;
        }
    }

    return;
}

void route_serve(Router* router, char* host, u16 port) {
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
    server_addr.sin_port = htons(port);

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
    printf("Router listening at %s:%d\n", host, port);

    // Accept connections in a loop
    while ((client_fd = accept(server_fd, (struct sockaddr*)&server_addr, &addr_len))) {
        printf("Accepting new connection...\n");
        _route_handle_client(router, client_fd);
        close(client_fd);
    }
    close(server_fd);
    return;
}
