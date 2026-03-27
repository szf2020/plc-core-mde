// =============================================================
// plc_modbus.cpp — PLC-CORE-MDE v4
//
// Módulos Waveshare soportados (mapas verificados wiki oficial):
//  RELAY8/16  : FC01 read coils, FC05/0F write coils
//  IO8CH      : FC02 read DI, FC01/0F write DO
//  AI8CH      : FC04 read input registers (0-4095)
//  AO8CH      : FC10 write holding registers (0-20000 uA)
//
// Módulos MDE propios (mismo Modbus, identificación FC03@0x4000):
//  Responde 0x4D44 ('MD') en reg 0x4000 + tipo en reg 0x4001
// =============================================================

#include "plc_modbus.h"
#include <Arduino.h>

bool rdi[MODBUS_MAX_RDI] = {};
bool rdo[MODBUS_MAX_RDO] = {};
int  rai[MODBUS_MAX_RAI] = {};
int  rao[MODBUS_MAX_RAO] = {};

ModbusSlave mbSlaves[MODBUS_MAX_SLAVES] = {};
int  mbSlaveCount    = 0;
bool mbDiscoveryDone = false;

static int   pollIndex   = 0;
static unsigned long lastPoll = 0;
static bool  discoveryRunning = false;
static uint8_t discAddr = MODBUS_DISC_START;

// ---- CRC16 Modbus ----
static uint16_t crc16(const uint8_t* d, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i=0; i<len; i++) {
        crc ^= d[i];
        for (int b=0; b<8; b++)
            crc = (crc&1) ? (crc>>1)^0xA001 : (crc>>1);
    }
    return crc;
}
static void appendCRC(uint8_t* f, size_t len) {
    uint16_t c = crc16(f,len);
    f[len]=c&0xFF; f[len+1]=c>>8;
}
static bool checkCRC(const uint8_t* f, size_t len) {
    if (len<2) return false;
    uint16_t got = f[len-2]|((uint16_t)f[len-1]<<8);
    return got == crc16(f,len-2);
}

// ---- RS485 físico ----
static void rs485Flush() { while(Serial2.available()) Serial2.read(); }

static void rs485Send(const uint8_t* buf, size_t len) {
    rs485Flush();
    digitalWrite(RS485_DE_PIN, HIGH);
    delayMicroseconds(150);
    Serial2.write(buf, len);
    Serial2.flush();
    delayMicroseconds(150);
    digitalWrite(RS485_DE_PIN, LOW);
    delay(MODBUS_INTER_FRAME);
}

static int rs485Read(uint8_t* buf, size_t maxLen, uint32_t tms) {
    unsigned long t0 = millis();
    size_t idx = 0;
    while ((millis()-t0)<tms && idx<maxLen) {
        if (Serial2.available()) buf[idx++]=Serial2.read();
        else delayMicroseconds(300);
    }
    return (int)idx;
}

// ---- Primitivas Modbus ----
static bool mbReadBits(uint8_t addr, uint8_t fc, uint16_t reg,
                        uint16_t qty, bool* dest, uint32_t tms=MODBUS_TIMEOUT_MS) {
    uint8_t req[8];
    req[0]=addr; req[1]=fc;
    req[2]=reg>>8; req[3]=reg&0xFF;
    req[4]=qty>>8; req[5]=qty&0xFF;
    appendCRC(req,6);
    rs485Send(req,8);
    uint8_t resp[40];
    int byCnt=(qty+7)/8;
    int n=rs485Read(resp,3+byCnt+2,tms);
    if (n<3+byCnt+2||!checkCRC(resp,n)||resp[0]!=addr||resp[1]!=fc) return false;
    for (int i=0;i<(int)qty;i++) dest[i]=(resp[3+i/8]>>(i%8))&1;
    return true;
}

static bool mbReadRegs(uint8_t addr, uint8_t fc, uint16_t reg,
                        uint16_t qty, int* dest, uint32_t tms=MODBUS_TIMEOUT_MS) {
    uint8_t req[8];
    req[0]=addr; req[1]=fc;
    req[2]=reg>>8; req[3]=reg&0xFF;
    req[4]=qty>>8; req[5]=qty&0xFF;
    appendCRC(req,6);
    rs485Send(req,8);
    uint8_t resp[80];
    int exp=3+qty*2+2;
    int n=rs485Read(resp,exp,tms);
    if (n<exp||!checkCRC(resp,n)||resp[0]!=addr||resp[1]!=fc) return false;
    for (int i=0;i<(int)qty;i++) dest[i]=(resp[3+i*2]<<8)|resp[4+i*2];
    return true;
}

