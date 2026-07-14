/*
 * practica1_digital2.c
 *
 * Created:
 * Author: Joaquin Fuentes
 * Description: Juego de carreras - Laboratorio 1
 *
*/
/*****************************************/
// Encabezado (Libraries)
#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include "display_ordered_opcodes.h"
/****************************************/
//Config
#define contador_win 4      // ahora solo 4 pulsaciones para ganar (patron 0001->0010->0100->1000)
#define antirrebote_ms 25
/****************************************/
// Function protoypes
void init_ports(void);
void init_timer0(void);
void init_pcint(void);
void leer_botones(void);
void verificar_ganador(void);
/****************************************/
// Estadoso del juego
typedef enum {ESPERA, CUENTA_REGRESIVA, CARRERA, FIN_CARRERA} estado_t;
// Estructura de antirrebote 
typedef struct {
	bool estado_estable; // 1 = botón suelto, 0 = presionado
	bool estado_previo; 
	uint8_t contador_antirrebote;
	bool evento_flanco; // set para cualquier pulsación nueva
	} boton_t;
/****************************************/
// Variables globales
volatile estado_t estado_juego = ESPERA;
volatile uint8_t contador_regresivo = 5;
volatile uint16_t ticks_ms = 0;
uint16_t ms_acumulados = 0;
volatile uint8_t mux = 0;

volatile uint8_t leds = 0x00;
volatile uint8_t digito_display = 0;
volatile bool display_activo = false;
volatile bool pcint_pendiente = false;

volatile uint8_t contador_j1 = 0;
volatile uint8_t contador_j2 = 0;
volatile uint8_t  ganador = 0; // 0 = nadie, 1 = J1, 2 = J2

boton_t boton_inicio = {true, true, 0, false};
boton_t boton_j1     = {true, true, 0, false};
boton_t boton_j2     = {true, true, 0, false};

/****************************************/
// Main Function	
int main(void)
{
	init_ports();
	init_timer0();
	init_pcint();
	sei();
	
	while (1)
	{

        if (ticks_ms >= 1)
        {
	        ticks_ms = 0;
	        leer_botones();

	        if (estado_juego == CUENTA_REGRESIVA)
	        {
		        ms_acumulados++;
		        if (ms_acumulados >= 1000)
		        {
			        ms_acumulados = 0;
			        if (contador_regresivo > 0)
			        {
				        contador_regresivo--;
			        }
			        else
			        {
				        estado_juego = CARRERA;
			        }
		        }
	        }
        }
			
		switch (estado_juego)
		{
			case ESPERA:
				digito_display = 0;
				display_activo = false;
				leds = 0x00;
				
				if (boton_inicio.evento_flanco)
				{
					boton_inicio.evento_flanco = false;
					contador_regresivo = 5;
					ms_acumulados = 0;
					contador_j1 = 0;
					contador_j2 = 0;
					ganador = 0;
					estado_juego = CUENTA_REGRESIVA;
				}
				break;
				
			case CUENTA_REGRESIVA:
				digito_display = contador_regresivo;
				display_activo = true;
				leds = 0x00; 
				// los botones no se leen en este estado.
				break;
				
			case CARRERA:
			{
				if (boton_j1.evento_flanco)
				{
					boton_j1.evento_flanco = false;
					if (contador_j1 < contador_win)
					{
						contador_j1++;
					}
				}
				
				if (boton_j2.evento_flanco)
				{
					boton_j2.evento_flanco = false;
					if (contador_j2 < contador_win)
					{
						contador_j2++;
					}
				}
				// leds muestran un solo bit corrido por cada pulsación:
				// 1 pulsación -> 0001, 2 -> 0010, 3 -> 0100, 4 -> 1000
				uint8_t nibble_j1 = (contador_j1 > 0) ? (1 << (contador_j1 - 1)) : 0x00;
				uint8_t nibble_j2 = (contador_j2 > 0) ? (1 << (contador_j2 - 1)) : 0x00;
				leds = ((nibble_j1 & 0x0F) << 4) | (nibble_j2 & 0x0F);
				display_activo = false;
				verificar_ganador();
				break;
			}
			case FIN_CARRERA:
				if (ganador == 1)
				{
					leds = 0xF0; // Prende el nibble de J1 únicamente
					digito_display = 1;
				}
				else if (ganador == 2)
				{
					leds = 0x0F; // Prende el nibble de J2 únicamente
					digito_display = 2;
				}
				display_activo = true;
				
				if (boton_inicio.evento_flanco)
				{
					boton_inicio.evento_flanco = false;
					estado_juego = ESPERA;
				}
				break;
		}
	}
}

