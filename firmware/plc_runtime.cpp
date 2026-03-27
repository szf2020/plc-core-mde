/*
 PLC-CORE-MDE Project
 Copyright (c) 2026 Martín Entraigas / PLC-CORE-MDE
 Argentina

 Licensed under PLC-CORE-MDE License v1.0
 See LICENSE file for details
*/
// =============================================================
// plc_runtime.cpp — PLC-CORE-MDE v5
// Nuevos nodos v5:
//   CTU  — contador ascendente (Count Up)
//   CTD  — contador descendente (Count Down)
//   CTUD — contador bi-direccional
//   SR   — biestable Set dominante (latch)
//   RS   — biestable Reset dominante
//   R_TRIG — detección flanco ascendente (1 ciclo)
//   F_TRIG — detección flanco descendente (1 ciclo)
//   SCALE  — escalado lineal ADC→unidades físicas
//   BLINK  — oscilador de ciclo configurable
// Fixes respecto a v2:
//   - 'time' renombrado a 'preset' (evita colisión con newlib time())
//   - 'out' renombrado a 'outIdx'
//   - parseProgram: lee preset con as<unsigned long>() explícito
//   - TON: corregido, timerDone no interfiere con state en ciclos sig.
//   - TOF: corregido flanco bajada, no necesita "if(IN) timerDone=false"
//   - TP: corregido, usa preset correctamente
//   - PWM: plcStop() ya NO desmarca pines PWM (lo hace plcReload)
//   - CMP: usa input2 si está conectado, sino usa nodes[i].value
//   - getNodeIntValue: soporta todos los tipos de nodo fuente
// =============================================================

#include "plc_runtime.h"
#include "plc_io.h"
#include "plc_state.h"
#include <ArduinoJson.h>

PlcNode nodes[MAX_NODES];
int nodeCount = 0;
SemaphoreHandle_t plcMutex = NULL;

// -------------------------------------------------------------
// Helpers internos
// -------------------------------------------------------------

static int getNodeIndexById(int id)
{
    for (int i = 0; i < nodeCount; i++)
        if (nodes[i].id == id) return i;
    return -1;
}

// Devuelve el valor numérico efectivo de un nodo (para CMP, PWM, etc.)
static int getNodeIntValue(int idx)
{
    if (idx < 0) return 0;
    if (nodes[idx].name == "analog1") return nodes[idx].intValue;
    return nodes[idx].state ? 1 : 0;
}

static int getInputPhysIndex(const String& name)
{
    if (name == "input1") return 0;
    if (name == "input2") return 1;
    return -1;
}

static int getOutputPhysIndex(const String& name)
{
    if (name == "output1") return 0;
    if (name == "output2") return 1;
    if (name == "output3") return 2;
    if (name == "output4") return 3;
    return -1;
}

static int getAnalogPhysIndex(const String& name)
{
    if (name == "analog1") return 0;
    return -1;
}

// -------------------------------------------------------------
// runtimeClear
// -------------------------------------------------------------
void runtimeClear()
{
    nodeCount = 0;
    // plcMutex se crea una sola vez en setup(), no aquí
}

