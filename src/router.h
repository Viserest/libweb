#ifndef ROUTER_H_
#define ROUTER_H_

#include <stdlib.h>

#include "types.h"

#ifndef BUFFER_CAPACITY
#define BUFFER_CAPACITY (8*1024)
#endif/*BUFFER_CAPACITY*/

#ifndef MAX_HEADERS
#define MAX_HEADERS (32)
#endif/*MAX_HEADERS*/

#ifndef DA_INIT_CAP
#define DA_INIT_CAP (256)
#endif/*DA_INIT_CAP*/

#define router_reserve(da, expected_capacity) \
    do { \
        if ((expected_capacity) > (da)->capacity) { \
            if ((da)->capacity == 0) { \
                (da)->capacity = DA_INIT_CAP; \
            } \
            while ((expected_capacity) > (da)->capacity) { \
                (da)->capacity *= 2; \
            } \
            (da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items)); \
            assert((da)->items != NULL && "Buy more RAM lol"); \
        } \
    } while (0)

// Append an item to a dynamic array
#define router_append(da, item) \
    do { \
        router_reserve((da), (da)->count + 1); \
        (da)->items[(da)->count++] = (item); \
    } while (0)

// Function over headers
#define header_foreach(da, Type, it) for (Type *it = (da).headers; it < (da).headers + (da).headers_length; ++it)

// Function over char
// #define TO_LOWER(c)       (unsigned char)(c | 0x20)
#define TO_LOWER(c)    (((c) >= 'A' && (c) <= 'Z') ? ((c) | 0x20) : (c))
#define TO_NUM(c)      ((c) - '0')
// #define IS_ALPHA(c)    (TO_LOWER(c) >= 'a' && TO_LOWER(c) <= 'z')
#define IS_ALPHA(c)    (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define IS_NUM(c)      ((c) >= '0' && (c) <= '9')
#define IS_ALPHANUM(c) (IS_ALPHA(c) || IS_NUM(c))
#define IS_HEX(c)      (IS_NUM(c) || (TO_LOWER(c) >= 'a' && TO_LOWER(c) <= 'f'))

// Function over null-terminated string
// In-place string lowercase converter (skips non-alphas like numbers)
#define STR_TO_LOWER(s) do { \
    for (char *p = (s); *p; p++) { \
        if (IS_ALPHA(*p)) { \
            *p = TO_LOWER(*p); \
        } \
    } \
} while(0)
// Convert null-terminated string to size_t
#define STR_TO_NUM(s, result) do { \
    for (; *s; s++) { \
        if (IS_NUM(*s)) { \
            result = (result * 10) + TO_NUM(*s); \
        } \
    } \
} while(0)


typedef enum {
    METHOD_POST,
    METHOD_GET,
    METHOD_HEAD,
    METHOD_OPTIONS,
    METHOD_PUT,
    METHOD_PATCH,
    METHOD_DELETE,
} Method;

typedef struct {
    char* key;
    char* value;
} Header;

typedef struct {
    Method method;
    char* uri;
    char* version;
    Header* headers[MAX_HEADERS];
    u16 headers_length;
    char* body;
} Request;

typedef enum {
    STATUSCODE_OK = 200,
    STATUSCODE_CREATED = 201,
    STATUSCODE_BAD_REQUEST = 400,
    STATUSCODE_UNAUTHORIZED = 401,
    STATUSCODE_FORBIDDEN = 403,
    STATUSCODE_NOT_FOUND = 404,
    STATUSCODE_INTERNAL_SERVER_ERROR = 500,
} StatusCode;

typedef struct {
    StatusCode status;
    Header* headers[MAX_HEADERS];
    u16 headers_length;
    char* body;
} Response;

typedef struct {
    char* path;
    Method method;
    void (*handler)(Request*, Response*);
} Route;

typedef struct {
    Route* items;
    u64 count;
    u64 capacity;
} Router;

#define route_post(router, path, handler)    _route_add(router, path, METHOD_POST, handler)
#define route_get(router, path, handler)     _route_add(router, path, METHOD_GET, handler)
#define route_head(router, path, handler)    _route_add(router, path, METHOD_HEAD, handler)
#define route_options(router, path, handler) _route_add(router, path, METHOD_OPTIONS, handler)
#define route_put(router, path, handler)     _route_add(router, path, METHOD_PUT, handler)
#define route_patch(router, path, handler)   _route_add(router, path, METHOD_PATCH, handler)
#define route_delete(router, path, handler)  _route_add(router, path, METHOD_DELETE, handler)

void _route_add(Router* router, char* path, Method method, void* handler);

void route_serve(Router* router, char* host, u16 port);

#endif/*ROUTER_H_*/
