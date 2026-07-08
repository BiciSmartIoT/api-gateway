#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <TinyGPSPlus.h>

// ==================== CONFIGURACION WIFI / EDGE ====================
const char* WIFI_SSID = "Olimpo";
const char* WIFI_PASSWORD = "1234oscar";
const char* GATEWAY_BASE_URL = "https://api-gateway-production-c440.up.railway.app";
const char* IOT_DEVICE_KEY = "dev-device-key";
const char* DEVICE_ID = "esp32-demo-01";

const unsigned long HEARTBEAT_INTERVAL_MS = 30000;
const unsigned long GPS_EVENT_INTERVAL_MS = 7000;
const unsigned long CONFIG_POLL_INTERVAL_MS = 15000;
const unsigned long COMMAND_POLL_INTERVAL_MS = 5000;

// ==================== PINES ====================
#define LED_VERDE 2
#define LED_ROJO 4
#define SERVO_PIN 18
#define GPS_RX_PIN 19 // ESP32 RX: conectar al TX del GPS
#define GPS_TX_PIN 23 // ESP32 TX: conectar al RX del GPS

// ==================== ACTUADORES / GPS ====================
Servo bloqueoServo;
TinyGPSPlus gps;

bool bloqueado = false;
double geofenceLat = -12.0464;
double geofenceLon = -77.0428;
double geofenceRadiusMeters = 100.0;
double speedLimitKmph = 30.0;

unsigned long lastHeartbeatAt = 0;
unsigned long lastGpsEventAt = 0;
unsigned long lastConfigPollAt = 0;
unsigned long lastCommandPollAt = 0;

const int SERVO_LOCK_ANGLE = 20;
const int SERVO_UNLOCK_ANGLE = 100;

// ==================================================
void conectarWifi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  Serial.print("Conectando WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 15000)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("WiFi conectado. IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("No se pudo conectar a WiFi. El circuito seguira funcionando localmente.");
  }
}

// ==================================================
String httpRequest(const String& method, const String& url, const String& payload, int& statusCode)
{
  conectarWifi();
  statusCode = 0;

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("HTTP cancelado: WiFi desconectado");
    return "";
  }

  HTTPClient http;
  WiFiClient plainClient;
  WiFiClientSecure secureClient;

  if (url.startsWith("https://"))
  {
    secureClient.setInsecure();
    http.begin(secureClient, url);
  }
  else
  {
    http.begin(plainClient, url);
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", IOT_DEVICE_KEY);

  if (method == "GET")
  {
    statusCode = http.GET();
  }
  else
  {
    statusCode = http.POST(payload);
  }

  String response = "";
  if (statusCode > 0)
  {
    response = http.getString();
  }

  http.end();
  return response;
}

// ==================================================
String jsonStringValue(const String& json, const String& key)
{
  String pattern = "\"" + key + "\":\"";
  int start = json.indexOf(pattern);
  if (start < 0)
  {
    return "";
  }
  start += pattern.length();
  int end = json.indexOf("\"", start);
  if (end < 0)
  {
    return "";
  }
  return json.substring(start, end);
}

double jsonNumberValue(const String& json, const String& key, double fallback)
{
  String pattern = "\"" + key + "\":";
  int start = json.indexOf(pattern);
  if (start < 0)
  {
    return fallback;
  }

  start += pattern.length();
  int end = start;
  while (end < json.length())
  {
    char c = json.charAt(end);
    if (!isDigit(c) && c != '-' && c != '+' && c != '.')
    {
      break;
    }
    end++;
  }

  return json.substring(start, end).toDouble();
}

// ==================================================
bool gpsTieneFix()
{
  return gps.location.isValid() && gps.location.age() < 5000;
}

double distanciaZonaMeters()
{
  if (!gpsTieneFix())
  {
    return -1.0;
  }

  return TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), geofenceLat, geofenceLon);
}

double velocidadKmph()
{
  if (!gps.speed.isValid())
  {
    return 0.0;
  }
  return gps.speed.kmph();
}

bool dentroDeZona()
{
  double distancia = distanciaZonaMeters();
  return distancia < 0 || distancia <= geofenceRadiusMeters;
}

// ==================================================
void setEstadoVisual(bool alerta)
{
  digitalWrite(LED_ROJO, alerta ? HIGH : LOW);
  digitalWrite(LED_VERDE, alerta ? LOW : HIGH);
}

void moverServo(int angle)
{
  if (!bloqueoServo.attached())
  {
    bloqueoServo.attach(SERVO_PIN, 500, 2500);
  }
  bloqueoServo.write(angle);
  delay(500);
  bloqueoServo.detach();
}

