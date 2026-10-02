// Binary wire protocol shared with GlassSDK/examples/posture/posture.c.
// Keep both sides in sync if you change any layout or constant here.

export const SETTINGS_CHANNEL = 0x504f; // 'PO', phone -> glasses
export const EVENT_CHANNEL = 0x5045; // 'PE', glasses -> phone
export const PROTOCOL_VERSION = 1;

export const COMMAND_CONFIG = 1;
export const COMMAND_REQUEST_CALIBRATE = 2;
export const COMMAND_STOP = 3;

export const EVENT_CALIBRATED = 1;
export const EVENT_ALERT = 2;

export const MIN_THRESHOLD_DEG = 5;
export const MAX_THRESHOLD_DEG = 45;
export const DEFAULT_THRESHOLD_DEG = 15;

export const MIN_DURATION_SEC = 2;
export const MAX_DURATION_SEC = 30;
export const DEFAULT_DURATION_SEC = 5;

// [version, COMMAND_CONFIG, enabled(0/1), threshold_deg, duration_sec]
export function encodeConfig(config) {
  return new Uint8Array([
    PROTOCOL_VERSION,
    COMMAND_CONFIG,
    config.enabled ? 1 : 0,
    config.threshold,
    config.durationSec,
  ]);
}

// [version, COMMAND_REQUEST_CALIBRATE, 0, 0] (padded to the 5-byte header).
export function encodeRequestCalibrate() {
  return new Uint8Array([PROTOCOL_VERSION, COMMAND_REQUEST_CALIBRATE, 0, 0, 0]);
}

// [version, COMMAND_STOP, 0, 0] (padded to the 5-byte header).
export function encodeStop() {
  return new Uint8Array([PROTOCOL_VERSION, COMMAND_STOP, 0, 0, 0]);
}

// Decodes a glasses -> phone event. Returns null for anything unrecognized.
export function decodeEvent(data) {
  if (!data || data.length < 4 || data[0] !== PROTOCOL_VERSION) return null;
  const event = data[1];
  if (event === EVENT_CALIBRATED) {
    const baseline = (data[2] | (data[3] << 8)) << 16 >> 16; // signed 16-bit
    return { event, baseline };
  }
  if (event === EVENT_ALERT) {
    if (data.length < 6) return null;
    const count = data[2] | (data[3] << 8);
    const pitch = (data[4] | (data[5] << 8)) << 16 >> 16; // signed 16-bit
    return { event, count, pitch };
  }
  return null;
}
