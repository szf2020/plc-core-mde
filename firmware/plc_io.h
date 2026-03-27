#ifndef PLC_IO_H
#define PLC_IO_H

#define NUM_INPUTS   2
#define NUM_OUTPUTS  4
#define NUM_ANALOG   1

void plcIOInit();

// Entradas
bool plcReadInput (int n);
int  plcReadAnalog(int n);

// Salidas digitales (respeta pines PWM)
void plcWriteOutput(int n, bool state);

// PWM
void plcRegisterPWM  (int pinIndex);  // configura LEDC (llamar en parseProgram)
void plcWritePWM     (int pinIndex, int value); // value 0-4095
void plcMarkPinAsPWM  (int pinIndex); // bloquea digitalWrite en ese pin
void plcUnmarkPinAsPWM(int pinIndex); // libera el pin para salida digital
void plcResetAllPWM  ();              // desmarca y detach todos (usar en plcReload)

// LEDs sistema
void plcSetStatusLed(bool s);
void plcSetWifiLed  (bool s);

#endif
