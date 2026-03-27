#include "plc_tags.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

Tag tags[MAX_TAGS_TOTAL];
int tagCount = 0;

static int tagSlot(TagType type, int index) {
    // Cada tipo tiene un rango de slots en el array global
    switch(type) {
        case TAG_DI:  return index;                               // 0..15
        case TAG_DO:  return MAX_TAGS_DI + index;                // 16..31
        case TAG_AI:  return MAX_TAGS_DI + MAX_TAGS_DO + index; // 32..39
        case TAG_RDI: return 40 + index;                         // 40..103
        case TAG_RDO: return 104 + index;                        // 104..167
        case TAG_RAI: return 168 + index;                        // 168..199
        case TAG_MEM: return 200 + index;                        // 200..231
        default: return -1;
    }
}

void tagsInit() {
    memset(tags, 0, sizeof(tags));
    tagCount = 0;
    // Inicializar con nombres por defecto
    const char* diNames[] = {"IN1","IN2","IN3","IN4","IN5","IN6","IN7","IN8",
                              "IN9","IN10","IN11","IN12","IN13","IN14","IN15","IN16"};
    const char* doNames[] = {"OUT1","OUT2","OUT3","OUT4","OUT5","OUT6","OUT7","OUT8",
                              "OUT9","OUT10","OUT11","OUT12","OUT13","OUT14","OUT15","OUT16"};
    const char* aiNames[] = {"AI1","AI2","AI3","AI4","AI5","AI6","AI7","AI8"};
    for (int i=0;i<MAX_TAGS_DI;i++) {
        int s=tagSlot(TAG_DI,i);
        tags[s].type=TAG_DI; tags[s].index=i; tags[s].used=true; tags[s].invert=false;
        tags[s].name[0]=0; tags[s].comment[0]=0;
        strncpy(tags[s].name,diNames[i],TAG_NAME_LEN-1);
    }
    for (int i=0;i<MAX_TAGS_DO;i++) {
        int s=tagSlot(TAG_DO,i);
        tags[s].type=TAG_DO; tags[s].index=i; tags[s].used=true; tags[s].invert=false;
        tags[s].name[0]=0; tags[s].comment[0]=0;
        strncpy(tags[s].name,doNames[i],TAG_NAME_LEN-1);
    }
    for (int i=0;i<MAX_TAGS_AI;i++) {
        int s=tagSlot(TAG_AI,i);
        tags[s].type=TAG_AI; tags[s].index=i; tags[s].used=true; tags[s].invert=false;
        tags[s].name[0]=0; tags[s].comment[0]=0;
        strncpy(tags[s].name,aiNames[i],TAG_NAME_LEN-1);
    }
}

Tag* tagGet(TagType type, int index) {
    int s=tagSlot(type,index);
    if (s<0||s>=(int)MAX_TAGS_TOTAL) return nullptr;
    return &tags[s];
}

Tag* tagGetByName(const char* name) {
    for (int i=0;i<MAX_TAGS_TOTAL;i++)
        if (tags[i].used && strcmp(tags[i].name,name)==0)
            return &tags[i];
    return nullptr;
}

void tagSet(TagType type, int index, const char* name, const char* comment) {
    int s=tagSlot(type,index);
    if (s<0) return;
    tags[s].type=type; tags[s].index=index; tags[s].used=true;
    strncpy(tags[s].name,name,TAG_NAME_LEN-1);
    if (comment) strncpy(tags[s].comment,comment,TAG_COMMENT_LEN-1);
}

const char* tagDefaultName(TagType type, int index) {
    static char buf[16];
    switch(type){
        case TAG_DI:  snprintf(buf,16,"IN%d",index+1); break;
        case TAG_DO:  snprintf(buf,16,"OUT%d",index+1); break;
        case TAG_AI:  snprintf(buf,16,"AI%d",index+1); break;
        case TAG_RDI: snprintf(buf,16,"RDI%d",index+1); break;
        case TAG_RDO: snprintf(buf,16,"RDO%d",index+1); break;
        case TAG_RAI: snprintf(buf,16,"RAI%d",index+1); break;
        case TAG_MEM: snprintf(buf,16,"M%d",index+1); break;
        default:      snprintf(buf,16,"SIG%d",index+1); break;
    }
    return buf;
}

void tagsToJSON(char* buf, size_t len) {
    size_t pos=0;
    pos+=snprintf(buf+pos,len-pos,"{\"tags\":[");
    bool first=true;
    const int typeMax[]={MAX_TAGS_DI,MAX_TAGS_DO,MAX_TAGS_AI,0,MAX_TAGS_RDI,MAX_TAGS_RDO,MAX_TAGS_RAI,0,MAX_TAGS_MEM};
    const char* typeStr[]={"DI","DO","AI","AO","RDI","RDO","RAI","RAO","MEM"};
    for (int t=0;t<=8;t++){
        if (typeMax[t]==0) continue;
        for (int i=0;i<typeMax[t];i++){
            Tag* tg=tagGet((TagType)t,i);
            if (!tg||!tg->used) continue;
            pos+=snprintf(buf+pos,len-pos,
                "%s{\"type\":\"%s\",\"index\":%d,\"name\":\"%s\",\"comment\":\"%s\",\"invert\":%s}",
                first?"":",",typeStr[t],i,tg->name,tg->comment,tg->invert?"true":"false");
            first=false;
        }
    }
    snprintf(buf+pos,len-pos,"]}");
}

bool tagsFromJSON(const char* json) {
    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc,json)!=DeserializationError::Ok) return false;
    JsonArray arr=doc["tags"].as<JsonArray>();
    const char* typeMap[]={"DI","DO","AI","AO","RDI","RDO","RAI","RAO","MEM"};
    for (JsonObject t : arr){
        const char* ts=t["type"]|"DI";
        int idx=t["index"]|0;
        TagType type=TAG_DI;
        for (int i=0;i<9;i++) if (strcmp(ts,typeMap[i])==0){type=(TagType)i;break;}
        tagSet(type,idx,t["name"]|tagDefaultName(type,idx),t["comment"]|"");
        Tag* tg=tagGet(type,idx);
        if (tg) tg->invert=t["invert"]|false;
    }
    return true;
}

void tagsLoad() {
    File f=SPIFFS.open("/tags.json","r");
    if (!f){Serial.println("[Tags] Sin archivo, usando defaults");return;}
    String s=f.readString(); f.close();
    if (!tagsFromJSON(s.c_str()))
        Serial.println("[Tags] Error parseando tags.json");
    else Serial.println("[Tags] Tags cargados");
}

void tagsSave() {
    char buf[4096];
    tagsToJSON(buf,sizeof(buf));
    File f=SPIFFS.open("/tags.json","w");
    if (!f){Serial.println("[Tags] Error guardando");return;}
    f.print(buf); f.close();
    Serial.println("[Tags] Guardado OK");
}
