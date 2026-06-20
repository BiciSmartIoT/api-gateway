import cors from "cors";
import express, { Request, Response } from "express";
import helmet from "helmet";
import morgan from "morgan";
import { ZodError } from "zod";

import { IotEventPayload, StoredGatewayEvent, normalizeEvent } from "./events";

const app = express();

const port = Number(process.env.PORT || 8080);
const backendBaseUrl = (process.env.BACKEND_BASE_URL || "http://localhost:8080").replace(/\/+$/, "");
const iotDeviceKey = process.env.IOT_DEVICE_KEY || "dev-device-key";
const gatewayInternalKey = process.env.GATEWAY_INTERNAL_KEY || "dev-gateway-key";
const corsOrigins = (process.env.CORS_ORIGINS || "http://localhost:3000")
  .split(",")
  .map((origin) => origin.trim())
  .filter(Boolean);

let latestEvent: StoredGatewayEvent | null = null;

app.use(helmet());
app.use(express.json({ limit: "32kb" }));
app.use(morgan("tiny"));
app.use(
  cors({
    origin(origin, callback) {
      if (!origin || corsOrigins.includes(origin)) {
        callback(null, true);
        return;
      }
      callback(new Error("CORS origin not allowed"));
    },
  })
);

app.get("/health", async (_req: Request, res: Response) => {
  const backendHealth = await checkBackendHealth();
  res.json({
    status: "UP",
    service: "bicesmartiot-api-gateway",
    backend: backendHealth,
    latestEvent,
  });
});

app.get("/api/iot/latest", (_req: Request, res: Response) => {
  if (!latestEvent) {
    res.status(204).send();
    return;
  }

  res.json(latestEvent);
});

app.post("/api/iot/events", async (req: Request, res: Response) => {
  const providedKey = req.header("X-Device-Key");
  if (!providedKey || providedKey !== iotDeviceKey) {
    res.status(401).json({ message: "Device key invalida" });
    return;
  }

  let event: IotEventPayload;
  try {
    event = normalizeEvent(req.body);
  } catch (error) {
    if (error instanceof ZodError) {
      res.status(400).json({ message: "Payload IoT invalido", issues: error.issues });
      return;
    }
    throw error;
  }

  const forwarded = await forwardToBackend(event);
  latestEvent = {
    ...event,
    occurredAt: event.occurredAt || new Date().toISOString(),
    receivedAt: new Date().toISOString(),
    forwardedToBackend: forwarded,
  };

  res.status(forwarded ? 202 : 502).json(latestEvent);
});

async function forwardToBackend(event: IotEventPayload): Promise<boolean> {
  try {
    const response = await fetch(`${backendBaseUrl}/api/iot/events`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "X-Gateway-Key": gatewayInternalKey,
      },
      body: JSON.stringify(event),
    });

    if (!response.ok) {
      const body = await response.text().catch(() => "");
      console.error(`Backend rejected IoT event: ${response.status} ${body}`);
      return false;
    }

    return true;
  } catch (error) {
    console.error("Backend unavailable while forwarding IoT event", error);
    return false;
  }
}

async function checkBackendHealth() {
  try {
    const response = await fetch(`${backendBaseUrl}/actuator/health`);
    return {
      reachable: response.ok,
      statusCode: response.status,
    };
  } catch {
    return {
      reachable: false,
      statusCode: null,
    };
  }
}

app.listen(port, "0.0.0.0", () => {
  console.log(`BiceSmartIoT API Gateway listening on port ${port}`);
});
