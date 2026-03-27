// =============================================================
// plc_io.cpp — PLC-CORE-MDE v3
// Fixes respecto a v2:
//   - plcStop() ya NO llama plcUnmarkPinAsPWM (se hace en plcReload)
//   - plcRegisterPWM: detach seguro antes de re-adjuntar
//   - pwmTable limpia en plcIOInit
//   - plcWriteOutput: protección correcta contra sobreescribir LEDC
//   - Separación clara: registrar ≠ marcar ≠ escribir
// =============================================================

#include "plc_io.h"
#include <Arduino.h>
#include "driver/ledc.h"

// -----------------------------------------------------------
// Mapa de pines físicos (índice → GPIO)
// -----------------------------------------------------------
static const int inputPins [NUM_INPUTS]   = { 21, 19 };
static const int outputPins[NUM_OUTPUTS]  = { 14, 27, 26, 25 };
static const int analogPins[NUM_ANALOG]   = { 36 };
static const int pinLedStatus = 5;
static const int pinLedWifi   = 2;

// -----------------------------------------------------------
// Tabla PWM — un canal LEDC por salida (índice = canal LEDC)
// -----------------------------------------------------------
#define PWM_FREQ_HZ    5000   // 5 kHz — adecuado para cargas inductivas/resistivas
#define PWM_RESOLUTION 12     // 12 bits → 0-4095 (igual que ADC: sin re-escalar)
#define PWM_MAX        4095

struct PwmSlot {
    bool configured; // ledcSetup+ledcAttachPin realizado
    bool active;     // marcado por runtime (bloquea digitalWrite)
};
static PwmSlot pwmTable[NUM_OUTPUTS];

// -----------------------------------------------------------
// plcIOInit — configura pines y limpia tabla PWM
// -----------------------------------------------------------
void plcIOInit()
{
    for (int i = 0; i < NUM_INPUTS; i++)
        pinMode(inputPins[i], INPUT);

    for (int i = 0; i < NUM_OUTPUTS; i++)
    {
        pinMode(outputPins[i], OUTPUT);
        digitalWrite(outputPins[i], LOW);
        pwmTable[i] = { false, false };
    }

    pinMode(pinLedStatus, OUTPUT);
    pinMode(pinLedWifi,   OUTPUT);
    digitalWrite(pinLedStatus, LOW);
    digitalWrite(pinLedWifi,   LOW);
}

// -----------------------------------------------------------
// plcRegisterPWM
// Configura el canal LEDC para outputPins[pinIndex].
// Llamada desde parseProgram() ANTES de plcScan().
// Si ya estaba configurado: detach seguro + re-attach limpio.
// -----------------------------------------------------------
void plcRegisterPWM(int pinIndex)
{
    if (pinIndex < 0 || pinIndex >= NUM_OUTPUTS) return;

    int gpio    = outputPins[pinIndex];
    uint8_t ch  = (uint8_t)pinIndex; // canal LEDC = índice de salida (0-3)

    if (pwmTable[pinIndex].configured)
        ledcDetachPin(gpio); // detach limpio antes de reconfigurar

    ledcSetup(ch, PWM_FREQ_HZ, PWM_RESOLUTION);
    ledcAttachPin(gpio, ch);
    ledcWrite(ch, 0); // arrancar en 0%

    pwmTable[pinIndex].configured = true;
    pwmTable[pinIndex].active     = false; // se activa con plcMarkPinAsPWM
}

// -----------------------------------------------------------
// plcMarkPinAsPWM / plcUnmarkPinAsPWM
// Controla si plcWriteOutput puede usar digitalWrite en ese pin.
// -----------------------------------------------------------
void plcMarkPinAsPWM(int pinIndex)
{
    if (pinIndex >= 0 && pinIndex < NUM_OUTPUTS)
        pwmTable[pinIndex].active = true;
}

void plcUnmarkPinAsPWM(int pinIndex)
{
    if (pinIndex >= 0 && pinIndex < NUM_OUTPUTS)
        pwmTable[pinIndex].active = false;
}

// -----------------------------------------------------------
// plcResetPWM — desmarca todos los pines PWM (en plcReload)
// NO hace detach: el canal LEDC queda para ser re-registrado.
// -----------------------------------------------------------
void plcResetAllPWM()
{
    for (int i = 0; i < NUM_OUTPUTS; i++)
    {
        if (pwmTable[i].configured)
        {
            ledcWrite(i, 0);           // poner a 0% antes de desregistrar
            ledcDetachPin(outputPins[i]);
            pwmTable[i].configured = false;
        }
        pwmTable[i].active = false;
        // Restaurar el pin como salida digital
        pinMode(outputPins[i], OUTPUT);
        digitalWrite(outputPins[i], LOW);
    }
}

// -----------------------------------------------------------
// plcWritePWM — escribe duty cycle (0-4095 = 0-100%)
// -----------------------------------------------------------
void plcWritePWM(int pinIndex, int value)
{
    if (pinIndex < 0 || pinIndex >= NUM_OUTPUTS) return;
    if (!pwmTable[pinIndex].configured) return;

    int duty = constrain(value, 0, PWM_MAX);
    ledcWrite((uint8_t)pinIndex, duty);
}

// -----------------------------------------------------------
// plcWriteOutput — escribe salida digital
// IGNORA el pin si está bajo control PWM activo.
// -----------------------------------------------------------
void plcWriteOutput(int n, bool state)
{
    if (n < 0 || n >= NUM_OUTPUTS) return;
    if (pwmTable[n].active) return; // protegido por LEDC

    digitalWrite(outputPins[n], state ? HIGH : LOW);
}

// -----------------------------------------------------------
// Lectura de entradas
// -----------------------------------------------------------
bool plcReadInput(int n)
{
    if (n < 0 || n >= NUM_INPUTS) return false;
    return digitalRead(inputPins[n]) == HIGH;
}

int plcReadAnalog(int n)
{
    if (n < 0 || n >= NUM_ANALOG) return 0;
    return analogRead(analogPins[n]);
}

// -----------------------------------------------------------
// LEDs de estado del sistema (independientes del programa PLC)
// -----------------------------------------------------------
void plcSetStatusLed(bool s) { digitalWrite(pinLedStatus, s ? HIGH : LOW); }
void plcSetWifiLed  (bool s) { digitalWrite(pinLedWifi,   s ? HIGH : LOW); }
