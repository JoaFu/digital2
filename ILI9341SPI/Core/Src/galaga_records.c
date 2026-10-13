#include "galaga_records.h"
#include "fatfs_sd.h"
#include "ff.h"
#include "diskio.h"
#include <string.h>

// Solo se publican marcas confirmadas en SD; cualquier error las invalida.
static FATFS volumen;
static FIL archivo;
static uint32_t mejores[3], secuencia;
static uint8_t disponible, ranura;
volatile uint32_t galagaRecordError;
static const char *rutas[2]={"0:/GALAGA0.DAT","0:/GALAGA1.DAT"};

static uint32_t leer32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void poner32(uint8_t *p,uint32_t n) {
    for (unsigned i=0;i<4;++i) p[i]=(uint8_t)(n>>(8*i));
}
static uint32_t crc(const uint8_t *p,unsigned n) {
    uint32_t c=0xFFFFFFFFUL;
    while(n--) {
        c^=*p++;
        for(unsigned i=0;i<8;++i) c=(c>>1)^((c&1)?0xEDB88320UL:0);
    }
    return ~c;
}
static void deshabilitar(uint32_t motivo) {
    disponible=0; memset(mejores,0,sizeof(mejores));
    galagaRecordError=motivo;
    f_mount(0,"0:",0);
}
// 0: ausente, 1: valido, 2: formato/CRC invalido, -1: fallo de acceso.
static int cargar(unsigned slot,uint8_t datos[28]) {
    FRESULT r=f_open(&archivo,rutas[slot],FA_READ);
    if(r==FR_NO_FILE) return 0;
    if(r!=FR_OK) {galagaRecordError=(uint32_t)r;return -1;}
    UINT leidos=0;
    uint8_t tamano=f_size(&archivo)==28;
    r=f_read(&archivo,datos,28,&leidos);
    FRESULT cierre=f_close(&archivo);
    if(r!=FR_OK || cierre!=FR_OK) {
        galagaRecordError=(uint32_t)(r!=FR_OK?r:cierre);return -1;
    }
    if(!tamano || leidos!=28 || memcmp(datos,"GLG1",4) ||
       leer32(datos+4)!=1 || leer32(datos+24)!=crc(datos,24)) return 2;
    return 1;
}
void GR_Iniciar(void) {
    disponible=0; secuencia=0; ranura=1; galagaRecordError=0;
    memset(mejores,0,sizeof(mejores));
    SD_Operacion(4000);
    FRESULT r=f_mount(&volumen,"0:",1);
    if(r!=FR_OK) {deshabilitar(r);return;}
    uint8_t a[28]={0},b[28]={0};
    int va=cargar(0,a),vb=cargar(1,b);
    if(va<0 || vb<0) {deshabilitar(galagaRecordError);return;}
    if(va==1 || vb==1) {
        // Comparacion modular; una escritura avanza la secuencia en uno.
        uint32_t diferencia=leer32(b+8)-leer32(a+8);
        ranura=(vb==1 && (va!=1 || (diferencia!=0 && diferencia<0x80000000UL)))?1:0;
        const uint8_t *datos=ranura?b:a;
        secuencia=leer32(datos+8);
        for(unsigned i=0;i<3;++i) mejores[i]=leer32(datos+12+4*i);
    } else if(va==2 || vb==2) {
        // No sobrescribir automaticamente archivos corruptos sin copia valida.
        deshabilitar(100);return;
    }
    disponible=1;
}
void GR_Comprobar(void) {
    if(!disponible) return;
    SD_Operacion(1000);
    if(disk_ioctl(0,CTRL_SYNC,0)!=RES_OK) deshabilitar(FR_DISK_ERR);
}
uint8_t GR_Disponible(void) {return disponible;}
uint32_t GR_Mejor(uint8_t nivel) {
    return disponible && nivel>=1 && nivel<=3?mejores[nivel-1]:0;
}
uint8_t GR_GuardarVictoria(uint8_t nivel,uint32_t ms) {
    if(!disponible || nivel<1 || nivel>3 || !ms) return 0;
    GR_Comprobar();
    if(!disponible || (mejores[nivel-1] && ms>=mejores[nivel-1])) return 0;
    uint8_t datos[28]={0},comprobacion[28];
    memcpy(datos,"GLG1",4); poner32(datos+4,1); poner32(datos+8,secuencia+1);
    for(unsigned i=0;i<3;++i) poner32(datos+12+4*i,i==nivel-1?ms:mejores[i]);
    poner32(datos+24,crc(datos,24));
    unsigned destino=ranura^1U;
    SD_Operacion(4000);
    FRESULT r=f_open(&archivo,rutas[destino],FA_WRITE|FA_CREATE_ALWAYS);
    if(r!=FR_OK) {deshabilitar(r);return 0;}
    UINT escritos=0;
    r=f_write(&archivo,datos,sizeof(datos),&escritos);
    if(r==FR_OK && escritos!=sizeof(datos)) r=FR_DISK_ERR;
    if(r==FR_OK) r=f_sync(&archivo);
    FRESULT cierre=f_close(&archivo);
    if(r==FR_OK) r=cierre;
    if(r!=FR_OK) {deshabilitar(r);return 0;}
    // Remontar invalida el cache: la comprobacion vuelve a leer el medio.
    r=f_mount(&volumen,"0:",1);
    if(r!=FR_OK || cargar(destino,comprobacion)!=1 || memcmp(datos,comprobacion,28)) {
        deshabilitar(r!=FR_OK?(uint32_t)r:101);return 0;
    }
    mejores[nivel-1]=ms; secuencia++; ranura=(uint8_t)destino;
    return 1;
}
