#include "galaga_pantalla.h"
#include "main.h"
#include "ili9341.h"
#include "galaga_recursos.h"
#include <string.h>
#include <stdio.h>

extern SPI_HandleTypeDef hspi1;
volatile uint32_t galagaDibujoMs, galagaBytesFrame, galagaFrameMaxMs;
/* 10 KB, no framebuffer de 150 KB. Una franja se compone y transmite de una vez. */
static uint16_t franja[G_ANCHO*16];
static uint32_t sucio[14];
static int rx,ry,rw,rh;
static uint32_t relojEstrellas;
static int hudBajas=-1,hudLento=-1,hudFase=-1,hudHP=-1;
static char hudAnterior[39];

static void enviar(const void *datos,unsigned n) {
    if(HAL_SPI_Transmit(&hspi1,(uint8_t *)datos,(uint16_t)n,100U)!=HAL_OK) {
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port,LCD_CS_Pin,GPIO_PIN_SET); Error_Handler();
    }
    galagaBytesFrame+=n;
}
static void comando(uint8_t c) {
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port,LCD_DC_Pin,GPIO_PIN_RESET); enviar(&c,1);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port,LCD_DC_Pin,GPIO_PIN_SET);
}
static void transmitir(int x,int y,int w,int h) {
    uint8_t limites[4];
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,LCD_CS_Pin,GPIO_PIN_RESET);
    comando(0x2A);
    limites[0]=(uint8_t)(x>>8); limites[1]=(uint8_t)x;
    limites[2]=(uint8_t)((x+w-1)>>8); limites[3]=(uint8_t)(x+w-1); enviar(limites,4);
    comando(0x2B);
    limites[0]=(uint8_t)(y>>8); limites[1]=(uint8_t)y;
    limites[2]=(uint8_t)((y+h-1)>>8); limites[3]=(uint8_t)(y+h-1); enviar(limites,4);
    comando(0x2C);
    for(int i=0;i<w*h;++i) franja[i]=(uint16_t)((franja[i]<<8)|(franja[i]>>8));
    enviar(franja,(unsigned)(w*h*2));
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,LCD_CS_Pin,GPIO_PIN_SET);
}
static void limpiar(void) {
    for(int y=0;y<G_ALTO;y+=16) {
        memset(franja,0,sizeof(franja)); transmitir(0,y,G_ANCHO,16);
    }
}
static void texto(const char *s,int x,int y,int grande,uint16_t color) {
    LCD_Print((char *)s,x,y,grande,color,0);
}
static void marcar(int x,int y,int w,int h) {
    int x2=x+w-1,y2=y+h-1;
    if(x2<0 || y2<G_HUD || x>=G_ANCHO || y>=G_ALTO) return;
    if(x<0)x=0;
    if(y<G_HUD)y=G_HUD;
    if(x2>=G_ANCHO)x2=G_ANCHO-1;
    if(y2>=G_ALTO)y2=G_ALTO-1;
    uint32_t mask=((1UL<<(x2/16+1))-1U)&~((1UL<<(x/16))-1U);
    for(int j=(y-G_HUD)/16;j<=(y2-G_HUD)/16;++j) sucio[j]|=mask;
}
static void pixel(int x,int y,uint16_t c) {
    if(x>=rx && x<rx+rw && y>=ry && y<ry+rh) franja[(y-ry)*rw+x-rx]=c;
}
static void rect(int x,int y,int w,int h,uint16_t c) {
    int x1=x>rx?x:rx,y1=y>ry?y:ry;
    int x2=x+w<rx+rw?x+w:rx+rw,y2=y+h<ry+rh?y+h:ry+rh;
    for(int j=y1;j<y2;++j) for(int i=x1;i<x2;++i) franja[(j-ry)*rw+i-rx]=c;
}
static void sprite(int x,int y,int tam,const uint16_t *datos, int tipo) {
    if(x+tam<=rx || x>=rx+rw || y+tam<=ry || y>=ry+rh) return;
    for(int j=0;j<tam;++j) {
        if(y+j<ry || y+j>=ry+rh) continue;
        for(int i=0;i<tam;++i) {
            if(x+i<rx || x+i>=rx+rw) continue;
            uint16_t c;
            if(datos) c=datos[j*tam+i];
            else {
                /* Recursos provisionales: silueta clasica recoloreada. */
                int k=(j*16/tam)*16+i*16/tam;
                c=enemigoPec[k];
                if(c) {
                    unsigned luz=((c>>11)&31U)+((c>>6)&31U)+(c&31U);
                    luz=10U+luz*21U/93U;
                    c=tipo==G_VERDE ? (uint16_t)((luz/3U)<<11 | (luz*2U)<<5 | luz/3U)
                      : tipo==G_AMARILLO ? (uint16_t)(luz<<11 | (luz*2U)<<5)
                      : (uint16_t)(luz<<11 | (luz/2U)<<5 | luz);
                }
            }
            if(c) pixel(x+i,y+j,c); /* Negro transparente: preserva fondo y rayo. */
        }
    }
}
static const uint16_t *imagen(unsigned tipo) {
#if GALAGA_SPRITES_NUEVOS
    if(tipo==G_AMARILLO) return enemigo_amarillo;
    if(tipo==G_VERDE) return enemigo_verde;
    if(tipo==G_BOSS) return enemigo_boss;
#endif
    return tipo==G_CLASICO || tipo==G_CRUCE ? enemigoPec : 0;
}
void GP_Iniciar(void) {
    for(int i=0;i<256;++i) {
        nave[i]=(uint16_t)((naveBytes[2*i]<<8)|naveBytes[2*i+1]);
        enemigoPec[i]=(uint16_t)((enemigoSmllBytes[2*i]<<8)|enemigoSmllBytes[2*i+1]);
        bala[i]=(uint16_t)((balaBytes[2*i]<<8)|balaBytes[2*i+1]);
    }
}
void GP_Portada(void) {
    for(int y=0;y<G_ALTO;y+=16) {
        memcpy(franja,pantallaInicio+y*G_ANCHO,sizeof(franja)); transmitir(0,y,G_ANCHO,16);
    }
    texto("B1: ELEGIR NIVEL",96,220,1,0xFFFF);
}
void GP_Menu(uint8_t seleccionado) {
    limpiar(); texto("ELIGE NIVEL",72,26,2,0x07FF);
    const char *opciones[]={"1  PRIMER CONTACTO","2  ESCUADRONES","3  EL JEFE"};
    for(unsigned i=0;i<3;++i) {
        int y=80+(int)i*36;
        texto(i+1U==seleccionado ? ">" : " ",32,y,1,0xFFE0);
        texto(opciones[i],56,y,1,i+1U==seleccionado?0xFFE0:0xBDF7);
    }
    texto("IZQ/DER: CAMBIAR  B1: JUGAR",48,208,1,0xFFFF);
}
void GP_Resultado(uint8_t victoria) {
    limpiar(); texto(victoria?"YOU WIN":"GAME OVER",victoria?104:88,92,2,victoria?0x07E0:0xF800);
    texto("VOLVIENDO A ELEGIR NIVEL",64,144,1,0xFFFF);
}
void GP_PrepararJuego(void) {
    limpiar();
    for(unsigned i=0;i<14;++i) sucio[i]=0xFFFFFU;
    hudBajas=hudLento=hudFase=hudHP=-1; relojEstrellas=0;
    memset(hudAnterior,' ',sizeof(hudAnterior));
}
void GP_Marcar(const GJuego *g) {
    marcar(g->naveX,g->naveY,16,18);
    if(g->jugador.activa) marcar((int)(g->jugador.x/256),(int)(g->jugador.y/256),16,16);
    for(unsigned i=0;i<G_ENEMIGOS;++i) {
        const GEnemigo *e=&g->enemigos[i]; if(e->activo) marcar(e->x,e->y,e->tam,e->tam);
    }
    for(unsigned i=0;i<G_BALAS;++i) if(g->balas[i].activa)
        marcar((int)(g->balas[i].x/256),(int)(g->balas[i].y/256),4,8);
    for(unsigned i=0;i<G_PARTICULAS;++i) if(g->particulas[i].vida)
        marcar(g->particulas[i].x,g->particulas[i].y,2,2);
    if(g->rayo) marcar(g->rayoX,g->rayoY,G_RAYO_ANCHO,g->rayo==1?G_ALTO-g->rayoY:G_RAYO_ALTO);
}
static void componer(const GJuego *g) {
    memset(franja,0,(unsigned)(rw*rh*2));
    for(unsigned i=0;i<CANTIDAD_ESTRELLAS;++i) {
        const Estrella *e=&estrellas[i]; rect(e->x,e->y,e->tamano,e->tamano,e->color);
    }
    if(g->rayo) {
        if(g->rayo==1) {
            /* Bordes del corredor para anunciar los 48 pixeles de ancho. */
            for(int y=g->rayoY;y<G_ALTO;y+=8) {
                rect(g->rayoX,y,2,4,0x07FF);
                rect(g->rayoX+G_RAYO_ANCHO-2,y,2,4,0x07FF);
            }
        } else {
#if GALAGA_SPRITE_RAYO
            int x1=g->rayoX>rx?g->rayoX:rx,y1=g->rayoY>ry?g->rayoY:ry;
            int x2=g->rayoX+G_RAYO_ANCHO<rx+rw?g->rayoX+G_RAYO_ANCHO:rx+rw;
            int y2=g->rayoY+G_RAYO_ALTO<ry+rh?g->rayoY+G_RAYO_ALTO:ry+rh;
            for(int y=y1;y<y2;++y) for(int x=x1;x<x2;++x) {
                uint16_t c=rayo_azul[(y-g->rayoY)*G_RAYO_ANCHO+x-g->rayoX];
                if(c) franja[(y-ry)*rw+x-rx]=c;
            }
#else
            rect(g->rayoX,g->rayoY,G_RAYO_ANCHO,G_RAYO_ALTO,0x019F);
            rect(g->rayoX+G_RAYO_ANCHO/4,g->rayoY,G_RAYO_ANCHO/2,G_RAYO_ALTO,0x07FF);
            rect(g->rayoX+G_RAYO_ANCHO/2-1,g->rayoY,2,G_RAYO_ALTO,0xBFFF);
#endif
        }
    }
    for(unsigned i=0;i<G_ENEMIGOS;++i) {
        const GEnemigo *e=&g->enemigos[i];
        if(e->activo && e->edad>=e->espera) sprite(e->x,e->y,e->tam,imagen(e->tipo),e->tipo);
    }
    if(g->jugador.activa) sprite((int)(g->jugador.x/256),(int)(g->jugador.y/256),16,bala,0);
    for(unsigned i=0;i<G_BALAS;++i) if(g->balas[i].activa) {
        int x=(int)(g->balas[i].x/256),y=(int)(g->balas[i].y/256);
        rect(x,y,4,8,0xFA80); rect(x+1,y+1,2,5,0xFFE0);
    }
    sprite(g->naveX,g->naveY,16,nave,0);
    if(g->lento) rect(g->naveX,g->naveY+16,16,2,0x07FF);
    for(unsigned i=0;i<G_PARTICULAS;++i) {
        const GParticula *p=&g->particulas[i]; if(p->vida) rect(p->x,p->y,2,2,p->color);
    }
}
void GP_Dibujar(const GJuego *g) {
    uint32_t inicio=HAL_GetTick(); galagaBytesFrame=0;
    if((uint32_t)(g->tiempo-relojEstrellas)>=100U) {
        relojEstrellas=g->tiempo;
        for(unsigned i=0;i<CANTIDAD_ESTRELLAS;++i) {
            Estrella *e=&estrellas[i]; marcar(e->x,e->y,e->tamano,e->tamano);
            e->y=(uint16_t)(e->y+e->velocidad);
            if(e->y+e->tamano>G_ALTO || e->y<G_HUD) e->y=G_HUD;
            marcar(e->x,e->y,e->tamano,e->tamano);
        }
    }
    GP_Marcar(g);
    for(int fila=0;fila<14;++fila) {
        int columna=0;
        while(columna<20) {
            if(!(sucio[fila]&(1UL<<columna))) { ++columna; continue; }
            int primera=columna;
            while(columna<20 && (sucio[fila]&(1UL<<columna))) ++columna;
            rx=primera*16; ry=G_HUD+fila*16; rw=(columna-primera)*16;
            rh=ry+16>G_ALTO?G_ALTO-ry:16;
            componer(g); transmitir(rx,ry,rw,rh);
        }
        sucio[fila]=0;
    }
    int hp=0;
    for(unsigned i=0;i<G_ENEMIGOS;++i)
        if(g->enemigos[i].activo && g->enemigos[i].tipo==G_BOSS) hp=g->enemigos[i].hp;
    if(hudBajas!=g->bajas || hudLento!=g->lento || hudFase!=g->fase || hudHP!=hp) {
        char s[41];
        if(g->nivel==3 && g->fase==1)
            snprintf(s,sizeof(s),"N3 BAJAS:%u/%u BOSS:%02d %s",g->bajas,g->objetivo,hp,g->lento?"LENTO":"");
        else snprintf(s,sizeof(s),"N%u BAJAS:%u/%u %s",g->nivel,g->bajas,g->objetivo,g->lento?"LENTO":"");
        /* Comparar y escribir solo caracteres cambiados, incluidos espacios
           que borran texto anterior. Evita limpiar 12800 bytes del HUD. */
        size_t largo=strlen(s);
        for(unsigned i=0;i<sizeof(hudAnterior);) {
            char c=i<largo?s[i]:' ';
            if(c==hudAnterior[i]) {++i;continue;}
            char tramo[40]; unsigned inicioTramo=i,n=0;
            do {
                c=i<largo?s[i]:' '; tramo[n++]=c; hudAnterior[i++]=c;
            } while(i<sizeof(hudAnterior) && (i<largo?s[i]:' ')!=hudAnterior[i]);
            tramo[n]=0; texto(tramo,4+(int)inicioTramo*8,4,1,0xFFFF);
        }
        hudBajas=g->bajas; hudLento=g->lento; hudFase=g->fase; hudHP=hp;
    }
    galagaDibujoMs=HAL_GetTick()-inicio;
    if(galagaDibujoMs>galagaFrameMaxMs) galagaFrameMaxMs=galagaDibujoMs;
}
