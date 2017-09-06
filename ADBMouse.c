/** \file
 *
 *  Interfaces with an ADB (Apple Desktop Bus) mouse. To query the mouse,
 *  call ADBMouseInit() once, then call ADBPollMouse() periodically.
 *
 *  This file is licensed as described by the file BSD.txt
 *
 *  Resources used to write the code here:
 *  AN591 from Microchip: http://ww1.microchip.com/downloads/en/AppNotes/00591b.pdf
 *  has a good overview of the transaction format, with required timings.
 *
 *  https://github.com/tmk/tmk_keyboard/blob/master/tmk_core/protocol/adb.c
 *  has lots of links resources. It also describes command formats.
 *
 *  https://developer.apple.com/legacy/library/technotes/hw/hw_01.html
 *  describes the ADB mouse protocol.
 */

#include <stdint.h>
#include <avr/io.h>
#include <util/delay.h>
#include "Util.h"
#include "KeyboardMouse.h"

/** The port that the ADB data line is connected to.
 *  0 = PORTA, 1 = PORTB, 2 = PORTC etc. */
#define ADB_PORT				3
/** The pin within a port that the ADB data line is connected to.
 *  0 = PA0, PB0, PC1 etc., 1 = PA1, PB1, PC1 etc. */
#define ADB_PIN					1
/** Macro to quickly read state of ADB data line. This is used instead of
  * "ReadPortPin(ADB_PORT, ADB_PIN)" in timing critical code. */
#define READ_ADB_PIN			((PIND >> 1) & 1)
/** Number of microseconds to wait before timing out. */
#define ADB_TIMEOUT				255
/** Time (in microseconds) threshold for determining whether a bit is 0 or 1.
 *  If the ADB line is observed to be in the low state for an amount of time
 *  below this threshold, that means the bit was a 1. If the line was observed
 *  to be in the low state for an amount of time above or equal to this
 *  threshold, that means the bit was a 0. */
#define ADB_THRESHOLD			50

/** Minimum value of AccumulatedX/AccumulatedY. This is currently set so
 *  that accumulated values will fit in an int8_t. */
#define ACCUMULATED_MIN			(-127)
/** Maximum value of AccumulatedX/AccumulatedY. This is currently set so
 *  that accumulated values will fit in an int8_t. */
#define ACCUMULATED_MAX			127

/** Accumulated X movement. Updated by ADBPollMouse(). This needs to be
 *  periodically reset back to 0, otherwise it will start to clip. */
int16_t AccumulatedX;
/** Accumulated Y movement. Updated by ADBPollMouse(). This needs to be
 *  periodically reset back to 0, otherwise it will start to clip. */
int16_t AccumulatedY;
/** 0 = not pressed, 1 = pressed. Updated by ADBPollMouse(). */
uint8_t Button1State;
/** 0 = not pressed, 1 = pressed. Updated by ADBPollMouse(). Some mice don't
 *  have a second button, in those cases this will always be 0. */
uint8_t Button2State;

/** Write a 0 bit on the ADB data line. */
static void ADBWriteZeroBit(void)
{
	WritePortPin(ADB_PORT, ADB_PIN, 0);
	DelayMicroseconds(65);
	WritePortPin(ADB_PORT, ADB_PIN, 1);
	DelayMicroseconds(35);
}

/** Write a 1 bit on the ADB data line. */
static void ADBWriteOneBit(void)
{
	WritePortPin(ADB_PORT, ADB_PIN, 0);
	DelayMicroseconds(35);
	WritePortPin(ADB_PORT, ADB_PIN, 1);
	DelayMicroseconds(65);
}

/** Write a command to the ADB data line.
 *  \param[in]     Command   Command to write. See https://github.com/tmk/tmk_keyboard/blob/master/tmk_core/protocol/adb.c#L315
 *                           for command format.
 */
static void ADBWriteCommand(uint8_t Command)
{
	uint8_t i;

	/* Attention signal: low state for 800 us. */
	WritePortPin(ADB_PORT, ADB_PIN, 0);
	DelayMicroseconds(800);
	/* Sync signal: high state for 70 us. */
	WritePortPin(ADB_PORT, ADB_PIN, 1);
	DelayMicroseconds(70);
	/* Command byte: eight 100 us bit cells, MSB first. */
	for (i = 0; i < 8; i++)
	{
		if (Command & 0x80)
			ADBWriteOneBit();
		else
			ADBWriteZeroBit();
		Command <<= 1;
	}
	/* Stop bit: always a 0 bit. */
	ADBWriteZeroBit();
}

/** Wait until ADB line is in the specified state. The ADB port/pin must
 *  be set to input mode before calling this.
 *  \param[in]     DesiredLineState   0 = wait until line is low, 1 = wait
 *                                    until line is high.
 *  \return uint16_t Number of microseconds it took for the ADB line to
 *                   transition to the desired state, or ADB_TIMEOUT if a
 *                   timeout occurred.
 */
static uint16_t ADBWait(const uint8_t DesiredLineState)
{
	uint16_t StartTime, Microseconds;

	Microseconds = 0;
	StartTime = TCNT1;
	while (READ_ADB_PIN != DesiredLineState)
	{
		Microseconds = (TCNT1 - StartTime) / 2;
		if (Microseconds >= ADB_TIMEOUT)
			return ADB_TIMEOUT;
	}
	return Microseconds;
}

/** Read 16 data bits from the ADB data line.
 *  \param[in]     OutRegisterValue   The 16 data bits will be written here, if
 *                                    the operation was successful.
 *  \return uint8_t 1 if operation was successful valid, 0 if a timeout occurred.
 */
