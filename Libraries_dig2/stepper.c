#include "stepper.h"
#include <avr/interrupt.h>

static const uint8_t stepSequence[8][4]= {
	{1,0,0,0},
	{1,1,0,0},
	{0,1,0,0},
	{0,1,1,0},
	{0,0,1,0},
	{0,0,1,1},
	{0,0,0,1},
	{1,0,0,1}
};

static StepperPins_t motorPins;
static uint16_t stepDelay;
static int32_t currentSteps;
static int32_t targetSteps;
static int8_t stepDirection;
static uint8_t sequenceDirection;
static bool moving; 
static uint32_t lastStepTick;

static const int32_t *positionTable;
static uint8_t positionTableSize;
static uint8_t currentPositionIndex;

static volatile uint32_t systemTick;


// Interrupt routines
ISR(TIMER0_COMA_vect){
	systemTick++;
}