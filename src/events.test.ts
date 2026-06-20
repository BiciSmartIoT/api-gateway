import assert from "node:assert/strict";
import test from "node:test";

import { normalizeEvent } from "./events";

test("normalizeEvent accepts a valid ESP32 event", () => {
  const event = normalizeEvent({
    deviceId: "esp32-demo-01",
    eventType: "LOCKED",
    blocked: true,
    message: "BICICLETA BLOQUEADA",
  });

  assert.equal(event.deviceId, "esp32-demo-01");
  assert.equal(event.eventType, "LOCKED");
  assert.equal(event.blocked, true);
  assert.ok(event.occurredAt);
});

test("normalizeEvent rejects unknown event types", () => {
  assert.throws(() =>
    normalizeEvent({
      deviceId: "esp32-demo-01",
      eventType: "UNKNOWN",
      blocked: false,
      message: "bad",
    })
  );
});
