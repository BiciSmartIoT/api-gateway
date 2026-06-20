# BiceSmartIoT API Gateway

Gateway entre el circuito ESP32 y el backend Spring Boot.

## Variables de entorno

- `PORT`: puerto HTTP del gateway.
- `BACKEND_BASE_URL`: URL base del backend, sin `/api`.
- `IOT_DEVICE_KEY`: clave que debe enviar el ESP32 en `X-Device-Key`.
- `GATEWAY_INTERNAL_KEY`: clave compartida con backend para `X-Gateway-Key`.
- `CORS_ORIGINS`: origenes permitidos separados por coma.

## Comandos

```powershell
npm install
npm run build
npm test
npm start
```

## Prueba local

```powershell
curl.exe -X POST http://localhost:8080/api/iot/events `
  -H "Content-Type: application/json" `
  -H "X-Device-Key: change-this-device-key" `
  -d "{\"deviceId\":\"esp32-demo-01\",\"eventType\":\"HEARTBEAT\",\"blocked\":false,\"message\":\"ESP32 conectado\"}"
```