static uint8_t ADBRead16(uint16_t *OutRegisterValue)
{
	uint16_t LowDuration[18]; // timings for low state
	uint16_t RegisterValue;
	uint8_t i;

	/* Set ADB line to be input. This must be restored back to output mode
	 * before returning from this function. */
	SetPortPinDirection(ADB_PORT, ADB_PIN, 0);
	/* Stop to Start (Tlt in AN591) wait time. The minimum time is 160 us, but
	 * we only wait for 100 us so that there is time to get to the ADBWait()
	 * call below. */
	DelayMicroseconds(100);
	/* Take timing measurements of the low state for the next 18 bits.
	 * This includes the start bit (1), 16 data bits, and the stop bit (0). */
	for (i = 0; i < 18; i++)
	{
		/* Wait until ADB line goes low. */
		if (ADBWait(0) == ADB_TIMEOUT)
		{
			/* Timeout waiting for line to go low. Set ADB line to be an output
			 * and return 0 to indicate that a timeout occurred. */
			SetPortPinDirection(ADB_PORT, ADB_PIN, 1);
			return 0;
		}
		/* Wait until ADB line goes high. */
		LowDuration[i] = ADBWait(1);
		if (LowDuration[i] == ADB_TIMEOUT)
		{
			/* Timeout waiting for line to go high. Set ADB line to be an output
			 * and return 0 to indicate that a timeout occurred. */
			SetPortPinDirection(ADB_PORT, ADB_PIN, 1);
			return 0;
		}
	}
	/* Restore ADB line back to being an output. */
	SetPortPinDirection(ADB_PORT, ADB_PIN, 1);
	/* Examine timing measurements to determine whether bits were 0 or 1. */
	RegisterValue = 0;
	for (i = 0; i < 16; i++)
	{
		RegisterValue <<= 1;
		/* Need a " + 1" here to skip over the start bit */
		if (LowDuration[i + 1] < ADB_THRESHOLD)
			RegisterValue |= 0x01;
	}
	*OutRegisterValue = RegisterValue;
	return 1; /* 1 = success */
}

/** Initialize/reset ADB mouse hardware. */
void ADBMouseInit(void)
{
	/* Set ADB line to output mode, defaulting to a high state. */
	SetPortPinDirection(ADB_PORT, ADB_PIN, 1);
	WritePortPin(ADB_PORT, ADB_PIN, 1);
	/* Give ADB controller time to start up. */
	_delay_ms(10);

	/* Reset trackball controller by holding ADB line low for at least 3 ms. */
	WritePortPin(ADB_PORT, ADB_PIN, 0);
	_delay_ms(4);
	WritePortPin(ADB_PORT, ADB_PIN, 1);
}

/** Poll ADB-connected mouse for updates to its state. This will update
 *  Button1State, Button2State, AccumulatedX, and AccumulatedY accordingly.
 *  \return uint8_t 1 if mouse reported something (a change in its state),
 *                  0 if mouse did not report anything. A mouse has two
 *                  reasons for not reporting anything: the mouse state might
 *                  not have physically changed, or the controller is not
 *                  ready to be polled.
 */
uint8_t ADBPollMouse(void)
{
	uint16_t RegisterValue;
	uint8_t X, Y;
	uint8_t valid;

	GlobalInterruptDisable();
	/* Command 0x3c = 0b00111100:
	 * 0011 = address, which is 3 - the default for mice,
	 * 11 = command type, which is 3 - talk (i.e. read register),
	 * 00 = register, which is 0, where classic Apple mice store button/pointer information. */
	ADBWriteCommand(0x3c);
	/* If the mouse has nothing to report (because nothing has changed), the
	 * attempt to read the register will fail due to timeout. If a timeout
	 * occurs, then we don't do anything. */
	valid = ADBRead16(&RegisterValue);
	GlobalInterruptEnable();
	if (valid)
	{
		/* Parse the register value. See https://developer.apple.com/legacy/library/technotes/hw/hw_01.html
		 * section "Classic Apple Mouse Protocol" for more information. */
		Button1State = 0;
		Button2State = 0;
		if ((RegisterValue & 0x8000) == 0)
		{
			/* First button pressed. */
			Button1State = 1;
		}
		if ((RegisterValue & 0x0080) == 0)
		{
			/* Second button pressed. */
			Button2State = 1;
		}
		/* Accumulate X/Y values into AccumulatedX and AccumulatedY. */
		X = (uint8_t)(RegisterValue & 0x007f);
		Y = (uint8_t)((RegisterValue & 0x7f00) >> 8);
		if (X < 0x40)
		{
			/* Positive X. */
			AccumulatedX += X;
			if (AccumulatedX > ACCUMULATED_MAX)
				AccumulatedX = ACCUMULATED_MAX;
		}
		else
		{
			/* Negative X. */
			AccumulatedX += (int16_t)X - (int16_t)0x80;
			if (AccumulatedX < ACCUMULATED_MIN)
				AccumulatedX = ACCUMULATED_MIN;
		}
		if (Y < 0x40)
		{
			/* Positive Y. */
			AccumulatedY += Y;
			if (AccumulatedY > ACCUMULATED_MAX)
				AccumulatedY = ACCUMULATED_MAX;
		}
		else
		{
			/* Negative Y. */
			AccumulatedY += (int16_t)Y - (int16_t)0x80;
			if (AccumulatedY < ACCUMULATED_MIN)
				AccumulatedY = ACCUMULATED_MIN;
		}
	}
	return valid;
}
