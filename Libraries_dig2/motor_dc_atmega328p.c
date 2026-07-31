/************************************************* 
*Author: Joaquín Fuentes, Luis Arriaza
*Control de motor dc mediante el driver TB6612FNG
*************************************************/

#include "motor_dc_atmega328p"

// Inicializción de pines

void MotorA_Init(void){
	// Haz que los pines sean de salida
	dirDDR1	  |= (1 << AIN1_PIN);
	dirDDR2   |= (1 << AIN2_PIN);
	stbyDDR   |= (1 << STBY_PIN);
	pwmDDR	  |= (1 << PWMA_PIN);
	
	// Detener el motor y habilitar el driver
	dirPORT1    &= ~(1 << AIN1_PIN);
	dirPORT2	&= ~(1 << AIN2_PIN);
	MotorA_Standby(1); // 1 = Activo, 0 = Standby
	
	// Configuración del Timer 0 para Fast PWM
	
	TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);
	TCCR0B = (1 << CS01)   | (1 << CS00); // Prescaler de 64
	OCR0A  = 0;
}

/*******************************************************************/
// Non interrupt
/*******************************************************************/
void MotorA_Drive(int16_t speed){
	if (speed > 0){
		dirPORT1 |=  (1 << AIN1_PIN);
		dirPORT2 &= ~(1 << AIN2_PIN);
		
		// No más de 255
		if (speed > 255) speed = 255;
		OCR0A = (uint8_t)speed;
	} 
	else if (speed < 0){
		dirPORT1 &= ~(1 << AIN1_PIN);
		dirPORT2 |=  (1 << AIN2_PIN);
	
		speed = -speed;
		if (speed > 255) speed = 255;
		OCR0A = (uint8_t)speed;
	}
	
	else {
		dirPORT1 &= ~(1 << AIN1_PIN);
		dirPORT2 &= ~(1 << AIN2_PIN);
		OCR0A = 0;
	} 
} 

// Freno en seco 
void MotorA_Brake(void){
	dirPORT1 |= (1 << AIN1_PIN);
	dirPORT2 |= (1 << AIN2_PIN);
	OCR0A = 0; //el freno se da por la lógica de AIN
}

// Controla el pin de Standby 0 = Driver apagado, 1 = Driver encendido
void MotorA_Standby(uint8_t state){
	if (state){
		STBY_PORT |= (1 << STBY_PIN);
	}
	else{
		STBY_PORT &= ~(1 << STBY_PIN);
	}
	
}