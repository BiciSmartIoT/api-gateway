# BiceSmartIoT API Gateway

Gateway entre el circuito ESP32 y el backend Spring Boot.

El gateway actua como edge HTTP del prototipo:

- Valida la clave del dispositivo (`X-Device-Key`).
- Recibe eventos y telemetria del ESP32.
- Reenvia eventos al backend con `X-Gateway-Key`.
- Expone polling de configuracion y comandos para el ESP32.

## Variables de entorno

- `PORT`: puerto HTTP del gateway.
- `BACKEND_BASE_URL`: URL base del backend, sin `/api`.
- `IOT_DEVICE_KEY`: clave que debe enviar el ESP32 en `X-Device-Key`.
- `GATEWAY_INTERNAL_KEY`: clave compartida con backend para `X-Gateway-Key`.
- `CORS_ORIGINS`: origenes permitidos separados por coma.

Valores locales por defecto:

- Gateway: `http://localhost:8081`
- Backend: `http://localhost:8080`
- `IOT_DEVICE_KEY`: `dev-device-key`
- `GATEWAY_INTERNAL_KEY`: `dev-gateway-key`

## Comandos

```powershell
npm install
npm run build
npm test
npm start
```

## Firmware ESP32

El sketch del dispositivo esta versionado en:

```text
firmware/esp32_bicesmartiot/esp32_bicesmartiot.ino
```

Se sube con Arduino IDE instalando `TinyGPSPlus` y `ESP32Servo`.

## Prueba local

```powershell
curl.exe -X POST http://localhost:8081/api/iot/events `
  -H "Content-Type: application/json" `
  -H "X-Device-Key: dev-device-key" `
  -d "{\"deviceId\":\"esp32-demo-01\",\"eventType\":\"HEARTBEAT\",\"blocked\":false,\"message\":\"ESP32 conectado\",\"latitude\":-12.0464,\"longitude\":-77.0428,\"speedKmph\":0,\"insideGeofence\":true,\"lockState\":\"UNLOCKED\"}"
```

```powershell
curl.exe http://localhost:8081/api/iot/devices/esp32-demo-01/config `
  -H "X-Device-Key: dev-device-key"
```

```powershell
curl.exe http://localhost:8081/api/iot/devices/esp32-demo-01/commands/next `
  -H "X-Device-Key: dev-device-key"
```
