# ESP32 BiceSmartIoT Edge Client

Sketch listo para Arduino IDE.

## Librerias Arduino

- `TinyGPSPlus`
- `ESP32Servo`

## Pines actuales

- D2: LED verde
- D4: LED rojo
- D18: servo
- D19: RX ESP32 conectado al TX del GPS
- D23: TX ESP32 conectado al RX del GPS

## Configuracion que debe coincidir con el gateway

En `esp32_bicesmartiot.ino`:

```cpp
const char* GATEWAY_BASE_URL = "https://api-gateway-production-c440.up.railway.app";
const char* IOT_DEVICE_KEY = "dev-device-key";
const char* DEVICE_ID = "esp32-demo-01";
```

En Railway, el `api-gateway` debe tener:

```text
IOT_DEVICE_KEY=dev-device-key
```

`DEVICE_ID` debe ser el mismo que se asigne al vehiculo desde el front o la app movil.
