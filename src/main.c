
#include <strings.h>
#define NOB_IMPLEMENTATION
#include "../nob.h"

#include <time.h>
#include <sys/socket.h>
#include "http_parser.h"
#include "types.h"

// Dev  = 0
// Prod = 1
#define IS_PROD 0

#if IS_PROD
    #define SETHICUS "sethicus.com"
    #define COOPS    "coopspizzeria.com"
    #define CSIGNS   "commercialsigns.biz"
#else
    #define SETHICUS "127.0.0.1:8000"
    #define COOPS    "127.0.0.1:8000"
    #define CSIGNS   "127.0.0.1:8000"
#endif

// Date is in this format:
// "%a, %d %b %Y %T %Z"
// "Mon, 18 Jul 2016 16:06:00 GMT"
#define make_res(fd, content_type, text) \
    do { \
        time_t now = time(NULL); \
        struct tm* t = gmtime(&now); \
        char date[32]; \
        strftime(date, sizeof(date), "%a, %d %b %Y %T %Z", t); \
        char* msg = nob_temp_sprintf( \
            "HTTP/1.1 200 OK" \
            "Content-Length: %ld" \
            "Content-Type: "content_type \
            "Date: %s" \
            "" \
            "%s", \
            strlen(text), date, text \
        ); \
        send(fd, msg, strlen(msg), 0); \
    } while(0)

/*
 * Index page complete
 */
void index_page_complete(i32 fd) {
    make_res(fd, "text/plain", "Game on");
}

/*
 * Admin login page
 */
void admin_login_page_complete(i32 fd) {
    make_res(fd, "text/plain", "Game on");
}

/*
 * Admin login handler
 */
void admin_login_handler_header(i32 fd, const i8* n, const i8* v) {
    if (strcasecmp(n, "Content-Type") == 0 && strcasecmp(v, "application/json") == 0) {
        // Everythings fine
    } else {
        // TODO: Tell the parser to error out because this route
        //       requires content-type to be application/json
    }
}
void admin_login_handler_body(i32 fd, const i8* c, usize len) {
    // Parse as json
}
void admin_login_handler_complete(i32 fd) {

}

const HttpRoute ROUTE_TABLE[MAX_ROUTES] = {
    { "GET",    "/",                     SETHICUS, NULL, NULL, index_page_complete },
    { "GET",    "/static",               SETHICUS, NULL, NULL, NULL },

    { "POST",   "/admin/login",          SETHICUS, admin_login_handler_header, admin_login_handler_body, admin_login_handler_complete },
    { "GET",    "/admin/login",          SETHICUS, NULL, NULL, admin_login_page_complete },
    { "GET",    "/admin/dashboard",      SETHICUS, NULL, NULL, NULL },
    { "POST",   "/admin/reset-password", SETHICUS, NULL, NULL, NULL },
    { "GET",    "/admin/session",        SETHICUS, NULL, NULL, NULL },
    { "GET",    "/admin/admin",          SETHICUS, NULL, NULL, NULL },

    { "POST",   "/fs",                   SETHICUS, NULL, NULL, NULL },
    { "GET",    "/fs",                   SETHICUS, NULL, NULL, NULL },
    { "DELETE", "/fs",                   SETHICUS, NULL, NULL, NULL },

    { "GET",    "/",                     COOPS,    NULL, NULL, NULL },
    { "GET",    "/static",               COOPS,    NULL, NULL, NULL },
    { "GET",    "/subscriber",           COOPS,    NULL, NULL, NULL },
    { "GET",    "/link",                 COOPS,    NULL, NULL, NULL },
    { "GET",    "/message",              COOPS,    NULL, NULL, NULL },

    { "GET",    "/",                     CSIGNS,   NULL, NULL, NULL },
    { "GET",    "/static",               CSIGNS,   NULL, NULL, NULL },
    { "GET",    "/user",                 CSIGNS,   NULL, NULL, NULL },
    { "GET",    "/contact",              CSIGNS,   NULL, NULL, NULL },
    { "GET",    "/store",                CSIGNS,   NULL, NULL, NULL },
    { "GET",    "/cart",                 CSIGNS,   NULL, NULL, NULL },
    { "GET",    "/invoice",              CSIGNS,   NULL, NULL, NULL },
};

i32 main(i32 argc, char** argv) {
    router_serve();
    return 0;
}