static bool mbWriteCoils(uint8_t addr, uint16_t startCoil,
                          uint16_t qty, const bool* vals) {
    int byCnt=(qty+7)/8;
    uint8_t req[24];
    req[0]=addr; req[1]=0x0F;
    req[2]=startCoil>>8; req[3]=startCoil&0xFF;
    req[4]=qty>>8; req[5]=qty&0xFF;
    req[6]=(uint8_t)byCnt;
    for (int b=0;b<byCnt;b++) req[7+b]=0;
    for (int i=0;i<(int)qty;i++)
        if(vals[i]) req[7+i/8]|=(1<<(i%8));
    appendCRC(req,7+byCnt);
    rs485Send(req,7+byCnt+2);
    uint8_t resp[8];
    int n=rs485Read(resp,8,MODBUS_TIMEOUT_MS);
    return (n>=8&&checkCRC(resp,n)&&resp[0]==addr&&resp[1]==0x0F);
}

static bool mbWriteRegs(uint8_t addr, uint16_t startReg,
                         uint16_t qty, const int* vals) {
    uint8_t req[64];
    req[0]=addr; req[1]=0x10;
    req[2]=startReg>>8; req[3]=startReg&0xFF;
    req[4]=qty>>8; req[5]=qty&0xFF;
    req[6]=(uint8_t)(qty*2);
    for (int i=0;i<(int)qty;i++){
        req[7+i*2]=(vals[i]>>8)&0xFF;
        req[7+i*2+1]=vals[i]&0xFF;
    }
    appendCRC(req,7+qty*2);
    rs485Send(req,7+qty*2+2);
    uint8_t resp[8];
    int n=rs485Read(resp,8,MODBUS_TIMEOUT_MS);
    return (n>=8&&checkCRC(resp,n)&&resp[0]==addr&&resp[1]==0x10);
}

// ---- Identificación automática ----
// Orden: MDE (0x4000) → WS_IO8 (FC02) → WS_RELAY (FC01) → WS_AI (FC04) → WS_AO (FC03)
static ModuleType probeModule(uint8_t addr) {
    int regs[2]={};
    // Probe MDE: FC03 reg 0x4000
    if (mbReadRegs(addr,0x03,0x4000,2,regs,MODBUS_DISC_TIMEOUT)) {
        if (regs[0]==0x4D44) {
            Serial.printf("[Modbus] MDE addr=%d tipo=0x%02X\n",addr,regs[1]);
            return (ModuleType)regs[1];
        }
    }
    bool bits[16]={};
    // Probe WS_IO8: FC02 discrete inputs
    if (mbReadBits(addr,0x02,0x0000,8,bits,MODBUS_DISC_TIMEOUT)) {
        Serial.printf("[Modbus] WS_IO8 en addr=%d\n",addr); return MOD_WS_IO8;
    }
    // Probe WS_RELAY8: FC01 coils
    if (mbReadBits(addr,0x01,0x0000,8,bits,MODBUS_DISC_TIMEOUT)) {
        Serial.printf("[Modbus] WS_RELAY8 en addr=%d\n",addr); return MOD_WS_RELAY8;
    }
    // Probe WS_AI8: FC04 input regs
    int ai[1]={};
    if (mbReadRegs(addr,0x04,0x0000,1,ai,MODBUS_DISC_TIMEOUT)) {
        Serial.printf("[Modbus] WS_AI8 en addr=%d\n",addr); return MOD_WS_AI8;
    }
    // Probe WS_AO8: FC03 holding regs
    if (mbReadRegs(addr,0x03,0x0000,1,ai,MODBUS_DISC_TIMEOUT)) {
        Serial.printf("[Modbus] WS_AO8 en addr=%d\n",addr); return MOD_WS_AO8;
    }
    return MOD_UNKNOWN;
}

