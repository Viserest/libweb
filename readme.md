# LibWeb
This library provides two parsers (http_parser and json_parser) and also a router with provides a routing table.

Currently the system runs something like this:
1. Router accepts incoming requests (router_serve)
2. Router continuously reads from request piping into HttpParser's buffer (router_handle_client)
3. HttpParser parses through the request line and finds Method, URI and Http Version (http_parser)
4. HttpParser parses through the headers (runs the on_header callback once name and value are found) and should find the Host header (http_parser)
5. If/Once all three are found (Method, URI, and Host), Router matches the incoming request to a route (filter_by_char, and filter_finalize)
   Otherwise HttpParser runs the default callbacks but they won't point to a specific route's callbacks.
6. Optionally handle the headers as they are parsed. (on_header)
7. Optionally handle the body as it's streamed in. (on_body)
   Parsers are provided for request bodies containing: url encoded, json, or multipart form data.
8. Optionally send a response otherwise a default one will be sent (on_complete)
9. Done!

All parsers can be used with streamed data because it's placed into XParser.buffer, the same spot where you can place c-strings.

## Router
Handles routing table and incoming requests (Incoming requests are parsed by http_parser).

http_parser only calls global callbacks (on_header, on_body, on_complete)

If the routing table "matches" a route (Method, URI, and Host header equal the incoming request),
then that route is chosen and its callbacks are run instead.

Router:
- Method
- URI
- Host
- on_header[1]
- on_body[1]
- on_complete[1]

[1] These callbacks are owned by the route and are different than http_parser callbacks.

## Http Parser
Parses http request and runs 3 major callbacks:
1. on_header
2. on_body
3. on_complete

## Json Parser
Parses json data and runs a callback when a key/value pair is found.

Possible json values:
- Object '{}'
- Array '[]'
- String '"+9000"'
- Number '1337'
- Boolean 'true/false'
- Null 'null'

### Nesting
Keys/Value pairs which are nested will use dot notation (for the key) to identify them.

Example:
```json
{
  "A": {
    "B": {
      "C": "Some",
      "D": "Thing"
    }
  }
}
```

Becomes:
```json
"A.B.C": "Some"
"A.B.D": "Thing"
```

This is also works for arrays, too

Example:
```json
{
  "A": [
    {
      "B": "Some",
      "C": "Thing"
    },
    {
      "D": "Another",
      "E": "Thing"
    }
  ]
}
```

```json
"A[0].B": "Some"
"A[0].C": "Thing"
"A[1].D": "Another"
"A[1].E": "Thing"
```
