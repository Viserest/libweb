
#include "../../nob.h"
#include "http_parser.h"
#include <stdio.h>

i32 main(i32 argc, i8** argv) {
    const char* request = \
        "GET / HTTP/1.1"
        "Host: 127.0.0.1:8000"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7"
        "Accept-Encoding: gzip, deflate, br, zstd"
        "Accept-Language: en-US,en;q=0.9"
        "Pragma: no-cache"
        "Priority: u=0, i"
        "Cache-Control: no-cache";
    HttpParser parser;
    snprintf(parser.buffer, strlen(parser.buffer), request, 0);

    http_parse(&parser);
    return 0;
}
