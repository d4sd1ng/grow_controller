/**
 * HTTP oder HTTPS gegen die Telemetry-API (ESP32).
 * SERVER_USE_HTTPS / SERVER_TLS_INSECURE kommen aus device_secrets.h (Export) oder Fallback.
 */
#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

inline WiFiClientSecure &growTlsClient() {
  static WiFiClientSecure c;
  return c;
}

inline void growHttpTlsInitOnce() {
  static bool done = false;
  if (done || !SERVER_USE_HTTPS)
    return;
  done = true;
  if (SERVER_TLS_INSECURE)
    growTlsClient().setInsecure();
}

inline String growApiUrl(const char *path) {
  if (SERVER_USE_HTTPS) {
    String u = String("https://") + SERVER_HOST;
    if (SERVER_PORT != 443)
      u = u + ":" + String((unsigned)SERVER_PORT);
    u += path;
    return u;
  }
  return String("http://") + SERVER_HOST + ":" + String((unsigned)SERVER_PORT) + path;
}

inline String growApiUrl(const String &path) {
  if (SERVER_USE_HTTPS) {
    String u = String("https://") + SERVER_HOST;
    if (SERVER_PORT != 443)
      u = u + ":" + String((unsigned)SERVER_PORT);
    u += path;
    return u;
  }
  return String("http://") + SERVER_HOST + ":" + String((unsigned)SERVER_PORT) + path;
}

inline bool growHttpBegin(HTTPClient &http, const char *path) {
  String url = growApiUrl(path);
  if (SERVER_USE_HTTPS) {
    growHttpTlsInitOnce();
    return http.begin(growTlsClient(), url);
  }
  return http.begin(url);
}

inline bool growHttpBegin(HTTPClient &http, const String &path) {
  String url = growApiUrl(path);
  if (SERVER_USE_HTTPS) {
    growHttpTlsInitOnce();
    return http.begin(growTlsClient(), url);
  }
  return http.begin(url);
}
