# Network Tutorial

This guide covers usage of the HeimWatt Network Stack (`src/net/`).

## Simple HTTP Server

The `http_server` module handles TCP, HTTP parsing, and thread management.

```c
#include "net/http_server.h"
#include "net/json.h"
#include <string.h>

// Request handler
int handle_request(const http_request *req, http_response *resp, void *ctx) {
    // Route: GET /health
    if (strcmp(req->method, "GET") == 0 && strcmp(req->path, "/health") == 0) {
        http_response_set_json(resp, "{\"status\":\"ok\"}");
        return 0;
    }
    
    // Default: 404
    http_response_set_status(resp, 404);
    http_response_set_json(resp, "{\"error\":\"not found\"}");
    return 0;
}

int main(void) {
    http_server *srv;
    
    // Initialize on port 8080
    http_server_init(&srv, 8080);
    
    // Set global handler
    http_server_set_handler(srv, handle_request, NULL);
    
    // Run (blocks)
    http_server_run(srv);
    
    http_server_destroy(&srv);
    return 0;
}
```

## Raw TCP Communication

For low-level socket operations, use `tcp.h`.

```c
#include "net/tcp_server.h" // or tcp_client.h
#include <stdio.h>

int main(void) {
    tcp_socket *server;
    tcp_listen(&server, 9000, 10);
    
    while (1) {
        tcp_socket *client;
        tcp_accept(server, &client);
        
        char buf[4096];
        int n = tcp_recv(client, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("Received: %s\n", buf);
            tcp_send(client, "OK\n", 3);
        }
        
        tcp_close(&client);
    }
    
    tcp_close(&server);
    return 0;
}
```
