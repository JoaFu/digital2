#ifndef MOTOR_DC_ATMEGA328P_H_
#define MOTOR_DC_ATMEGA328P_H_

/*************************************************
 * Author : Joaquín Fuentes, Luis Arriaza
 * Device : ATmega328P
 * Driver : TB6612FNG
 * Module : DC Motor Driver (Channel A)
 *************************************************/

#include <avr/io.h>
#include <stdint.h>

/*==================================================
=                Configuración de Pines            =
==================================================*/

/*---------- AIN1 ----------*/
#define MOTOR_AIN1_PORT     PORTD
#define MOTOR_AIN1_DDR      DDRD
#define MOTOR_AIN1_PIN      PD7

/*---------- AIN2 ----------*/
#define MOTOR_AIN2_PORT     PORTB
#define MOTOR_AIN2_DDR      DDRB
#define MOTOR_AIN2_PIN      PB0

/*---------- PWM (OC0A) ----------*/
#define MOTOR_PWM_PORT      PORTD
#define MOTOR_PWM_DDR       DDRD
#define MOTOR_PWM_PIN       PD6

/*---------- Standby ----------*/
#define MOTOR_STBY_PORT     PORTB
#define MOTOR_STBY_DDR      DDRB
#define MOTOR_STBY_PIN      PB1

/*==================================================
=               Constantes de Control             =
==================================================*/

#define MOTOR_ENABLE        1U
#define MOTOR_STANDBY       0U

#define MOTOR_MAX_SPEED     255
#define MOTOR_MIN_SPEED    -255

/*==================================================
=              Prototipos de Funciones            =
==================================================*/

/**
 * Inicializa el TB6612FNG y configura el Timer0 en Fast PWM.
 */
void MotorA_Init(void);

/**
 * Controla velocidad y dirección.
 *
 * speed > 0  -> Giro hacia adelante.
 * speed < 0  -> Giro hacia atrás.
 * speed = 0  -> Motor detenido (coast).
 *
 * Rango permitido:
 * [-255,255]
 */
void MotorA_Drive(int16_t speed);

/**
 * Aplica frenado dinámico.
 */
void MotorA_Brake(void);

/**
 * Activa o desactiva el modo Standby.
 *
 * Utilizar:
 * MOTOR_ENABLE
 * MOTOR_STANDBY
 */
void MotorA_Standby(uint8_t state);

/**
 * Detiene el motor sin aplicar freno.
 */
void MotorA_Stop(void);

#endif /* MOTOR_DC_ATMEGA328P_H_ */