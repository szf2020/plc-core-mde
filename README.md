# PLC-CORE-MDE

**PLC modular abierto basado en ESP32 con programación FBD + Ladder, simulador web y arquitectura industrial escalable**

---

## 🚀 Descripción

**PLC-CORE-MDE** es un controlador lógico programable (PLC) modular, abierto y escalable, basado en ESP32, diseñado para:

* Automatización industrial
* Domótica
* Educación técnica
* Desarrollo experimental

El sistema permite programar lógica de control utilizando:

👉 **FBD (Function Block Diagram)**
👉 **Ladder (diagrama de contactos)**

Ambos basados en la norma **IEC 61131**, con conversión interna automática y consistente.

---

## 🧠 Diferencial del sistema

A diferencia de otros proyectos, PLC-CORE-MDE utiliza una arquitectura unificada basada en una:

### 🔄 Representación Intermedia (IR)

Esto permite coherencia total entre:

**FBD ↔ IR ↔ Ladder ↔ Runtime**

✔ Lo que ves en el simulador es lo mismo que ejecuta el hardware
✔ No hay diferencias entre lógica visual y ejecución real
✔ Permite múltiples lenguajes sobre un mismo núcleo

---

## 🌐 Simulador online

Podés probar el sistema completo directamente desde el navegador:

👉 https://martinentraigas07-design.github.io/plc-core-mde/

El simulador incluye:

* Editor FBD (Drawflow)
* Conversión a Ladder
* Runtime PLC completo en JavaScript
* Monitor en tiempo real
* Simulación de entradas/salidas
* Módulos virtuales RS485

💡 No requiere hardware

---

## ✨ Características principales

* ⚡ Núcleo basado en ESP32
* 🌐 Servidor web embebido (sin software externo)
* 🧩 Arquitectura modular RS485 / Modbus
* 🔄 Programación en **FBD y Ladder**
* 🧠 Runtime propio en C++ (PLC real)
* 🧪 Simulador web completo (mismo comportamiento que el hardware)
* 📡 Monitor en tiempo real
* 🔍 Sistema de debug integrado
* 🔌 Detección automática de módulos
* 📦 Expansión mediante módulos externos
* 🏭 Diseño para montaje en riel DIN
* 🎓 Orientado a educación técnica y uso profesional

---

## 🏗 Arquitectura del sistema

### 🔹 Módulo CORE (ESP32)

* Ejecución del programa PLC
* Runtime de control
* Servidor web embebido
* Gestión de comunicaciones
* Maestro Modbus RS485
* Configuración del sistema

---

### 🔹 Módulos de expansión

* Entradas digitales
* Salidas digitales
* Entradas analógicas
* Salidas analógicas
* Módulos mixtos
* Dispositivos Modbus RS485
* Futuros módulos de comunicación

---

## 🔄 Flujo de programación

El sistema trabaja con un flujo unificado:

**FBD (editor gráfico)**
→ **IR (representación interna)**
→ **Ladder (visualización industrial)**
→ **Runtime (ejecución real o simulada)**

Esto garantiza:

✔ Consistencia total
✔ Portabilidad entre simulador y hardware
✔ Base sólida para futuras expansiones (IEC 61131 completo)

---

## 🧑‍💻 Cómo empezar

### 🔹 Opción 1 — Simulador (recomendado)

Abrí directamente:

👉 https://martinentraigas07-design.github.io/plc-core-mde/

---

### 🔹 Opción 2 — PLC real

1. Grabar firmware en ESP32
2. Conectarse vía navegador
3. Programar desde la interfaz web

---

## 📂 Estructura del repositorio

```
firmware/              → Runtime PLC (ESP32)
firmware/data/         → Interfaz embebida
simulator/             → Simulador web completo
docs/                  → Documentación técnica
hardware/              → Esquemáticos y PCB
```

---

## 🎯 Objetivos del proyecto

PLC-CORE-MDE está diseñado para:

* Educación técnica en automatización
* Aprendizaje de lógica PLC (FBD / Ladder)
* Desarrollo de sistemas de control personalizados
* Proyectos de bajo costo
* Experimentación con arquitectura industrial
* Formación en IEC 61131

---

## 🔮 Futuro del proyecto

* Simulación híbrida (PLC real + virtual)
* Comunicación Ethernet / TCP
* Gateway industrial
* Monitoreo remoto
* Registro de datos (data logging)
* Paneles web de visualización
* Soporte multi-dispositivo

---

## 👤 Autor

**Martín Entraigas** - **MDE-ELECTRONICA**
Argentina

Proyecto enfocado en educación técnica, automatización y desarrollo abierto.

---

## 🔐 Licencia

**PLC-CORE-MDE License v1.0**

* ✔ Uso educativo permitido
* ⚠ Uso comercial requiere autorización
* ✔ Modificaciones permitidas bajo condiciones

Ver archivo `LICENSE` para más detalles.
