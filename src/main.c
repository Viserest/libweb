#define NOB_IMPLEMENTATION
#include "../nob.h"

#include "router.h"

#define HOST "127.0.0.1"
#define PORT 8000

void handle_index(Request* req, Response* res) {
    return;
}

int main(int argc, char** argv) {
    Router router = {0};
    route_get(&router, "/", handle_index);
    route_serve(&router, HOST, PORT);
    return 0;
}
