# PLC-CORE-MDE v3 — Ejemplos de prueba

Cada ejemplo indica qué nodos usar, cómo conectarlos y qué comportamiento esperado observar.

---

## TEST 1 — Paso directo (más básico)
**Objetivo:** verificar lectura de entrada y escritura de salida.

Nodos: IN1 → OUT1

Procedimiento:
1. Agregar nodo IN1
2. Agregar nodo OUT1
3. Conectar salida de IN1 a entrada de OUT1
4. Guardar → Reload
5. Activar señal en GPIO21

Esperado: OUT1 (GPIO14) sigue el estado de IN1 sin delay.

---

## TEST 2 — TON básico
**Objetivo:** verificar Timer On-Delay con 3 segundos.

Nodos: IN1 → TON(3000ms) → OUT1

Procedimiento:
1. IN1 → TON → OUT1
2. Configurar TON con time=3000
3. Activar IN1

Esperado:
- OUT1 apagada durante los primeros 3 segundos
- OUT1 enciende exactamente a los 3s
- Al soltar IN1: OUT1 apaga inmediatamente y timer se resetea

---

## TEST 3 — TOF (apagado retardado)
**Objetivo:** simular lámpara que se apaga 5 segundos después de soltar el pulsador.

Nodos: IN1 → TOF(5000ms) → OUT1

Procedimiento:
1. Activar IN1: OUT1 enciende inmediatamente
2. Soltar IN1: OUT1 permanece encendida 5s, luego apaga

Esperado: Q=1 al instante, Q=0 a los 5s del flanco de bajada.

---

## TEST 4 — TP (pulso temporizado)
**Objetivo:** generar pulso de 2 segundos exactos con cada flanco ascendente.

Nodos: IN1 → TP(2000ms) → OUT1

Procedimiento:
1. Pulsar IN1 brevemente (menos de 2s)
2. OUT1 debe mantenerse encendida exactamente 2s
3. Volver a pulsar: nuevo pulso de 2s

Esperado: OUT1 no re-dispara si IN1 sigue activa. Solo flanco ascendente inicia el pulso.

---

## TEST 5 — AND lógico
**Objetivo:** OUT1 activa solo si IN1 AND IN2 simultáneas.

Nodos: IN1 → AND → OUT1
       IN2 ↗

Procedimiento:
1. Solo IN1 activa: OUT1 apagada
2. Solo IN2 activa: OUT1 apagada
3. IN1 + IN2 activas: OUT1 enciende

---

## TEST 6 — OR lógico
**Objetivo:** OUT1 activa si IN1 OR IN2.

Nodos: IN1 → OR → OUT1
       IN2 ↗

Esperado: cualquiera de las dos entradas activa OUT1.

---

## TEST 7 — NOT (contacto normalmente cerrado)
**Objetivo:** OUT1 activa cuando IN1 NO está activa (NC).

Nodos: IN1 → NOT → OUT1

Esperado:
- IN1=0: OUT1=1
- IN1=1: OUT1=0

---

## TEST 8 — CMP con entrada analógica
**Objetivo:** encender OUT1 cuando AI1 supere 2000 (≈49% del rango ADC).

Nodos: AI1 → CMP(> 2000) → OUT1

Procedimiento:
1. Conectar potenciómetro a GPIO36
2. Girar debajo del umbral: OUT1 apagada
3. Girar sobre el umbral: OUT1 enciende

---

## TEST 9 — PWM controlado por potenciómetro
**Objetivo:** controlar brillo de LED/velocidad de motor con AI1.

Nodos: AI1 → PWM(OUT:1)

Procedimiento:
1. Conectar LED con resistencia a GPIO14
2. Girar potenciómetro: brillo varía proporcionalmente

Esperado: 0V entrada → 0% duty; 3.3V entrada → 100% duty. Sin saltos.

---

## TEST 10 — Secuencia: pulsador arranca motor con retardo
**Objetivo:** simular arranque estrella-triángulo simplificado.

Nodos: IN1 → TON(2000ms) → OUT1 (contactor principal)
       IN1 → OUT2 (señal "arrancando")

Procedimiento:
1. Pulsar IN1
2. OUT2 (piloto) enciende inmediatamente
3. OUT1 (contactor) enciende a los 2s
4. Soltar IN1: ambas apagan

---

## TEST 11 — Alarma por nivel analógico con latch
**Objetivo:** AI1 > umbral activa alarma que permanece hasta reset manual.

Nodos: AI1 → CMP(> 3000) → OR → OUT1 (alarma)
                              ↑
       IN2 (reset) → NOT ——→ AND (futuro: con SR flip-flop)

Nota: Con la v3 actual usar: CMP → OUT1 directamente para probar el umbral.
El latch completo se implementa con el nodo SR (próxima versión).

---

## TEST 12 — Parpadeo con TP encadenado (oscilador)
**Objetivo:** OUT1 parpadea cada 1 segundo automáticamente.

Nodos: TP(500ms) → OUT1
       TP salida → NOT → TP entrada (retroalimentación)

Nota: conectar salida del NOT a entrada del TP crea un oscilador.
Primero iniciar con IN1 momentáneo para disparar el primer ciclo.

---

## Checklist de validación post-flash

- [ ] TEST 1: respuesta directa IN→OUT sin lag perceptible
- [ ] TEST 2: TON 3s ±100ms de tolerancia
- [ ] TEST 3: TOF 5s ±100ms
- [ ] TEST 4: TP 2s ±100ms, no re-dispara
- [ ] TEST 5-7: lógica booleana correcta
- [ ] TEST 8: CMP responde al umbral sin rebote
- [ ] TEST 9: PWM suave sin glitches al recargar programa
- [ ] Monitor web: tabla de nodos actualiza en tiempo real
- [ ] LED STATUS: parpadeo 1Hz en RUN, 5Hz en STOP
- [ ] LED WIFI: encendido fijo cuando AP activo