void activarBloqueo(const char* reason)
{
  if (!bloqueado)
  {
    moverServo(SERVO_LOCK_ANGLE);
    bloqueado = true;
  }

  setEstadoVisual(true);
  Serial.print("BLOQUEADO: ");
  Serial.println(reason);
}

void desbloquear(const char* reason)
{
  moverServo(SERVO_UNLOCK_ANGLE);
  bloqueado = false;
  setEstadoVisual(false);
  Serial.print("DESBLOQUEADO: ");
  Serial.println(reason);
}

// ==================================================
void enviarEventoIoT(const char* eventType, const char* message)
{
  bool fix = gpsTieneFix();
  bool inside = dentroDeZona();
  double speed = velocidadKmph();

  String payload = "{";
  payload += "\"deviceId\":\"";
  payload += DEVICE_ID;
  payload += "\",\"eventType\":\"";
  payload += eventType;
  payload += "\",\"blocked\":";
  payload += bloqueado ? "true" : "false";
  payload += ",\"message\":\"";
  payload += message;
  payload += "\"";

  if (fix)
  {
    payload += ",\"latitude\":";
    payload += String(gps.location.lat(), 6);
    payload += ",\"longitude\":";
    payload += String(gps.location.lng(), 6);
  }

  payload += ",\"speedKmph\":";
  payload += String(speed, 2);
  payload += ",\"insideGeofence\":";
  payload += inside ? "true" : "false";
  payload += ",\"lockState\":\"";
  payload += bloqueado ? "LOCKED" : "UNLOCKED";
  payload += "\"}";

  int statusCode = 0;
  String url = String(GATEWAY_BASE_URL) + "/api/iot/events";
  String response = httpRequest("POST", url, payload, statusCode);

  Serial.print("POST ");
  Serial.print(eventType);
  Serial.print(" -> ");
  Serial.println(statusCode);

  if (response.length() > 0)
  {
    Serial.println(response);
  }
}

// ==================================================
void consultarConfiguracion()
{
  if (millis() - lastConfigPollAt < CONFIG_POLL_INTERVAL_MS)
  {
    return;
  }
  lastConfigPollAt = millis();

  int statusCode = 0;
  String url = String(GATEWAY_BASE_URL) + "/api/iot/devices/" + DEVICE_ID + "/config";
  String response = httpRequest("GET", url, "", statusCode);

  if (statusCode != 200)
  {
    Serial.print("Config no disponible -> ");
    Serial.println(statusCode);
    return;
  }

  speedLimitKmph = jsonNumberValue(response, "speedLimitKmph", speedLimitKmph);
  geofenceLat = jsonNumberValue(response, "geofenceCenterLat", geofenceLat);
  geofenceLon = jsonNumberValue(response, "geofenceCenterLon", geofenceLon);
  geofenceRadiusMeters = jsonNumberValue(response, "geofenceRadiusMeters", geofenceRadiusMeters);

  Serial.println("Config IoT actualizada:");
  Serial.print("Velocidad limite: ");
  Serial.println(speedLimitKmph);
  Serial.print("Zona: ");
  Serial.print(geofenceLat, 6);
  Serial.print(",");
  Serial.print(geofenceLon, 6);
  Serial.print(" radio=");
  Serial.println(geofenceRadiusMeters);
}

void confirmarComando(const String& commandId, const String& status, const String& message)
{
  String payload = "{\"status\":\"" + status + "\",\"message\":\"" + message + "\"}";
  String url = String(GATEWAY_BASE_URL) + "/api/iot/devices/" + DEVICE_ID + "/commands/" + commandId + "/ack";

  int statusCode = 0;
  httpRequest("POST", url, payload, statusCode);

  Serial.print("ACK comando ");
  Serial.print(commandId);
  Serial.print(" -> ");
  Serial.println(statusCode);
}

