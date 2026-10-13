#include "main.h"
#include "diskio.h"
#include "fatfs_sd.h"
#include <string.h>

extern SPI_HandleTypeDef hspi2;
static DSTATUS estado = STA_NOINIT;
static uint8_t bloques, fallo;
static uint32_t inicioOperacion, limiteOperacion;
volatile uint32_t galagaSDError;

void SD_Operacion(uint32_t limiteMs) {
    inicioOperacion = HAL_GetTick(); limiteOperacion = limiteMs;
}
static uint8_t vigente(void) {
    if (fallo) return 0;
    if ((uint32_t)(HAL_GetTick()-inicioOperacion) >= limiteOperacion) {
        fallo=1; galagaSDError=1; estado=STA_NOINIT; return 0;
    }
    return 1;
}
static uint8_t transferir(const uint8_t *tx, uint8_t *rx, uint16_t n) {
    if (!vigente()) return 0;
    if (HAL_SPI_TransmitReceive(&hspi2,(uint8_t *)tx,rx,n,20)!=HAL_OK) {
        fallo=1; galagaSDError=2; estado=STA_NOINIT; return 0;
    }
    return 1;
}
static uint8_t byte(uint8_t dato) {
    uint8_t recibido=0xFF;
    transferir(&dato,&recibido,1); return recibido;
}
static void soltar(void) {
    HAL_GPIO_WritePin(SD_CS_GPIO_Port,SD_CS_Pin,GPIO_PIN_SET);
    byte(0xFF);
}
static uint8_t esperar(uint32_t limite) {
    uint32_t inicio=HAL_GetTick();
    do {
        if (!vigente()) return 0;
        if (byte(0xFF)==0xFF && !fallo) return 1;
    } while ((uint32_t)(HAL_GetTick()-inicio)<limite);
    return 0;
}
static uint8_t comando(uint8_t cmd,uint32_t arg) {
    soltar();
    if (!vigente()) return 0xFF;
    HAL_GPIO_WritePin(SD_CS_GPIO_Port,SD_CS_Pin,GPIO_PIN_RESET);
    byte(0xFF);
    if (!esperar(500)) return 0xFF;
    uint8_t tx[6]={(uint8_t)(0x40U|cmd),(uint8_t)(arg>>24),
        (uint8_t)(arg>>16),(uint8_t)(arg>>8),(uint8_t)arg,
        cmd==0?0x95:cmd==8?0x87:0x01};
    uint8_t rx[6];
    if (!transferir(tx,rx,6)) return 0xFF;
    for (unsigned i=0;i<16 && vigente();++i) {
        uint8_t r=byte(0xFF);
        if (!(r&0x80)) return r;
    }
    return 0xFF;
}
static DRESULT error(void) {
    estado=STA_NOINIT;
    if (!galagaSDError) galagaSDError=3;
    soltar(); return RES_ERROR;
}
static uint8_t velocidad(uint32_t divisor) {
    hspi2.Init.BaudRatePrescaler=divisor;
    if (HAL_SPI_Init(&hspi2)!=HAL_OK) {
        fallo=1; galagaSDError=2; return 0;
    }
    return 1;
}
DSTATUS disk_status(BYTE unidad) {
    return unidad ? STA_NOINIT : estado;
}
DSTATUS disk_initialize(BYTE unidad) {
    if (unidad) return STA_NOINIT;
    estado=STA_NOINIT; bloques=0; fallo=0; galagaSDError=0;
    if (!vigente() || !velocidad(SPI_BAUDRATEPRESCALER_256)) return estado;
    HAL_GPIO_WritePin(SD_CS_GPIO_Port,SD_CS_Pin,GPIO_PIN_SET);
    HAL_Delay(10);
    for (unsigned i=0;i<10;++i) byte(0xFF);
    uint32_t inicio=HAL_GetTick();
    uint8_t respuesta;
    do { respuesta=comando(0,0); }
    while (respuesta!=1 && vigente() && (uint32_t)(HAL_GetTick()-inicio)<300);
    if (respuesta!=1 || comando(8,0x1AA)!=1) {error();return estado;}
    uint8_t r7[4];
    for (unsigned i=0;i<4;++i) r7[i]=byte(0xFF);
    // SD v2: verificar voltaje admitido y patron de respuesta.
    if (r7[2]!=1 || r7[3]!=0xAA) {error();return estado;}
    inicio=HAL_GetTick();
    do {
        respuesta=comando(55,0);
        if (respuesta>1) break;
        respuesta=comando(41,0x40000000UL);
        if (respuesta==0) break;
        if (respuesta!=1) break;
        HAL_Delay(1);
    } while (vigente() && (uint32_t)(HAL_GetTick()-inicio)<1500);
    if (respuesta!=0 || comando(58,0)!=0) {error();return estado;}
    uint8_t ocr[4];
    for (unsigned i=0;i<4;++i) ocr[i]=byte(0xFF);
    if (!(ocr[0]&0x80) || fallo) {error();return estado;}
    bloques=(ocr[0]&0x40)!=0;
    if (!bloques && comando(16,512)!=0) {error();return estado;}
    soltar();
    // APB1=42 MHz: arranque a 164 kHz y transferencias a 5.25 MHz.
    if (!velocidad(SPI_BAUDRATEPRESCALER_8) || !vigente()) return estado;
    estado=0; return estado;
}
static uint8_t recibir(uint8_t *destino,UINT cantidad) {
    uint32_t inicio=HAL_GetTick(); uint8_t token;
    do { token=byte(0xFF); }
    while (token==0xFF && vigente() && (uint32_t)(HAL_GetTick()-inicio)<300);
    if (token!=0xFE || !vigente()) return 0;
    uint8_t tx[32]; memset(tx,0xFF,sizeof(tx));
    while (cantidad) {
        UINT n=cantidad>sizeof(tx)?sizeof(tx):cantidad;
        if (!transferir(tx,destino,(uint16_t)n)) return 0;
        destino+=n; cantidad-=n;
    }
    byte(0xFF); byte(0xFF); return !fallo;
}
DRESULT disk_read(BYTE unidad,BYTE *destino,DWORD sector,UINT cantidad) {
    if (unidad || !destino || !cantidad) return RES_PARERR;
    if (estado&STA_NOINIT) return RES_NOTRDY;
    while (cantidad--) {
        if ((!bloques && sector>0x7FFFFFUL) ||
            comando(17,bloques?sector:sector*512UL)!=0 || !recibir(destino,512)) return error();
        soltar(); destino+=512; ++sector;
    }
    return RES_OK;
}
DRESULT disk_write(BYTE unidad,const BYTE *origen,DWORD sector,UINT cantidad) {
    if (unidad || !origen || !cantidad) return RES_PARERR;
    if (estado&STA_NOINIT) return RES_NOTRDY;
    uint8_t rx[32];
    while (cantidad--) {
        if ((!bloques && sector>0x7FFFFFUL) ||
            comando(24,bloques?sector:sector*512UL)!=0) return error();
        byte(0xFF); byte(0xFE);
        for (unsigned i=0;i<512;i+=sizeof(rx))
            if (!transferir(origen+i,rx,sizeof(rx))) return error();
        byte(0xFF); byte(0xFF);
        uint8_t respuesta=byte(0xFF);
        if ((respuesta&0x1F)!=0x05 || !esperar(500)) return error();
        // CMD13 confirma tambien errores internos de programacion.
        if (comando(13,0)!=0 || byte(0xFF)!=0 || fallo) return error();
        soltar(); origen+=512; ++sector;
    }
    return RES_OK;
}
DRESULT disk_ioctl(BYTE unidad,BYTE cmd,void *salida) {
    if (unidad) return RES_PARERR;
    if (estado&STA_NOINIT) return RES_NOTRDY;
    if (cmd==CTRL_SYNC) {
        if (comando(13,0)!=0 || byte(0xFF)!=0 || !vigente()) return error();
        soltar(); return RES_OK;
    }
    if (cmd==GET_SECTOR_SIZE && salida) {*(WORD *)salida=512;return RES_OK;}
    // No se habilitan formateo, TRIM ni calculo de capacidad.
    return RES_PARERR;
}
