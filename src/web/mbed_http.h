#pragma once

#include <Arduino.h>
#include <WiFi.h>

/** Minimal HTTP/1.1 responder for Nicla Vision (WiFiServer, bez WebServer ESP32). */
class MbedHttpRequest {
public:
    String method;
    String path;
    String query;

    bool hasArg(const char* name) const;
    String arg(const char* name) const;
};

class MbedHttpConnection {
public:
    explicit MbedHttpConnection(WiFiClient client);

    bool readRequest(MbedHttpRequest& out, unsigned long timeoutMs = 2000);

    void send(int code, const char* contentType, const String& body);
    void send(int code, const char* contentType, const char* body);
    void sendPlain(int code, const String& body);

    void stop();

private:
    WiFiClient _client;
};

class MbedHttpServer {
public:
    explicit MbedHttpServer(uint16_t port);

    void begin();
    void handleClient();

    using Handler = void (*)(MbedHttpConnection& conn, const MbedHttpRequest& req);

    void on(const char* path, Handler handler);

private:
    WiFiServer _server;
    struct Route {
        const char* path;
        Handler handler;
    };
    static const int kMaxRoutes = 8;
    Route _routes[kMaxRoutes];
    int _routeCount = 0;
};