/****************************************/
// NON-Interrupt subroutines
void init_ports(void)
{
	DDRD = 0xFF; // Display y LEDs
	PORTD = 0x00;

	// PB0 = enable display, PB1 = enable LEDs
	DDRB |= (1 << PB0) | (1 << PB1);
	PORTB &= ~((1 << PB0) | (1 << PB1));

	// Botones PC0 = inicio, PC1 = J1, PC2 = J2
	DDRC &= ~((1 << PC0) | (1 << PC1) | (1 << PC2));
	PORTC |= (1 << PC0) | (1 << PC1) | (1 << PC2); // pull-ups
}

void init_timer0(void)
{
	TCCR0A = (1 << WGM01);             // CTC
	OCR0A  = 250;                      // ~1ms con prescaler 64 @16MHz
	TCCR0B = (1 << CS01) | (1 << CS00);
	TIMSK0 = (1 << OCIE0A);
}

void init_pcint(void)
{
	PCICR  |= (1 << PCIE1);
	PCMSK1 |= (1 << PCINT8) | (1 << PCINT9) | (1 << PCINT10);
}

// Antirrebote no bloqueante para cada botón
static void antirrebote_boton(boton_t*b, bool lectura_cruda)
{
	if (lectura_cruda != b->estado_previo)
	{
		// La lectura cambió
		b->estado_previo = lectura_cruda;
		b->contador_antirrebote = 0;
	}
	else if (b->contador_antirrebote < 25) //25ms es el tiempo de estabilidad
	{
		b->contador_antirrebote++;
		// se acaba de confirmar estabilidad en este tick
		if (b->contador_antirrebote == 25)
		{
			if (b->estado_estable == true && lectura_cruda == false)
			{
				b->evento_flanco = true;
			}
			b->estado_estable = lectura_cruda;
		}
	}
}

// Se llama cada 1ms desde el loop principal y dado a que cada botón es independiente la lectura de uno no es bloqueante con la otra.
void leer_botones(void)
{
	bool lectura_inicio = (PINC & (1 << PC0)) != 0;
    bool lectura_j1     = (PINC & (1 << PC1)) != 0;
    bool lectura_j2     = (PINC & (1 << PC2)) != 0;
	
	antirrebote_boton(&boton_inicio, lectura_inicio);
	// Botones de J1 y J2 solo en estado de carrera
	if (estado_juego == CARRERA)
	{
		antirrebote_boton(&boton_j1, lectura_j1);
		antirrebote_boton(&boton_j2, lectura_j2);
	}
}

void verificar_ganador(void)
{
	if (contador_j1 >= contador_win)
	{
		ganador = 1;
		estado_juego = FIN_CARRERA;
	}
	else if (contador_j2 >= contador_win)
	{
		ganador = 2;
		estado_juego = FIN_CARRERA;
	}
}

/****************************************/
// Interrupt routines

ISR (TIMER0_COMPA_vect)
{
	ticks_ms++;
	PORTB &= ~((1 << PB0) | (1 << PB1));
	
	if (mux == 0)
	{
		if (display_activo)
		{
			display_show(digito_display);
		}
		else
		{
			PORTD = 0x00;
		}
		PORTB |= (1 << PB0);
		mux = 1;
	}
	else
	{
		PORTD = leds;
		PORTB |= (1 << PB1);
		mux = 0;
	}
}

// conteos por rebote mecanico.
ISR(PCINT1_vect)
{
	pcint_pendiente = true;
}