/*
 PLC-CORE-MDE Project
 Copyright (c) 2026 Martín Entraigas / PLC-CORE-MDE
 Argentina

 Licensed under PLC-CORE-MDE License v1.0
 See LICENSE file for details
*/
#include "web_server.h"
#include <WebServer.h>
#include <SPIFFS.h>
#include "plc_control.h"


extern WebServer server;

void handleSave()
{
  if (server.method() != HTTP_POST)
  {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  File f = SPIFFS.open("/program.json", "w");

  if (!f)
  {
    server.send(500, "text/plain", "Error open file");
    return;
  }

  f.print(server.arg("plain"));

  f.close();

  server.send(200, "text/plain", "Saved OK");
}
