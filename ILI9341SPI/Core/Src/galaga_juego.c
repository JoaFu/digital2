#include "galaga_juego.h"
#include <string.h>

/* Tabla entera: orbitas sin trigonometria de punto flotante. */
static const int8_t seno[32] = {
    0,25,49,71,90,106,118,125,127,125,118,106,90,71,49,25,
    0,-25,-49,-71,-90,-106,-118,-125,-127,-125,-118,-106,-90,-71,-49,-25
};
static int limitar(int n, int a, int b) { return n < a ? a : (n > b ? b : n); }
static uint32_t azar(GJuego *g) {
    g->semilla = g->semilla * 1664525U + 1013904223U;
    return g->semilla;
}
static int solapa(int x, int y, int w, int h, int a, int b, int c, int d) {
    return x < a+c && x+w > a && y < b+d && y+h > b;
}
static int acercar(int valor, int meta, int paso) {
    if (valor < meta) return valor + paso > meta ? meta : valor + paso;
    return valor - paso < meta ? meta : valor - paso;
}
static GEnemigo *crear(GJuego *g, GTipo tipo, int x, int y, unsigned espera, unsigned fase) {
    for (unsigned i=0; i<G_ENEMIGOS; ++i) {
        GEnemigo *e = &g->enemigos[i];
        if (e->activo) continue;
        memset(e,0,sizeof(*e));
        e->activo=1; e->tipo=(uint8_t)tipo; e->tam=16;
        e->hp=tipo==G_BOSS ? G_HP_BOSS : tipo==G_VERDE ? G_HP_VERDE : tipo==G_AMARILLO ? G_HP_AMARILLO : 1;
        e->x=e->origenX=(fase&1U) ? G_ANCHO+16 : -32;
        e->y=-32; e->destinoX=(int16_t)x; e->destinoY=(int16_t)y;
        e->espera=espera; e->fase=(uint8_t)fase; e->entrando=1;
        return e;
    }
    return 0; /* Capacidad fija: nunca se sobrescribe un enemigo vivo. */
}
static void confeti(GJuego *g, const GEnemigo *e) {
    static const int8_t vx[8]={-3,-2,0,2,3,2,0,-2};
    static const int8_t vy[8]={0,-2,-3,-2,0,2,3,2};
    unsigned n=0;
    for (unsigned i=0; i<G_PARTICULAS && n<8; ++i) {
        GParticula *p=&g->particulas[i];
        if (p->vida) continue;
        p->x=(int16_t)(e->x+e->tam/2); p->y=(int16_t)(e->y+e->tam/2);
        p->vx=vx[n]; p->vy=vy[n]; p->vida=(uint8_t)(14U+azar(g)%8U);
        p->color=(n&1U) ? 0xFFFF : e->tipo==G_VERDE ? 0x07E0 : e->tipo==G_BOSS ? 0x07FF : 0xFFE0;
        ++n;
    }
}
static void formacionX(GJuego *g) {
    /* Diez reservas simultaneas; cada pareja entra con 150 ms de desfase. */
    for (unsigned i=0;i<10;++i) crear(g,G_CRUCE,0,0,(i/2)*150U,i);
}
uint8_t G_ObjetivosVivos(const GJuego *g) {
    uint8_t n=0;
    for (unsigned i=0;i<G_ENEMIGOS;++i)
        if (g->enemigos[i].activo && g->enemigos[i].tipo!=G_CRUCE) ++n;
    return n;
}
void G_Iniciar(GJuego *g, uint8_t nivel) {
    memset(g,0,sizeof(*g));
    g->nivel=(uint8_t)limitar(nivel,1,3); g->naveX=152; g->naveY=216;
    g->semilla=0xC0FFEEU; g->objetivo=g->nivel==1 ? 4 : g->nivel==2 ? 5 : 11;
    if (g->nivel==1) crear(g,G_CLASICO,152,44,0,0);
    if (g->nivel==2) {
        crear(g,G_VERDE,152,44,0,0);
        crear(g,G_CLASICO,72,96,150,1); crear(g,G_CLASICO,232,96,300,2);
    }
    if (g->nivel==3) {
        crear(g,G_AMARILLO,88,44,0,0); crear(g,G_VERDE,216,44,0,1);
        GEnemigo *a=crear(g,G_CLASICO,72,100,150,2);
        GEnemigo *b=crear(g,G_CLASICO,232,100,300,3);
        if(a) a->reposiciones=1;
        if(b) b->reposiciones=1;
    }
}
static void danar(GJuego *g, GEnemigo *e) {
    if (--e->hp) return;
    GEnemigo muerto=*e;
    confeti(g,e); e->activo=0;
    if (muerto.tipo==G_CRUCE) { ++g->protecciones; return; }
    ++g->bajas;
    if (g->nivel==1 && g->fase==1 && !g->reposicionUsada) {
        g->reposicionUsada=1;
        crear(g,G_CLASICO,muerto.destinoX,muerto.destinoY,200,muerto.fase);
    } else if (g->nivel==2 && muerto.tipo==G_VERDE) {
        crear(g,G_AMARILLO,96,48,150,0); crear(g,G_AMARILLO,208,48,300,1);
    } else if (g->nivel==3 && g->fase==0 && muerto.reposiciones) {
        crear(g,G_CLASICO,muerto.destinoX,muerto.destinoY,200,muerto.fase);
    }
}
static void disparar(GJuego *g,int x,int y,int vx,int vy) {
    for(unsigned i=0;i<G_BALAS;++i) if(!g->balas[i].activa) {
        g->balas[i]=(GBala){x*256,y*256,(int16_t)vx,(int16_t)vy,1}; return;
    }
}
static uint8_t hayBalas(const GJuego *g) {
    for(unsigned i=0;i<G_BALAS;++i) if(g->balas[i].activa) return 1;
    return 0;
}
static void ataqueOrdenado(GJuego *g) {
    if(g->nivel!=3 || g->fase!=1 || g->celebracion) return;
    /* El rayo ocupa su propia ventana. No acumular turnos pendientes. */
    if(g->rayo || g->lentoRestante) {g->esperaAtaque=G_BOSS_TURNO_MS;return;}
    if(g->esperaAtaque>G_PASO_MS) {g->esperaAtaque-=G_PASO_MS;return;}
    g->esperaAtaque=0;
    if(hayBalas(g)) return;
    for(unsigned n=0;n<5;++n) {
        unsigned turno=(g->turnoAtaque+n)%5U;
        unsigned fase=turno==4?0:turno+1;
        for(unsigned i=0;i<G_ENEMIGOS;++i) {
            GEnemigo *e=&g->enemigos[i];
            if(!e->activo || e->entrando || e->tipo==G_CRUCE || e->fase!=fase) continue;
            int x=e->x+e->tam/2-2,y=e->y+e->tam;
            int distancia=g->naveY-y;
            int vx=distancia>0?limitar((g->naveX+8-x)*640/distancia,-256,256):0;
            if(e->tipo==G_BOSS) {
                disparar(g,x-3,y,vx-192,640); disparar(g,x+3,y,vx+192,640);
            } else disparar(g,x,y,vx,640);
            g->turnoAtaque=(uint8_t)((turno+1U)%5U);
            g->esperaAtaque=G_BOSS_TURNO_MS;
            return;
        }
    }
}
static void moverEnemigo(GJuego *g,GEnemigo *e) {
    e->edad+=G_PASO_MS;
    if(e->edad<e->espera) return;
    uint32_t t=e->edad-e->espera;
    if(e->tipo==G_CRUCE) {
        e->entrando=0;
        if(t>=G_CRUCE_VUELO_MS) { e->activo=0; return; }
        int recorrido=(int)(t*352U/G_CRUCE_VUELO_MS);
        e->x=(int16_t)((e->fase&1U) ? 320-recorrido : -16+recorrido);
        e->y=(int16_t)(4+(int)(t*252U/G_CRUCE_VUELO_MS));
        return; /* Los protectores no disparan. */
    }
    if(t<800U) {
        /* Entrada diagonal con frenado cuadratico al llegar a su formacion. */
        int u=(int)(800U-t);
        int mezcla=640000-u*u;
        e->x=(int16_t)(e->origenX+(e->destinoX-e->origenX)*mezcla/640000);
        e->y=(int16_t)(-32+(e->destinoY+32)*mezcla/640000);
        return;
    }
    e->entrando=0;
    unsigned p=((t-800U)/60U+e->fase*8U)&31U;
    if(e->tipo==G_CLASICO) {
        int amplitud=(g->nivel==1 && g->fase==0) ? 120 : 36;
        e->x=(int16_t)(e->destinoX+amplitud*seno[p]/127);
        e->y=e->destinoY;
    } else if(e->tipo==G_VERDE) {
        e->x=(int16_t)(e->destinoX+34*seno[p]/127);
        e->y=(int16_t)(e->destinoY+12*seno[(p+8U)&31U]/127);
    } else if(e->tipo==G_AMARILLO) {
        /* Persigue lateralmente, acotado a su zona para conservar formacion. */
        int meta=limitar(g->naveX,e->destinoX-45,e->destinoX+45);
        e->x=(int16_t)acercar(e->x,meta,2);
        e->y=(int16_t)(e->destinoY+8*seno[p]/127);
    } else {
        e->x=(int16_t)acercar(e->x,limitar(g->naveX,24,280),1);
        e->y=(int16_t)(e->destinoY+4*seno[p]/127);
    }
    e->x=(int16_t)limitar(e->x,0,G_ANCHO-e->tam);
    /* Fase jefe: un coordinador concede los turnos, no timers individuales. */
    if(g->nivel==3 && g->fase==1) return;
    e->disparo+=G_PASO_MS;
    uint32_t intervalo=e->tipo==G_BOSS ? 1400U : e->tipo==G_VERDE ? 1700U : e->tipo==G_AMARILLO ? 1450U : g->nivel==1 ? 2000U : 2100U;
    intervalo+=e->fase*110U;
    if(e->disparo<intervalo) return;
    e->disparo=0;
    int x=e->x+e->tam/2-2, y=e->y+e->tam;
    int vy=g->nivel==1 ? 640 : 768;
    int distancia=g->naveY-y;
    int vx=distancia>0 ? limitar((g->naveX+8-x)*vy/distancia,-384,384) : 0;
    if(e->tipo==G_CLASICO) disparar(g,x,y,0,vy);
    else if(e->tipo==G_AMARILLO) {
        disparar(g,x-3,y,vx-160,vy); disparar(g,x+3,y,vx+160,vy);
    } else if(e->tipo==G_VERDE) disparar(g,x,y,vx,896);
    else {
        disparar(g,x,y,vx-256,vy); disparar(g,x,y,vx,vy); disparar(g,x,y,vx+256,vy);
    }
}
static void actualizarRayo(GJuego *g) {
    GEnemigo *boss=0;
    for(unsigned i=0;i<G_ENEMIGOS;++i)
        if(g->enemigos[i].activo && g->enemigos[i].tipo==G_BOSS) boss=&g->enemigos[i];
    if(!boss || boss->entrando) { g->rayo=0; return; }
    g->cicloRayo+=G_PASO_MS;
    if(g->cicloRayo>=G_RAYO_CICLO_MS) {
        /* Espera a que se despeje la ultima rafaga antes de avisar el rayo. */
        if(hayBalas(g)) return;
        g->cicloRayo=0; g->rayo=1;
        g->rayoX=(int16_t)limitar(boss->x+boss->tam/2-G_RAYO_ANCHO/2,0,G_ANCHO-G_RAYO_ANCHO);
        g->rayoY=(int16_t)(boss->y+boss->tam);
    } else if(g->rayo && g->cicloRayo>=G_RAYO_AVISO_MS+G_RAYO_ACTIVO_MS) g->rayo=0;
    else if(g->rayo && g->cicloRayo>=G_RAYO_AVISO_MS) {
        /* Sprite completo de 48x80: desciende sin estirarse ni repetirse.
           Conserva la columna anunciada aunque el jefe se mueva. */
        g->rayo=2; g->rayoY+=G_RAYO_VELOCIDAD;
        if(g->rayoY>=G_ALTO) g->rayo=0;
    }
    if(g->rayo==2 && solapa(g->naveX,g->naveY,16,16,g->rayoX,g->rayoY,G_RAYO_ANCHO,G_RAYO_ALTO))
        g->lentoRestante=900;
}
static void actualizarParticulas(GJuego *g) {
    for(unsigned i=0;i<G_PARTICULAS;++i) {
        GParticula *p=&g->particulas[i];
        if(!p->vida) continue;
        --p->vida; p->x+=p->vx; p->y+=p->vy;
        if((p->vida%6)==0) ++p->vy;
        if(p->x<0 || p->x>=G_ANCHO || p->y<G_HUD || p->y>=G_ALTO) p->vida=0;
    }
}
static void cambiarOleada(GJuego *g) {
    if(G_ObjetivosVivos(g)) return;
    if(g->nivel==1 && g->fase==0) {
        g->fase=1; crear(g,G_CLASICO,80,52,100,0); crear(g,G_CLASICO,224,52,250,1);
    } else if(g->nivel==3 && g->fase==0) {
        g->fase=1;
        /* Limpia balas antes de presentar al jefe; conserva el confeti. */
        memset(g->balas,0,sizeof(g->balas)); g->jugador.activa=0;
        crear(g,G_BOSS,144,28,200,0);
        crear(g,G_AMARILLO,48,94,350,1); crear(g,G_AMARILLO,128,94,450,2);
        crear(g,G_VERDE,208,94,550,3); crear(g,G_VERDE,272,94,650,4);
        formacionX(g); g->cruce=g->tiempo; g->cicloRayo=0;
        g->esperaAtaque=G_BOSS_ENTRADA_MS; g->turnoAtaque=0;
    } else {
        /* Deja ver la ultima explosion antes de la pantalla de victoria. */
        g->celebracion=500; g->rayo=0; g->lento=0; g->lentoRestante=0;
        memset(g->balas,0,sizeof(g->balas)); g->jugador.activa=0;
        memset(g->enemigos,0,sizeof(g->enemigos));
    }
}
void G_Paso(GJuego *g,GEntrada in) {
    if(g->resultado!=G_JUGANDO) return;
    g->tiempo+=G_PASO_MS; actualizarParticulas(g);
    if(g->celebracion) {
        if(g->celebracion<=G_PASO_MS) { g->celebracion=0; g->resultado=G_VICTORIA; }
        else g->celebracion-=G_PASO_MS;
        return;
    }
    if(g->lentoRestante>G_PASO_MS) g->lentoRestante-=G_PASO_MS;
    else g->lentoRestante=0;
    g->lento=g->lentoRestante!=0;
    int velocidad=g->lento ? 1 : 3;
    if(in.izquierda!=in.derecha) g->naveX+=(int16_t)(in.izquierda ? -velocidad : velocidad);
    g->naveX=(int16_t)limitar(g->naveX,0,G_ANCHO-16);
    if(in.disparar && !g->jugador.activa)
        g->jugador=(GBala){g->naveX*256,(g->naveY-16)*256,0,-1536,1};
    if((g->nivel==2 || (g->nivel==3 && g->fase==1)) && (uint32_t)(g->tiempo-g->cruce)>=G_CRUCE_MS) {
        formacionX(g); g->cruce=g->tiempo;
    }
    for(unsigned i=0;i<G_ENEMIGOS;++i) if(g->enemigos[i].activo) moverEnemigo(g,&g->enemigos[i]);
    if(g->jugador.activa) {
        int previo=(int)(g->jugador.y/256);
        g->jugador.y+=g->jugador.vy;
        int x=(int)(g->jugador.x/256)+6, y=(int)(g->jugador.y/256)+2;
        /* Barrido vertical: elegir el blanco mas cercano desde abajo.
           Las escoltas y los cruces interceptan antes que el jefe. */
        int elegido=-1, fondo=-100;
        for(unsigned i=0;i<G_ENEMIGOS;++i) {
            GEnemigo *e=&g->enemigos[i];
            if(!e->activo || e->edad<e->espera || e->y+e->tam<=G_HUD) continue;
            if(solapa(x,y,4,previo+14-y,e->x,e->y,e->tam,e->tam) && e->y+e->tam>fondo) {
                elegido=(int)i; fondo=e->y+e->tam;
            }
        }
        if(elegido>=0) { danar(g,&g->enemigos[elegido]); g->jugador.activa=0; }
        if(g->jugador.y/256+16<=G_HUD) g->jugador.activa=0;
    }
    for(unsigned i=0;i<G_BALAS;++i) {
        GBala *b=&g->balas[i]; if(!b->activa) continue;
        b->x+=b->vx; b->y+=b->vy;
        int x=(int)(b->x/256), y=(int)(b->y/256);
        if(solapa(x,y,4,8,g->naveX+2,g->naveY+2,12,12)) { g->resultado=G_DERROTA; return; }
        if(y>=G_ALTO || x<-4 || x>=G_ANCHO) b->activa=0;
    }
    actualizarRayo(g);
    ataqueOrdenado(g);
    cambiarOleada(g);
}
