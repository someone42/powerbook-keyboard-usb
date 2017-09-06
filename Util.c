/** \file
 *
 *  Utility functions for GPIO interaction, and for waiting/delaying.
 *  This is specific to the AVR8 architecture.
 *
 *  This file is licensed as described by the file BSD.txt
 */

#include <stdint.h>
#include <avr/io.h>
#include "Util.h"

/** Configure GPIO pin as an input (with pull-up) or as an output.
 *  \param[in]     Port      0 = PORTA, 1 = PORTB, 2 = PORTC etc.
 *  \param[in]     Num       0 = pin 0 of the specified port, 1 = pin 1 of the specified port etc.
 *  \param[in]     IsOutput  Specify 0 to set pin as input with pull-up, 1 to set pin as output
 */
void SetPortPinDirection(const uint8_t Port, const uint8_t Num, const uint8_t IsOutput)
{
	uint8_t Mask = 1 << Num;
	switch(Port)
	{
	case 0:
		if (IsOutput) DDRA |= Mask;
		else
		{
			DDRA &= ~Mask;
			PORTA |= Mask;
		}
		break;
	case 1:
		if (IsOutput) DDRB |= Mask;
		else
		{
			DDRB &= ~Mask;
			PORTB |= Mask;
		}
		break;
	case 2:
		if (IsOutput) DDRC |= Mask;
		else
		{
			DDRC &= ~Mask;
			PORTC |= Mask;
		}
		break;
	case 3:
		if (IsOutput) DDRD |= Mask;
		else
		{
			DDRD &= ~Mask;
			PORTD |= Mask;
		}
		break;
	case 4:
		if (IsOutput) DDRE |= Mask;
		else
		{
			DDRE &= ~Mask;
			PORTE |= Mask;
		}
		break;
	case 5:
		if (IsOutput) DDRF |= Mask;
		else
		{
			DDRF &= ~Mask;
			PORTF |= Mask;
		}
		break;
	}
}

/** Write to a GPIO pin. The pin must have been configured as an output.
 *  \param[in]     Port   0 = PORTA, 1 = PORTB, 2 = PORTC etc.
 *  \param[in]     Num    0 = pin 0 of the specified port, 1 = pin 1 of the specified port etc.
 *  \param[in]     Val    Specify 0 to set pin low, 1 to set pin high
 */
void WritePortPin(const uint8_t Port, const uint8_t Num, const uint8_t Val)
{
	uint8_t Mask = 1 << Num;
	switch(Port)
	{
	case 0:
		if (Val) PORTA |= Mask;
		else PORTA &= ~Mask;
		break;
	case 1:
		if (Val) PORTB |= Mask;
		else PORTB &= ~Mask;
		break;
	case 2:
		if (Val) PORTC |= Mask;
		else PORTC &= ~Mask;
		break;
	case 3:
		if (Val) PORTD |= Mask;
		else PORTD &= ~Mask;
		break;
	case 4:
		if (Val) PORTE |= Mask;
		else PORTE &= ~Mask;
		break;
	case 5:
		if (Val) PORTF |= Mask;
		else PORTF &= ~Mask;
		break;
	}
}

/** Read from a GPIO pin. The pin must have been configured as an input.
 *  \param[in]     Port   0 = PORTA, 1 = PORTB, 2 = PORTC etc.
 *  \param[in]     Num    0 = pin 0 of the specified port, 1 = pin 1 of the specified port etc.
 *  \return uint8_t 0 if pin is low, 1 if pin is high.
 */
uint8_t ReadPortPin(const uint8_t Port, const uint8_t Num)
{
	switch(Port)
	{
	case 0:
		return (PINA >> Num) & 1;
	case 1:
		return (PINB >> Num) & 1;
	case 2:
		return (PINC >> Num) & 1;
	case 3:
		return (PIND >> Num) & 1;
	case 4:
		return (PINE >> Num) & 1;
	case 5:
		return (PINF >> Num) & 1;
	default:
		return 0;
	}
}

/** Delay for some number of microseconds. This uses Timer1 as a timing
 *  reference, so it will wait correctly, even if an interrupt occurs,
 *  as long as the interrupt handler doesn't execute for too long.
 *  This assumes Timer1 counts at a rate of 2 MHz.
 *  \param[in]    MicroSeconds    Number of microseconds to wait. Must be less than
 *                                or equal to 32767.
 */
void DelayMicroseconds(uint16_t MicroSeconds)
{
	uint16_t StartTime, CurrentTime, DesiredCount;

	DesiredCount = MicroSeconds * 2; /* assume counter counts at a rate of 2 MHz */
	StartTime = TCNT1;
	do
	{
		CurrentTime = TCNT1;
	} while ((CurrentTime - StartTime) < DesiredCount);
}