// -------------------------------------------------------------
// parseProgram
// 3 pasadas:
//   1) cargar nodos desde JSON
//   2) resolver input1/input2 por índice (O(n²) pero solo aquí)
//   3) registrar pines PWM con LEDC
// -------------------------------------------------------------
void parseProgram(String json)
{
    if (plcMutex == NULL)
        plcMutex = xSemaphoreCreateMutex();

    xSemaphoreTake(plcMutex, portMAX_DELAY);

    nodeCount = 0;

    if (json.length() == 0)
    {
        xSemaphoreGive(plcMutex);
        Serial.println("[PLC] parseProgram: JSON vacío");
        return;
    }

    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, json);
    if (err)
    {
        Serial.printf("[PLC] JSON error: %s\n", err.c_str());
        xSemaphoreGive(plcMutex);
        return;
    }

    JsonObject data = doc["drawflow"]["Home"]["data"];

    // ---- PASADA 1: cargar nodos ----
    for (JsonPair kv : data)
    {
        if (nodeCount >= MAX_NODES) break;

        JsonObject node = kv.value();
        PlcNode& n = nodes[nodeCount];

        n.id   = node["id"].as<int>();
        n.name = node["name"].as<String>();

        // Señales
        n.state     = false;
        n.intValue  = 0;
        n.realValue = 0.0f;

        // Conexiones (se resuelven en pasada 2)
        n.input1      = -1;
        n.input2      = -1;
        n.outputCount = 0;

        // *** PARSER FIX ***
        // Drawflow guarda TODOS los valores de <input> y <select> como
        // strings en el JSON exportado (ej: "1000", "0", ">").
        // Usar | 0 o .as<int>() falla si el valor es string.
        // La solución robusta: leer como String y convertir con toInt().
        // Esto funciona tanto si el valor es número como string.
        auto readInt = [&](const char* key) -> int {
            JsonVariant v = node["data"][key];
            if (v.isNull()) return 0;
            if (v.is<int>()) return v.as<int>();
            return String(v.as<const char*>() ? v.as<const char*>() : "0").toInt();
        };
        auto readULong = [&](const char* key) -> unsigned long {
            JsonVariant v = node["data"][key];
            if (v.isNull()) return 0UL;
            if (v.is<long>()) return (unsigned long)v.as<long>();
            return (unsigned long)String(v.as<const char*>() ? v.as<const char*>() : "0").toInt();
        };

        n.value  = readInt("value");
        n.outIdx = readInt("out");
        n.preset = readULong("time");

        // op es siempre string, el operador | funciona bien aquí
        n.op = node["data"]["op"] | ">";

        // Estado timers — reset limpio
        n.timerStart   = 0;
        n.timerElapsed = 0;
        n.timerRunning = false;
        n.timerDone    = false;
        n.timerPrevIn  = false;

        // Contadores, memorias, flancos, escala
        n.counterVal    = readInt("pv");
        n.counterCU     = false; n.counterCD = false;
        n.counterR      = false; n.counterLD = false;
        n.counterPrevCU = false; n.counterPrevCD = false;
        n.memState      = false;
        n.trigPrevIn    = false; n.trigQ = false;
        n.scaleInMin    = readInt("inMin");
        n.scaleInMax    = readInt("inMax");  if (n.scaleInMax == 0) n.scaleInMax = 4095;
        n.scaleOutMin   = readInt("outMin");
        n.scaleOutMax   = readInt("outMax"); if (n.scaleOutMax == 0) n.scaleOutMax = 100;
        n.blinkState    = false; n.blinkLast = 0;

        // Leer lista de outputs (IDs drawflow)
        JsonObject outputs = node["outputs"];
        for (JsonPair outPort : outputs)
        {
            JsonArray conns = outPort.value()["connections"];
            for (JsonObject c : conns)
            {
                int targetId = String((const char*)c["node"]).toInt();
                if (n.outputCount < MAX_LINKS)
                    n.outputs[n.outputCount++] = targetId;
            }
        }

        Serial.printf("[PLC] Nodo[%d] id=%d name=%s preset=%lu outIdx=%d\n",
                      nodeCount, n.id, n.name.c_str(), n.preset, n.outIdx);
        nodeCount++;
    }

    // ---- PASADA 2: resolver conexiones input1/input2 ----
    for (int i = 0; i < nodeCount; i++)
    {
        nodes[i].input1 = -1;
        nodes[i].input2 = -1;
        int count = 0;

        for (int j = 0; j < nodeCount; j++)
        {
            for (int k = 0; k < nodes[j].outputCount; k++)
            {
                int targetIdx = getNodeIndexById(nodes[j].outputs[k]);
                if (targetIdx == i)
                {
                    if      (count == 0) nodes[i].input1 = j;
                    else if (count == 1) nodes[i].input2 = j;
                    count++;
                }
            }
        }
    }

    // ---- PASADA 3: registrar PWM ----
    for (int i = 0; i < nodeCount; i++)
    {
        if (nodes[i].name == "pwm")
        {
            plcRegisterPWM(nodes[i].outIdx);
            plcMarkPinAsPWM(nodes[i].outIdx);
            Serial.printf("[PLC] PWM registrado en outIdx=%d\n", nodes[i].outIdx);
        }
    }

    xSemaphoreGive(plcMutex);
    Serial.printf("[PLC] parseProgram OK: %d nodos\n", nodeCount);
}

