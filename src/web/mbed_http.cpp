#include "web/mbed_http.h"

#include <string.h>

bool MbedHttpRequest::hasArg(const char* name) const {
    if (query.length() == 0) {
        return false;
    }
    String key = String(name) + "=";
    return query.indexOf(key) >= 0;
}

String MbedHttpRequest::arg(const char* name) const {
    if (!hasArg(name)) {
        return String();
    }
    const String key = String(name) + "=";
    int start = query.indexOf(key);
    if (start < 0) {
        return String();
    }
    start += key.length();
    int end = query.indexOf('&', start);
    if (end < 0) {
        return query.substring(start);
    }
    return query.substring(start, end);
}

MbedHttpConnection::MbedHttpConnection(WiFiClient client) : _client(client) {}

bool MbedHttpConnection::readRequest(MbedHttpRequest& out, unsigned long timeoutMs) {
    if (!_client) {
        return false;
    }

    MbedHttpRequest parsed;
    String line;
    const unsigned long deadline = millis() + timeoutMs;
    while (_client.connected() && millis() < deadline) {
        while (_client.available()) {
            char c = (char)_client.read();
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                if (line.length() == 0) {
                    out = parsed;
                    return out.method.length() > 0;
                }

                if (parsed.method.length() == 0 && line.startsWith("GET ")) {
                    int sp = line.indexOf(' ', 4);
                    if (sp > 4) {
                        String target = line.substring(4, sp);
                        int q = target.indexOf('?');
                        if (q >= 0) {
                            parsed.path = target.substring(0, q);
                            parsed.query = target.substring(q + 1);
                        } else {
                            parsed.path = target;
                            parsed.query = "";
                        }
                        parsed.method = "GET";
                    }
                }
                line = "";
                continue;
            }
            line += c;
        }
        delay(1);
    }
    return false;
}

void MbedHttpConnection::send(int code, const char* contentType, const String& body) {
    if (!_client) {
        return;
    }
    _client.print(F("HTTP/1.1 "));
    _client.print(code);
    _client.println(F(" OK"));
    _client.println(F("Connection: close"));
    _client.println(F("Cache-Control: no-store"));
    _client.print(F("Content-Type: "));
    _client.println(contentType);
    _client.print(F("Content-Length: "));
    _client.println(body.length());
    _client.println();
    _client.print(body);
}

void MbedHttpConnection::send(int code, const char* contentType, const char* body) {
    send(code, contentType, String(body));
}

void MbedHttpConnection::sendPlain(int code, const String& body) {
    send(code, "text/plain", body);
}

void MbedHttpConnection::stop() {
    if (_client) {
        _client.stop();
    }
}

MbedHttpServer::MbedHttpServer(uint16_t port) : _server(port) {}

void MbedHttpServer::begin() {
    _server.begin();
}

void MbedHttpServer::on(const char* path, Handler handler) {
    if (_routeCount >= kMaxRoutes) {
        return;
    }
    _routes[_routeCount].path = path;
    _routes[_routeCount].handler = handler;
    _routeCount++;
}

void MbedHttpServer::handleClient() {
    WiFiClient client = _server.accept();
    if (!client) {
        return;
    }

    MbedHttpConnection conn(client);
    MbedHttpRequest req;
    if (!conn.readRequest(req)) {
        conn.stop();
        return;
    }

    for (int i = 0; i < _routeCount; i++) {
        if (req.path == _routes[i].path) {
            _routes[i].handler(conn, req);
            conn.stop();
            return;
        }
    }

    conn.send(404, "text/plain", "Not found");
    conn.stop();
}
