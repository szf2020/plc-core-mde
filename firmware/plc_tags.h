#ifndef PLC_TAGS_H
#define PLC_TAGS_H

// =============================================================
// plc_tags.h — Sistema de Tags PLC-CORE-MDE v5
//
// Un "tag" es el nombre simbólico de una señal de IO.
// Equivalente a la "Tabla de Variables" de Siemens TIA Portal
// o la "Tag Database" de Allen-Bradley Studio 5000.
//
// Ejemplo de uso en ladder / FBD:
//   En vez de IN1, el usuario escribe: M1_START, SENSOR_NIVEL
//   En vez de OUT1: BOMBA_1, CONTACTOR_K1
//
// Los tags se persisten en SPIFFS /tags.json
// El editor los consulta en /tags/get y los usa como nombre del nodo
// =============================================================

#include <Arduino.h>

// Tipos de canal
typedef enum {
    TAG_DI = 0,   // Entrada digital local
    TAG_DO,       // Salida digital local
    TAG_AI,       // Entrada analógica local
    TAG_AO,       // Salida analógica local (futuro)
    TAG_RDI,      // Entrada digital remota (Modbus)
    TAG_RDO,      // Salida digital remota (Modbus)
    TAG_RAI,      // Entrada analógica remota
    TAG_RAO,      // Salida analógica remota
    TAG_MEM,      // Variable interna / memoria
} TagType;

#define TAG_NAME_LEN  24
#define TAG_COMMENT_LEN 48

struct Tag {
    TagType type;
    int     index;          // índice físico (0-based)
    char    name[TAG_NAME_LEN];    // ej: "M1_START"
    char    comment[TAG_COMMENT_LEN]; // ej: "Pulsador arranque motor 1"
    bool    invert;         // lógica negada
    bool    used;           // slot ocupado
};

// Límites
#define MAX_TAGS_DI   16
#define MAX_TAGS_DO   16
#define MAX_TAGS_AI    8
#define MAX_TAGS_RDI  64
#define MAX_TAGS_RDO  64
#define MAX_TAGS_RAI  32
#define MAX_TAGS_MEM  32

#define MAX_TAGS_TOTAL (MAX_TAGS_DI + MAX_TAGS_DO + MAX_TAGS_AI + \
                        MAX_TAGS_RDI + MAX_TAGS_RDO + MAX_TAGS_RAI + MAX_TAGS_MEM)

extern Tag tags[MAX_TAGS_TOTAL];
extern int tagCount;

// API pública
void    tagsInit();
void    tagsLoad();             // leer desde /tags.json en SPIFFS
void    tagsSave();             // guardar en /tags.json
Tag*    tagGet(TagType type, int index);
Tag*    tagGetByName(const char* name);
void    tagSet(TagType type, int index, const char* name, const char* comment = "");
void    tagsToJSON(char* buf, size_t len);
bool    tagsFromJSON(const char* json);

// Nombres por defecto si no hay tag definido
const char* tagDefaultName(TagType type, int index);

#endif