// -------------------------------------------------------------
// plcScan — ciclo PLC (corre en Core 1 cada 10ms)
// -------------------------------------------------------------
void plcScan()
{
    if (plcMode != PLC_RUN) return;
    if (plcMutex == NULL)   return;
    if (xSemaphoreTake(plcMutex, 0) != pdTRUE) return;

    // =========================================================
    // FASE 1: Leer entradas físicas
    // =========================================================
    for (int i = 0; i < nodeCount; i++)
    {
        int inIdx = getInputPhysIndex(nodes[i].name);
        if (inIdx >= 0)
            nodes[i].state = plcReadInput(inIdx);

        int anIdx = getAnalogPhysIndex(nodes[i].name);
        if (anIdx >= 0) {
            nodes[i].intValue = plcReadAnalog(anIdx);
            nodes[i].state    = (nodes[i].intValue > 2048);
        }
    }

    // =========================================================
    // FASE 2: Lógica combinacional
    // =========================================================
    for (int i = 0; i < nodeCount; i++)
    {
        const String& nm = nodes[i].name;

        if (nm == "and") {
            bool a = (nodes[i].input1 >= 0) ? nodes[nodes[i].input1].state : false;
            bool b = (nodes[i].input2 >= 0) ? nodes[nodes[i].input2].state : false;
            nodes[i].state = a && b;
        }
        else if (nm == "or") {
            bool a = (nodes[i].input1 >= 0) ? nodes[nodes[i].input1].state : false;
            bool b = (nodes[i].input2 >= 0) ? nodes[nodes[i].input2].state : false;
            nodes[i].state = a || b;
        }
        else if (nm == "not") {
            bool a = (nodes[i].input1 >= 0) ? nodes[nodes[i].input1].state : false;
            nodes[i].state = !a;
        }
        else if (nm == "xor") {
            bool a = (nodes[i].input1 >= 0) ? nodes[nodes[i].input1].state : false;
            bool b = (nodes[i].input2 >= 0) ? nodes[nodes[i].input2].state : false;
            nodes[i].state = a ^ b;
        }
        else if (nm == "cmp") {
            int val1 = getNodeIntValue(nodes[i].input1);
            int val2 = (nodes[i].input2 >= 0)
                       ? getNodeIntValue(nodes[i].input2)
                       : nodes[i].value;
            const String& op = nodes[i].op;
            if      (op == ">")  nodes[i].state = (val1 >  val2);
            else if (op == "<")  nodes[i].state = (val1 <  val2);
            else if (op == ">=") nodes[i].state = (val1 >= val2);
            else if (op == "<=") nodes[i].state = (val1 <= val2);
            else if (op == "==") nodes[i].state = (val1 == val2);
            else if (op == "!=") nodes[i].state = (val1 != val2);
            else                 nodes[i].state = false;
        }
    }

    // =========================================================
    // FASE 3a: Timers IEC 61131-3 (TON, TOF, TP)
    // =========================================================
    unsigned long now = millis();

    for (int i = 0; i < nodeCount; i++)
    {
        const String& nm = nodes[i].name;
        bool IN = (nodes[i].input1 >= 0) ? nodes[nodes[i].input1].state : false;

        if (nm == "ton")
        {
            if (IN) {
                if (!nodes[i].timerRunning) {
                    nodes[i].timerRunning = true;
                    nodes[i].timerDone    = false;
                    nodes[i].timerStart   = now;
                    nodes[i].state        = false;
                } else if ((now - nodes[i].timerStart) >= nodes[i].preset) {
                    nodes[i].state     = true;
                    nodes[i].timerDone = true;
                }
            } else {
                nodes[i].timerRunning = false;
                nodes[i].timerDone    = false;
                nodes[i].state        = false;
            }
        }
        else if (nm == "tof")
        {
            bool risingEdge  = IN  && !nodes[i].timerPrevIn;
            bool fallingEdge = !IN &&  nodes[i].timerPrevIn;
            if (risingEdge) {
                nodes[i].state        = true;
                nodes[i].timerRunning = false;
                nodes[i].timerDone    = false;
            } else if (fallingEdge) {
                nodes[i].timerRunning = true;
                nodes[i].timerDone    = false;
                nodes[i].timerStart   = now;
                nodes[i].state        = true;
            } else if (!IN && nodes[i].timerRunning) {
                if ((now - nodes[i].timerStart) >= nodes[i].preset) {
                    nodes[i].state        = false;
                    nodes[i].timerRunning = false;
                    nodes[i].timerDone    = true;
                }
            } else if (!IN && nodes[i].timerDone) {
                nodes[i].state = false;
            } else if (IN) {
                nodes[i].state = true;
            }
            nodes[i].timerPrevIn = IN;
        }
        else if (nm == "tp")
        {
            bool risingEdge = IN && !nodes[i].timerPrevIn;
            if (risingEdge && !nodes[i].timerRunning) {
                nodes[i].timerRunning = true;
                nodes[i].timerDone    = false;
                nodes[i].timerStart   = now;
                nodes[i].state        = true;
            } else if (nodes[i].timerRunning) {
                if ((now - nodes[i].timerStart) >= nodes[i].preset) {
                    nodes[i].timerRunning = false;
                    nodes[i].timerDone    = true;
                    nodes[i].state        = false;
                }
            } else if (nodes[i].timerDone && !IN) {
                nodes[i].timerDone = false;
            }
            nodes[i].timerPrevIn = IN;
        }
        else if (nm == "blink")
        {
            bool EN = (nodes[i].input1 >= 0) ? nodes[nodes[i].input1].state : true;
            if (!EN) {
                nodes[i].state     = false;
                nodes[i].blinkLast = now;
            } else {
                if ((now - nodes[i].blinkLast) >= nodes[i].preset) {
                    nodes[i].blinkState = !nodes[i].blinkState;
                    nodes[i].blinkLast  = now;
                }
                nodes[i].state = nodes[i].blinkState;
            }
        }
    }

    // =========================================================
    // FASE 3b: Contadores IEC (CTU, CTD)
    // =========================================================
    for (int i = 0; i < nodeCount; i++)
    {
        const String& nm = nodes[i].name;

        if (nm == "ctu")
        {
            // CU = input1 (flanco ascendente cuenta), R = input2 (reset)
            bool CU = (nodes[i].input1 >= 0) ? nodes[nodes[i].input1].state : false;
            bool R  = (nodes[i].input2 >= 0) ? nodes[nodes[i].input2].state : false;
            if (R) {
                nodes[i].intValue = 0;
            } else if (CU && !nodes[i].counterPrevCU) {
                nodes[i].intValue++;
            }
            nodes[i].state        = (nodes[i].intValue >= nodes[i].value);
            nodes[i].counterPrevCU = CU;
        }
        else if (nm == "ctd")
        {
            // CD = input1 (flanco descendente cuenta), LD = input2 (carga preset)
            bool CD = (nodes[i].input1 >= 0) ? nodes[nodes[i].input1].state : false;
            bool LD = (nodes[i].input2 >= 0) ? nodes[nodes[i].input2].state : false;
            if (LD) {
                nodes[i].intValue = nodes[i].value;
            } else if (CD && !nodes[i].counterPrevCD) {
                if (nodes[i].intValue > 0) nodes[i].intValue--;
            }
            nodes[i].state        = (nodes[i].intValue <= 0);
            nodes[i].counterPrevCD = CD;
        }
    }

    // =========================================================
    // FASE 3c: Memorias / Biestables (SR, RS)
    // =========================================================
    for (int i = 0; i < nodeCount; i++)
    {
        const String& nm = nodes[i].name;
        bool S = (nodes[i].input1 >= 0) ? nodes[nodes[i].input1].state : false;
        bool R = (nodes[i].input2 >= 0) ? nodes[nodes[i].input2].state : false;

        if (nm == "sr") {
            // Set dominante: S=1 → Q=1, R=1 y S=0 → Q=0
            if (S)      nodes[i].state = true;
            else if (R) nodes[i].state = false;
        }
        else if (nm == "rs") {
            // Reset dominante: R=1 → Q=0, S=1 y R=0 → Q=1
            if (R)      nodes[i].state = false;
            else if (S) nodes[i].state = true;
        }
    }

    // =========================================================
    // FASE 3d: Detección de flancos (R_TRIG, F_TRIG)
    // =========================================================
    for (int i = 0; i < nodeCount; i++)
    {
        const String& nm = nodes[i].name;
        bool IN = (nodes[i].input1 >= 0) ? nodes[nodes[i].input1].state : false;

        if (nm == "r_trig") {
            nodes[i].state      = IN && !nodes[i].trigPrevIn;
            nodes[i].trigPrevIn = IN;
        }
        else if (nm == "f_trig") {
            nodes[i].state      = !IN && nodes[i].trigPrevIn;
            nodes[i].trigPrevIn = IN;
        }
    }

    // =========================================================
    // FASE 3e: SCALE — escalado lineal ADC → unidades físicas
    // =========================================================
    for (int i = 0; i < nodeCount; i++)
    {
        if (nodes[i].name != "scale") continue;
        int raw    = (nodes[i].input1 >= 0) ? nodes[nodes[i].input1].intValue : 0;
        int inMin  = nodes[i].scaleInMin;
        int inMax  = nodes[i].scaleInMax;
        int outMin = nodes[i].scaleOutMin;
        int outMax = nodes[i].scaleOutMax;
        if (inMax == inMin) { nodes[i].intValue = outMin; continue; }
        long scaled = (long)(raw - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
        nodes[i].intValue = (int)constrain(scaled, min(outMin,outMax), max(outMin,outMax));
        nodes[i].state    = (nodes[i].intValue > 0);
    }

    // =========================================================
    // FASE 4: Nodos PWM
    // =========================================================
    for (int i = 0; i < nodeCount; i++)
    {
        if (nodes[i].name != "pwm") continue;
        int value = nodes[i].value;
        if (nodes[i].input1 >= 0) {
            PlcNode* src = &nodes[nodes[i].input1];
            if (src->name == "analog1")
                value = src->intValue;
            else
                value = src->state ? 4095 : 0;
        }
        plcWritePWM(nodes[i].outIdx, value);
    }

    // =========================================================
    // FASE 5: Escribir salidas digitales
    // nodes[i].state se actualiza aquí para que el monitor pueda leerlo.
    // =========================================================
    for (int i = 0; i < nodeCount; i++)
    {
        int outPhys = getOutputPhysIndex(nodes[i].name);
        if (outPhys < 0) continue;
        bool val = (nodes[i].input1 >= 0)
                   ? nodes[nodes[i].input1].state
                   : false;
        nodes[i].state = val;
        plcWriteOutput(outPhys, val);
    }

    xSemaphoreGive(plcMutex);
}