void consultarComandos()
{
  if (millis() - lastCommandPollAt < COMMAND_POLL_INTERVAL_MS)
  {
    return;
  }
  lastCommandPollAt = millis();

  int statusCode = 0;
  String url = String(GATEWAY_BASE_URL) + "/api/iot/devices/" + DEVICE_ID + "/commands/next";
  String response = httpRequest("GET", url, "", statusCode);

  if (statusCode == 204)
  {
    return;
  }

  if (statusCode != 200)
  {
    Serial.print("Comandos no disponibles -> ");
    Serial.println(statusCode);
    return;
  }

  String commandId = jsonStringValue(response, "commandId");
  String type = jsonStringValue(response, "type");
  String reason = jsonStringValue(response, "reason");

  Serial.print("Comando recibido: ");
  Serial.print(type);
  Serial.print(" - ");
  Serial.println(reason);

  if (type == "LOCK")
  {
    activarBloqueo("Comando remoto");
    enviarEventoIoT("LOCKED", "BICICLETA BLOQUEADA POR COMANDO");
    confirmarComando(commandId, "ACKED", "LOCK ejecutado");
  }
  else if (type == "UNLOCK")
  {
    desbloquear("Comando remoto");
    enviarEventoIoT("UNLOCKED", "BICICLETA DESBLOQUEADA POR COMANDO");
    confirmarComando(commandId, "ACKED", "UNLOCK ejecutado");
  }
  else if (type == "RESET")
  {
    desbloquear("Reset remoto");
    enviarEventoIoT("RESET", "SISTEMA REINICIADO");
    confirmarComando(commandId, "ACKED", "RESET ejecutado");
  }
  else if (type == "SET_CONFIG")
  {
    lastConfigPollAt = 0;
    consultarConfiguracion();
    enviarEventoIoT("CONFIG_UPDATED", "CONFIGURACION APLICADA");
    confirmarComando(commandId, "ACKED", "Configuracion aplicada");
  }
  else
  {
    confirmarComando(commandId, "REJECTED", "Comando no soportado");
  }
}

// ==================================================
void evaluarReglas()
{
  while (Serial2.available())
  {
    gps.encode(Serial2.read());
  }

  bool inside = dentroDeZona();
  double speed = velocidadKmph();
  bool overSpeed = speed > speedLimitKmph;

  if (!inside)
  {
    activarBloqueo("Fuera de geofence");
    enviarEventoIoT("GEOFENCE_OUTSIDE", "BICICLETA FUERA DEL AREA PERMITIDA");
  }
  else if (overSpeed)
  {
    setEstadoVisual(true);
    enviarEventoIoT("SPEED_ALERT", "VELOCIDAD NO PERMITIDA");
  }
  else if (!bloqueado)
  {
    setEstadoVisual(false);
  }

  if (millis() - lastGpsEventAt >= GPS_EVENT_INTERVAL_MS)
  {
    lastGpsEventAt = millis();
    enviarEventoIoT("GPS_UPDATE", gpsTieneFix() ? "GPS ACTUALIZADO" : "GPS SIN FIX");
  }
}

void enviarHeartbeat()
{
  if (millis() - lastHeartbeatAt < HEARTBEAT_INTERVAL_MS)
  {
    return;
  }

  lastHeartbeatAt = millis();
  enviarEventoIoT("HEARTBEAT", "ESP32 conectado al edge");
}

void comandosSerialLocales()
{
  if (!Serial.available())
  {
    return;
  }

  char c = Serial.read();
  switch (c)
  {
    case 'B':
    case 'b':
      activarBloqueo("Serial local");
      enviarEventoIoT("LOCKED", "BICICLETA BLOQUEADA POR SERIAL");
      break;

    case 'R':
    case 'r':
      desbloquear("Serial local");
      enviarEventoIoT("RESET", "SISTEMA REINICIADO POR SERIAL");
      break;

    case 'S':
    case 's':
      Serial.println("===== ESTADO =====");
      Serial.print("WiFi: ");
      Serial.println(WiFi.status() == WL_CONNECTED ? "OK" : "DESCONECTADO");
      Serial.print("GPS chars: ");
      Serial.println(gps.charsProcessed());
      Serial.print("GPS fix: ");
      Serial.println(gpsTieneFix() ? "SI" : "NO");
      Serial.print("Velocidad: ");
      Serial.println(velocidadKmph());
      Serial.print("Bloqueado: ");
      Serial.println(bloqueado ? "SI" : "NO");
      break;
  }
}

// ==================================================
void setup()
{
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);

  desbloquear("Inicio del sistema");

  Serial.println();
  Serial.println("=================================");
  Serial.println(" BiceSmartIoT - ESP32 Edge Client ");
  Serial.println("=================================");
  Serial.println("B = bloquear local");
  Serial.println("R = reset/desbloquear local");
  Serial.println("S = estado local");

  conectarWifi();
  consultarConfiguracion();
  enviarEventoIoT("HEARTBEAT", "ESP32 conectado al edge");
  lastHeartbeatAt = millis();
}

// ==================================================
void loop()
{
  comandosSerialLocales();
  consultarConfiguracion();
  consultarComandos();
  evaluarReglas();
  enviarHeartbeat();
}
