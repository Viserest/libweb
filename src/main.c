
#define NOB_IMPLEMENTATION
#include "../nob.h"

#include <sys/socket.h>
#include "http_parser.h"
#include "types.h"

void index_complete(int fd) {
    const char *res = "HTTP/1.1 200 OK\r\nContent-Length: 8\r\n\r\nGame on\n";
    send(fd, res, strlen(res), 0);
}

void api_header(int fd, const char *n, const char *v) {
    printf("[API] Header: %s: %s\n", n, v);
}
void api_body(int fd, const char *c, size_t len) {
    printf("[API] Body chunk: %zu bytes\n", len);
}
void api_complete(int fd) {
    const char *res = "HTTP/1.1 200 OK\r\nContent-Length: 15\r\n\r\n{\"status\":\"ok\"}";
    send(fd, res, strlen(res), 0);
}

const HttpRoute ROUTE_TABLE[MAX_ROUTES] = {
    { "GET",  "/",    "127.0.0.1:8000", NULL,       NULL,     index_complete }, // Simple route
    { "POST",  "/api", "127.0.0.1:8000", api_header, api_body, api_complete   }
};

i32 main(i32 argc, char** argv) {
    router_serve();
    return 0;
}