// ---- Registro de módulo ----
void modbusRegisterSlave(uint8_t addr, ModuleType type, const char* label) {
    if (mbSlaveCount>=MODBUS_MAX_SLAVES) return;
    for (int i=0;i<mbSlaveCount;i++)
        if (mbSlaves[i].address==addr) return;

    int di=0,doo=0,ai=0,ao=0;
    for (int i=0;i<mbSlaveCount;i++){
        di+=mbSlaves[i].numDI; doo+=mbSlaves[i].numDO;
        ai+=mbSlaves[i].numAI; ao+=mbSlaves[i].numAO;
    }

    ModbusSlave& s=mbSlaves[mbSlaveCount];
    s.address=addr; s.type=type;
    s.online=false; s.discovered=false;
    s.rdiOff=di; s.rdoOff=doo; s.raiOff=ai; s.raoOff=ao;
    s.pollCount=0; s.errorCount=0;

    switch(type){
        case MOD_WS_RELAY8:  s.numDI=0;s.numDO=8;s.numAI=0;s.numAO=0;snprintf(s.label,16,"WS-RELAY8@%02d",addr);break;
        case MOD_WS_RELAY16: s.numDI=0;s.numDO=16;s.numAI=0;s.numAO=0;snprintf(s.label,16,"WS-RELAY16@%02d",addr);break;
        case MOD_WS_IO8:     s.numDI=8;s.numDO=8;s.numAI=0;s.numAO=0;snprintf(s.label,16,"WS-IO8@%02d",addr);break;
        case MOD_WS_AI8:     s.numDI=0;s.numDO=0;s.numAI=8;s.numAO=0;snprintf(s.label,16,"WS-AI8@%02d",addr);break;
        case MOD_WS_AO8:     s.numDI=0;s.numDO=0;s.numAI=0;s.numAO=8;snprintf(s.label,16,"WS-AO8@%02d",addr);break;
        case MOD_MDE_DI16:   s.numDI=16;s.numDO=0;s.numAI=0;s.numAO=0;snprintf(s.label,16,"MDE-DI16@%02d",addr);break;
        case MOD_MDE_DO16:   s.numDI=0;s.numDO=16;s.numAI=0;s.numAO=0;snprintf(s.label,16,"MDE-DO16@%02d",addr);break;
        case MOD_MDE_AI8:    s.numDI=0;s.numDO=0;s.numAI=8;s.numAO=0;snprintf(s.label,16,"MDE-AI8@%02d",addr);break;
        case MOD_MDE_AO8:    s.numDI=0;s.numDO=0;s.numAI=0;s.numAO=8;snprintf(s.label,16,"MDE-AO8@%02d",addr);break;
        case MOD_MDE_ETH:    s.numDI=0;s.numDO=0;s.numAI=0;s.numAO=0;snprintf(s.label,16,"MDE-ETH@%02d",addr);break;
        case MOD_MDE_4G:     s.numDI=0;s.numDO=0;s.numAI=0;s.numAO=0;snprintf(s.label,16,"MDE-4G@%02d",addr);break;
        case MOD_MDE_LORA:   s.numDI=0;s.numDO=0;s.numAI=0;s.numAO=0;snprintf(s.label,16,"MDE-LORA@%02d",addr);break;
        case MOD_MDE_MIXED:  s.numDI=8;s.numDO=8;s.numAI=4;s.numAO=4;snprintf(s.label,16,"MDE-MIX@%02d",addr);break;
        default:             s.numDI=0;s.numDO=0;s.numAI=0;s.numAO=0;snprintf(s.label,16,"UNKN@%02d",addr);break;
    }
    if (label) strncpy(s.label,label,15);
    mbSlaveCount++;
    Serial.printf("[Modbus] Registrado: %s DI=%d DO=%d AI=%d AO=%d\n",
                  s.label,s.numDI,s.numDO,s.numAI,s.numAO);
}

