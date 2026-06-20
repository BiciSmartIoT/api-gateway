import { z } from "zod";

export const eventTypes = ["SPEED_ALERT", "NEAR_LIMIT", "LOCKED", "RESET", "HEARTBEAT"] as const;

export const iotEventSchema = z.object({
  deviceId: z.string().trim().min(1).max(80),
  eventType: z.enum(eventTypes),
  blocked: z.boolean().default(false),
  message: z.string().trim().min(1).max(255),
  occurredAt: z.string().datetime().optional(),
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
