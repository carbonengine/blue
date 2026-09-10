// Copyright © 2026 CCP ehf.

#pragma once
#ifndef BlueTelemetryZones_h
#define BlueTelemetryZones_h

#include <CcpTelemetry.h>

// The Telemetry category that all zones emitted by blue are tagged with. The category is registered
// with the Telemetry registry on first use and stays registered for the lifetime of the process. If
// it could not be registered, the returned category has an empty name and no zone tagged with it is
// ever captured.
const CcpTelemetryCategory& BlueTelemetryZoneCategory();

// Zones entered from Python cannot be scoped `TelemetryZone` locals, as `EnterZone` and `LeaveZone`
// are separate calls into blue. These keep such a zone alive in between, keyed by the Python frame
// that entered it. A frame can hold more than one zone, as `EnterZone` can be called repeatedly from
// the same frame, in which case the zones nest. Entering a zone while Telemetry is not started is a
// no-op, as is leaving or annotating a zone that was never entered.
// The zones are guarded by a mutex of their own, so all three are safe to call from any thread.
void BlueTelemetryEnterZone( const void* key, const char* name, const char* filename, uint32_t lineno );
void BlueTelemetryLeaveZone( const void* key );
void BlueTelemetryZoneAddText( const void* key, const char* text );

#endif // BlueTelemetryZones_h
