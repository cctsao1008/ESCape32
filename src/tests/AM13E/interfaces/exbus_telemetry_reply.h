/* Original Rel17 src/io.c::exbusresp response formatting only.
 * Device identifier MUST come from board-qualified stable identity;
 * it is never inferred from an AM13E SDK part-number register.
 * This layer does not operate any half-duplex UART pin or wire.
 */
#pragma once
#include <stdint.h>
typedef uint8_t (*AM13E_ExbusCrc8)(const char *,int);
typedef uint16_t (*AM13E_ExbusCrc16)(const char *,int);
typedef struct {
    uint32_t device_id;
    int temp1;
    int temp2;
    int volt;
    int curr;
    int csum;
    int erpm;
    int telem_poles;
} AM13E_ExbusTelemetryValues;
typedef struct {
    uint8_t description_index;
} AM13E_ExbusTelemetryState;
/* Build 7 description packets (ESCape32, Temp1/2, Volt, Curr, Csum,
 * RPM), then 38B live-sensor data packets; both original Rel17
 * CRC/length calculations are provided by the original src/util.c.
 * On validation failure state is NOT advanced.
 */
unsigned am13e_exbus_telemetry_build(AM13E_ExbusTelemetryState *state,
                                    const AM13E_ExbusTelemetryValues *value,
                                    uint8_t request_id,uint8_t *out,
                                    unsigned capacity,AM13E_ExbusCrc8 crc8,
                                    AM13E_ExbusCrc16 crc16);
