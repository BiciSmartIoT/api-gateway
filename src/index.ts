import cors from "cors";
import express, { Request, Response } from "express";
import helmet from "helmet";
import morgan from "morgan";
import { ZodError } from "zod";

import { IotEventPayload, StoredGatewayEvent, normalizeEvent } from "./events";

const app = express();

const port = Number(process.env.PORT || 8081);
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

app.get("/api/iot/devices/:deviceId/config", async (req: Request, res: Response) => {
  if (!ensureDeviceKey(req, res)) {
    return;
  }

  const response = await forwardBackendJson(`/api/iot/devices/${encodeURIComponent(req.params.deviceId)}/config`);
  res.status(response.statusCode).json(response.body);
});

app.get("/api/iot/devices/:deviceId/commands/next", async (req: Request, res: Response) => {
  if (!ensureDeviceKey(req, res)) {
    return;
  }

  const response = await forwardBackendJson(
    `/api/iot/devices/${encodeURIComponent(req.params.deviceId)}/commands/next`
  );
  if (response.statusCode === 204) {
    res.status(204).send();
    return;
  }
  res.status(response.statusCode).json(response.body);
});

app.post("/api/iot/devices/:deviceId/commands/:commandId/ack", async (req: Request, res: Response) => {
  if (!ensureDeviceKey(req, res)) {
    return;
  }

  const response = await forwardBackendJson(
    `/api/iot/devices/${encodeURIComponent(req.params.deviceId)}/commands/${encodeURIComponent(
      req.params.commandId
    )}/ack`,
    "POST",
    req.body
  );
  res.status(response.statusCode).json(response.body);
});

app.post("/api/iot/events", async (req: Request, res: Response) => {
  if (!ensureDeviceKey(req, res)) {
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

function ensureDeviceKey(req: Request, res: Response): boolean {
  const providedKey = req.header("X-Device-Key");
  if (!providedKey || providedKey !== iotDeviceKey) {
    res.status(401).json({ message: "Device key invalida" });
    return false;
  }
  return true;
}

async function forwardBackendJson(path: string, method = "GET", body?: unknown) {
  try {
    const response = await fetch(`${backendBaseUrl}${path}`, {
      method,
      headers: {
        "Content-Type": "application/json",
        "X-Gateway-Key": gatewayInternalKey,
      },
      body: body === undefined ? undefined : JSON.stringify(body),
    });

    if (response.status === 204) {
      return { statusCode: 204, body: null };
    }

    const text = await response.text();
    const parsed = safeJson(text);
    return { statusCode: response.status, body: parsed };
  } catch (error) {
    console.error("Backend unavailable while proxying IoT request", error);
    return { statusCode: 502, body: { message: "Backend IoT no disponible" } };
  }
}

function safeJson(text: string) {
  if (!text) {
    return null;
  }
  try {
    return JSON.parse(text);
  } catch {
    return { message: text };
  }
}

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
