# PLC-CORE-MDE

Open modular PLC based on ESP32 with web interface, IEC61131 FBD programming, RS485 Modbus expansion and cloud simulator support.

---

## Description

PLC-CORE-MDE is an open, modular and scalable programmable logic controller (PLC) based on ESP32, designed for education, automation, industrial control and experimental development.

The project aims to provide a flexible, low-cost and high-performance control platform that can be expanded using external modules connected through RS485 using Modbus communication.

The system includes an embedded web server that allows configuration, monitoring and programming directly from a web browser without the need for additional software.

The programming interface is based on graphical Function Block Diagram (FBD) inspired by IEC 61131.

---

## Online simulator

You can test the PLC-CORE-MDE interface online:

https://martinentraigas07-design.github.io/plc-core-mde/

The simulator runs the same web interface used by the embedded PLC and allows testing the editor, runtime and monitoring system directly in the browser.

This simulator is intended for education, development and testing.

---

## Main features

* ESP32 based core module
* Embedded web server
* Graphical programming using FBD (Function Block Diagram)
* IEC 61131 inspired runtime engine
* RS485 Modbus expansion bus
* Automatic module detection
* Digital inputs and outputs
* Analog inputs and outputs
* PWM outputs
* Timers, counters and memory blocks
* Modular hardware design
* DIN rail mounting format
* Educational oriented architecture
* Open modular platform
* Cloud simulator support (planned)
* Virtual PLC for training and testing

---

## Architecture

### CORE module

* Processing
* Runtime engine
* Embedded web server
* Communication manager
* Modbus master
* System configuration

### Expansion modules

* Digital input modules
* Digital output modules
* Analog input modules
* Analog output modules
* Mixed IO modules
* RS485 Modbus devices
* Future communication modules

---

## Future features

* Cloud PLC simulator
* USB communication mode
* Remote Modbus mode
* Gateway mode
* Ethernet / TCP mode
* Hybrid simulation + real PLC
* Multi-device support
* Remote monitoring
* Data logging
* Web visualization panels

---

## Project goals

This project is intended for:

* Technical education
* Industrial automation learning
* Experimental development
* Custom control systems
* Low-cost PLC applications
* Embedded systems training
* IEC61131 programming practice

The project is designed to be understandable, modifiable and expandable.

---

## Repository structure

```
firmware/    → ESP32 firmware and runtime
hardware/    → schematics and PCB
docs/        → manuals and technical documents
simulator/   → web simulator
web files    → embedded web server interface
```

---

## Author

Martín Entraigas
Project: PLC-CORE-MDE
Argentina

---

## License

PLC-CORE-MDE License v1.0

Copyright (c) 2026 Martín Entraigas / PLC-CORE-MDE, Argentina

Educational use allowed
Commercial use requires authorization

See LICENSE file for full terms.
