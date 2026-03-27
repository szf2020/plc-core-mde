#ifndef PLC_MODBUS_H
#define PLC_MODBUS_H

// =============================================================
// plc_modbus.h — PLC-CORE-MDE v4
// Soporte Modbus RTU Master con:
//   - Auto-discovery (escaneo de direcciones 1-247)
//   - Módulos Waveshare con mapas de registros exactos verificados
//   - Protocolo MDE propio (mismo sistema, registro 0x4000 = ID tipo)
//   - Módulos: RELAY-8, IO-8CH, AI-8CH, AO-8CH
// =============================================================

#include <Arduino.h>

// --- Pines RS485 ---
#define RS485_TX_PIN    17
#define RS485_RX_PIN    16
#define RS485_DE_PIN    4
#define RS485_UART_NUM  2

// --- Parámetros bus ---
#define MODBUS_BAUD         9600
#define MODBUS_TIMEOUT_MS   80
#define MODBUS_INTER_FRAME  4    // ms entre tramas (Modbus: 3.5 char = ~4ms a 9600)
#define MODBUS_POLL_MS      100  // intervalo entre polls sucesivos

// --- Discovery ---
#define MODBUS_DISC_START   1
#define MODBUS_DISC_END     10   // escanear 1..10 por defecto (ampliable)
#define MODBUS_DISC_TIMEOUT 40   // ms timeout corto para discovery

// --- Tamaño tablas IO remotas ---
#define MODBUS_MAX_RDI   64
#define MODBUS_MAX_RDO   64
#define MODBUS_MAX_RAI   32
#define MODBUS_MAX_RAO   32
#define MODBUS_MAX_SLAVES 16

// =============================================================
// Tipos de módulo
// Waveshare reales + MDE propios (mismo protocolo)
// =============================================================
typedef enum {
    MOD_UNKNOWN   = 0,

    // --- Waveshare ---
    // Relay-8: DO relay 8CH. FC01/05/0F. Coils 0x0000-0x0007.
    MOD_WS_RELAY8  = 1,

    // Relay-16: DO relay 16CH. FC01/05/0F. Coils 0x0000-0x000F.
    MOD_WS_RELAY16 = 2,

    // IO-8CH: 8DI + 8DO. DI=FC02 disc.input 0x0000-0x0007.
    //         DO=FC01/05/0F coils 0x0000-0x0007.
    MOD_WS_IO8     = 3,

    // AI-8CH: 8 entradas analógicas. FC04 input regs 0x0000-0x0007.
    //         Valor 0-4095 (12-bit ADC). Factor escala según modo.
    MOD_WS_AI8     = 4,

    // AO-8CH: 8 salidas analógicas. FC10 holding regs 0x0000-0x0007.
    //         Valor en uA (0-20000) o mV (0-10000) según versión.
    MOD_WS_AO8     = 5,

    // --- Módulos MDE propios ---
    // Mismo protocolo Modbus. Se identifican respondiendo a
    // FC03 reg 0x4000 con código de tipo MDE.
    MOD_MDE_DI16   = 0x10,  // 16 DI
    MOD_MDE_DO16   = 0x11,  // 16 DO
    MOD_MDE_AI8    = 0x12,  // 8 AI 16-bit
    MOD_MDE_AO8    = 0x13,  // 8 AO 16-bit
    MOD_MDE_ETH    = 0x20,  // Puente Ethernet
    MOD_MDE_4G     = 0x21,  // Módulo 4G/LTE
    MOD_MDE_LORA   = 0x22,  // Módulo LoRa
    MOD_MDE_MIXED  = 0x30,  // DI+DO+AI mixto
} ModuleType;

// =============================================================
// Descriptor de módulo esclavo
// =============================================================
typedef struct {
    uint8_t    address;     // dirección Modbus (1-247), 0=vacío
    ModuleType type;
    bool       online;
    bool       discovered;  // encontrado por auto-discovery

    // Offsets en las tablas globales rdi/rdo/rai/rao
    int        rdiOff;
    int        rdoOff;
    int        raiOff;
    int        raoOff;

    // Canales del módulo
    uint8_t    numDI;
    uint8_t    numDO;
    uint8_t    numAI;
    uint8_t    numAO;

    // Estadísticas
    uint32_t   pollCount;
    uint32_t   errorCount;
    char       label[16];   // nombre legible ("WS-IO8@01")
} ModbusSlave;

// =============================================================
// Tablas IO remotas — accesibles desde el runtime
// =============================================================
extern bool rdi[MODBUS_MAX_RDI];  // Remote Digital Inputs
extern bool rdo[MODBUS_MAX_RDO];  // Remote Digital Outputs
extern int  rai[MODBUS_MAX_RAI];  // Remote Analog Inputs  (0-65535)
extern int  rao[MODBUS_MAX_RAO];  // Remote Analog Outputs (0-65535)

extern ModbusSlave mbSlaves[MODBUS_MAX_SLAVES];
extern int mbSlaveCount;
extern bool mbDiscoveryDone;

// =============================================================
// API pública
// =============================================================
void modbusInit();
void modbusPoll();                          // llamar desde loop(), no bloqueante
void modbusStartDiscovery();               // lanzar escaneo (puede tomar ~1s)
void modbusRegisterSlave(uint8_t addr, ModuleType type, const char* label = nullptr);
bool modbusIsOnline(uint8_t addr);
void modbusGetStatus(char* buf, size_t len);

#endif
