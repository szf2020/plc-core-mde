#include <Arduino.h>
#include "storage.h"
#include <SPIFFS.h>

String loadProgram()
{
    Serial.println("loadProgram called");

    File f = SPIFFS.open("/program.json", "r");

    if (!f)
    {
        Serial.println("No program");
        return "";
    }

    String data = f.readString();

    f.close();

    Serial.println("Program loaded:");
    Serial.println(data);

    return data;
}
