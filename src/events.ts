import { z } from "zod";

export const eventTypes = [
  "SPEED_ALERT",
  "NEAR_LIMIT",
  "GPS_UPDATE",
  "GEOFENCE_OUTSIDE",
  "UNLOCKED",
  "LOCKED",
  "COMMAND_ACK",
  "CONFIG_UPDATED",
  "RESET",
  "HEARTBEAT",
] as const;

export const iotEventSchema = z.object({
  deviceId: z.string().trim().min(1).max(80),
  eventType: z.enum(eventTypes),
  blocked: z.boolean().default(false),
  message: z.string().trim().min(1).max(255),
  occurredAt: z.string().datetime().optional(),
  latitude: z.number().min(-90).max(90).optional(),
  longitude: z.number().min(-180).max(180).optional(),
  speedKmph: z.number().min(0).max(250).optional(),
  insideGeofence: z.boolean().optional(),
  lockState: z.string().trim().max(40).optional(),
});

export type IotEventPayload = z.infer<typeof iotEventSchema>;

export type StoredGatewayEvent = IotEventPayload & {
  occurredAt: string;
  receivedAt: string;
  forwardedToBackend: boolean;
};

export function normalizeEvent(input: unknown): IotEventPayload {
  const parsed = iotEventSchema.parse(input);
  return {
    ...parsed,
    occurredAt: parsed.occurredAt ?? new Date().toISOString(),
  };
}
