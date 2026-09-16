#!/bin/bash

echo -ne "POST /api HTTP/1.1\r
Host: 127.0.0.1:8000\r
User-Agent: Testing Simple Script\r
Content-Length: 4\r
\r
Data" | ncat 127.0.0.1 8000

echo ""