// ---- Poll de un esclavo ----
static bool pollSlave(ModbusSlave& s) {
    bool ok=true;
    switch(s.type){
        case MOD_WS_RELAY8:
        case MOD_WS_RELAY16:
            ok=mbWriteCoils(s.address,0x0000,s.numDO,&rdo[s.rdoOff]);
            break;
        case MOD_WS_IO8: {
            bool ok1=mbReadBits(s.address,0x02,0x0000,s.numDI,&rdi[s.rdiOff]);
            bool ok2=mbWriteCoils(s.address,0x0000,s.numDO,&rdo[s.rdoOff]);
            ok=ok1&&ok2; break;
        }
        case MOD_WS_AI8:
            ok=mbReadRegs(s.address,0x04,0x0000,s.numAI,&rai[s.raiOff]);
            break;
        case MOD_WS_AO8:
            ok=mbWriteRegs(s.address,0x0000,s.numAO,&rao[s.raoOff]);
            break;
        case MOD_MDE_DI16:
            ok=mbReadBits(s.address,0x02,0x0000,s.numDI,&rdi[s.rdiOff]);
            break;
        case MOD_MDE_DO16:
            ok=mbWriteCoils(s.address,0x0000,s.numDO,&rdo[s.rdoOff]);
            break;
        case MOD_MDE_AI8:
            ok=mbReadRegs(s.address,0x04,0x0000,s.numAI,&rai[s.raiOff]);
            break;
        case MOD_MDE_AO8:
            ok=mbWriteRegs(s.address,0x0000,s.numAO,&rao[s.raoOff]);
            break;
        case MOD_MDE_MIXED: {
            bool o1=mbReadBits(s.address,0x02,0x0000,s.numDI,&rdi[s.rdiOff]);
            bool o2=mbWriteCoils(s.address,0x0000,s.numDO,&rdo[s.rdoOff]);
            bool o3=mbReadRegs(s.address,0x04,0x0000,s.numAI,&rai[s.raiOff]);
            bool o4=mbWriteRegs(s.address,0x0000,s.numAO,&rao[s.raoOff]);
            ok=o1&&o2&&o3&&o4; break;
        }
        default: ok=false; break;
    }
    s.pollCount++;
    if (!ok) { s.errorCount++; Serial.printf("[Modbus] Error: %s\n",s.label); }
    s.online=ok;
    return ok;
}

// ---- Discovery: un paso por llamada ----
static void discoveryStep() {
    if (discAddr > MODBUS_DISC_END) {
        discoveryRunning=false; mbDiscoveryDone=true;
        Serial.printf("[Modbus] Discovery OK. %d módulos\n",mbSlaveCount);
        return;
    }
    bool known=false;
    for (int i=0;i<mbSlaveCount;i++)
        if (mbSlaves[i].address==discAddr){known=true;break;}
    if (!known) {
        ModuleType t=probeModule(discAddr);
        if (t!=MOD_UNKNOWN) {
            modbusRegisterSlave(discAddr,t);
            mbSlaves[mbSlaveCount-1].discovered=true;
        }
    }
    discAddr++;
}

// ---- API pública ----
void modbusInit() {
    pinMode(RS485_DE_PIN,OUTPUT);
    digitalWrite(RS485_DE_PIN,LOW);
    Serial2.begin(MODBUS_BAUD,SERIAL_8N1,RS485_RX_PIN,RS485_TX_PIN);
    Serial.println("[Modbus] Init 9600 8N1");
}

void modbusStartDiscovery() {
    discAddr=MODBUS_DISC_START;
    discoveryRunning=true; mbDiscoveryDone=false;
    Serial.printf("[Modbus] Discovery: scan addr %d..%d\n",MODBUS_DISC_START,MODBUS_DISC_END);
}

bool modbusIsOnline(uint8_t addr) {
    for (int i=0;i<mbSlaveCount;i++)
        if (mbSlaves[i].address==addr) return mbSlaves[i].online;
    return false;
}

void modbusPoll() {
    unsigned long now=millis();
    if (discoveryRunning) {
        if ((now-lastPoll)>=MODBUS_DISC_TIMEOUT) { lastPoll=now; discoveryStep(); }
        return;
    }
    if (mbSlaveCount==0) return;
    if ((now-lastPoll)<MODBUS_POLL_MS) return;
    lastPoll=now;
    if (pollIndex>=mbSlaveCount) pollIndex=0;
    pollSlave(mbSlaves[pollIndex++]);
}

void modbusGetStatus(char* buf, size_t len) {
    size_t pos=0;
    pos+=snprintf(buf+pos,len-pos,"{\"discovery\":%s,\"slaves\":[",
                  mbDiscoveryDone?"true":"false");
    for (int i=0;i<mbSlaveCount;i++) {
        ModbusSlave& s=mbSlaves[i];
        pos+=snprintf(buf+pos,len-pos,
            "%s{\"addr\":%d,\"label\":\"%s\",\"type\":%d,"
            "\"online\":%s,\"discovered\":%s,"
            "\"di\":%d,\"do\":%d,\"ai\":%d,\"ao\":%d,"
            "\"polls\":%lu,\"errors\":%lu}",
            i>0?",":"",s.address,s.label,(int)s.type,
            s.online?"true":"false",s.discovered?"true":"false",
            s.numDI,s.numDO,s.numAI,s.numAO,
            s.pollCount,s.errorCount);
    }
    snprintf(buf+pos,len-pos,"]}");
}
