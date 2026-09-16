
#ifndef ROUTER_H_
#define ROUTER_H_

#include "types.h"

#ifndef MAX_ROUTES
#define MAX_ROUTES (32)
#endif

// Event callbacks executed natively during the active stream
typedef void (*HttpHeaderCallback)(i32 fd, const i8 *name, const i8 *value);
typedef void (*HttpBodyCallback)(i32 fd, const i8 *chunk, u64 len);
typedef void (*HttpCompleteCallback)(i32 fd);

typedef struct {
    const i8 *method;
    const i8 *uri;
    const i8 *host;
    HttpHeaderCallback   on_header;
    HttpBodyCallback     on_body;
    HttpCompleteCallback on_complete;
} HttpRoute;

extern const HttpRoute ROUTE_TABLE[MAX_ROUTES];

typedef struct {
    // Stores list of potentially matching routes.
    //
    // 0 = disqualified
    // 1 = still potentially matching
    u8 candidates[MAX_ROUTES];

    // Found dynamic route callbacks
    HttpHeaderCallback   matched_on_header;
    HttpBodyCallback     matched_on_body;
    HttpCompleteCallback matched_on_complete;
} Router;

void router_serve();

#endif // ROUTER_H_
