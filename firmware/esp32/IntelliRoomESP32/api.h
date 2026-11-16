#pragma once

// This API is deliberately supplied as a header because it shares the ESP32
// controller's domain model. It is included exactly once by the main sketch.
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

struct AuthSession {
  bool active = false;
  String token;
  ControlSource source = ControlSource::Teacher;
  uint32_t expiresAt = 0;
};

inline AuthSession sessions[4];

inline String jsonResponse(JsonDocument& document) {
  String body;
  serializeJson(document, body);
  return body;
}

inline void sendJson(AsyncWebServerRequest* request, int status, JsonDocument& document) {
  request->send(status, "application/json", jsonResponse(document));
}

inline AuthSession* authenticatedSession(AsyncWebServerRequest* request) {
  if (!request->hasHeader("Cookie")) return nullptr;
  const String cookie = request->getHeader("Cookie")->value();
  const int start = cookie.indexOf("session=");
  if (start < 0) return nullptr;
  const int tokenStart = start + 8;
  int tokenEnd = cookie.indexOf(';', tokenStart);
  if (tokenEnd < 0) tokenEnd = cookie.length();
  const String token = cookie.substring(tokenStart, tokenEnd);
  for (auto& session : sessions) {
    if (session.active && session.token == token && int32_t(session.expiresAt - millis()) > 0) return &session;
  }
  return nullptr;
}

inline bool requireAuth(AsyncWebServerRequest* request, AuthSession*& session) {
  session = authenticatedSession(request);
  if (session) return true;
  StaticJsonDocument<96> doc;
  doc["error"] = "UNAUTHENTICATED";
  sendJson(request, 401, doc);
  return false;
}
