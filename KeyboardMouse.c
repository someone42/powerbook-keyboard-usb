/*
             LUFA Library
     Copyright (C) Dean Camera, 2017.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2017  Dean Camera (dean [at] fourwalledcubicle [dot] com)
  Copyright 2010  Denver Gingerich (denver [at] ossguy [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the KeyboardMouse demo. This file contains the main tasks of the demo and
 *  is responsible for the initial application hardware configuration.
 */

#include <stdint.h>
#include <util/delay.h>
#include "KeyboardMouse.h"
#include "ADBMouse.h"
#include "KeyboardSwitchMatrix.h"
#include "Util.h"

/** Maximum number of non-modifier keys that can be pressed at once. This
 * is a limitation of the USB keyboard boot protocol, don't change this
 * unless you know what you're doing. */
#define MAX_KEYS_PRESSED		6

/** Global structure to hold the current keyboard interface HID report, for transmission to the host */
static USB_KeyboardReport_Data_t KeyboardReportData;

/** Global structure to hold the current mouse interface HID report, for transmission to the host */
static USB_MouseReport_Data_t MouseReportData;

/** This is used to stop the keyboard switch matrix from being polled/scanned too often. */
static uint8_t KeyboardSuppressPolling;

/** Main program entry point. This routine configures the hardware required by the application, then
 *  enters a loop to run the application tasks in sequence.
 */
int main(void)
{
	SetupHardware();

	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
	GlobalInterruptEnable();

	for (;;)
	{
		Keyboard_HID_Task();
		Mouse_HID_Task();
		USB_USBTask();
	}
}



/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
#if (ARCH == ARCH_AVR8)
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);
#elif (ARCH == ARCH_XMEGA)
	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
	XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
	XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
#endif

	/* Hardware Initialization */

	// Set up 16 bit Timer1 to count up to 0xffff, as a timing reference.
	TCCR1A = 0x00; // normal counting mode (just count up to 0xffff)
	TCCR1B = (1 << CS01); // clock source = clkIO / 8 = 2 MHz
	TCCR1C = 0x00; // no force output compare
	
	KeyboardInit();
	
	ADBMouseInit();

	USB_Init();
}

/** Event handler for the USB_Connect event. This indicates that the device is enumerating via the status LEDs and
 *  starts the library USB task to begin the enumeration and USB management process.
 */
void EVENT_USB_Device_Connect(void)
{
	/* Indicate USB enumerating */
	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the USB_Disconnect event. This indicates that the device is no longer connected to a host via
 *  the status LEDs and stops the USB management task.
 */
void EVENT_USB_Device_Disconnect(void)
{
	/* Indicate USB not ready */
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host sets the current configuration
 *  of the USB device after enumeration, and configures the keyboard and mouse device endpoints.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	/* Setup Keyboard HID Report Endpoints */
	ConfigSuccess &= Endpoint_ConfigureEndpoint(KEYBOARD_IN_EPADDR, EP_TYPE_INTERRUPT, HID_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(KEYBOARD_OUT_EPADDR, EP_TYPE_INTERRUPT, HID_EPSIZE, 1);

	/* Setup Mouse HID Report Endpoint */
	ConfigSuccess &= Endpoint_ConfigureEndpoint(MOUSE_IN_EPADDR, EP_TYPE_INTERRUPT, HID_EPSIZE, 1);

	/* Indicate endpoint configuration success or failure */
	LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the USB_ControlRequest event. This is used to catch and process control requests sent to
 *  the device from the USB host before passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest(void)
{
	uint8_t* ReportData;
	uint8_t  ReportSize;

	/* Handle HID Class specific requests */
	switch (USB_ControlRequest.bRequest)
	{
		case HID_REQ_GetReport:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				/* Determine if it is the mouse or the keyboard data that is being requested */
				if (!(USB_ControlRequest.wIndex))
				{
					ReportData = (uint8_t*)&KeyboardReportData;
					ReportSize = sizeof(KeyboardReportData);
				}
				else
				{
					ReportData = (uint8_t*)&MouseReportData;
					ReportSize = sizeof(MouseReportData);
				}

				/* Write the report data to the control endpoint */
				Endpoint_Write_Control_Stream_LE(ReportData, ReportSize);
				Endpoint_ClearOUT();

				/* Clear the report data afterwards */
				memset(ReportData, 0, ReportSize);
			}

			break;
		case HID_REQ_SetReport:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				/* Wait until the LED report has been sent by the host */
				while (!(Endpoint_IsOUTReceived()))
				{
					if (USB_DeviceState == DEVICE_STATE_Unattached)
					  return;
				}

				/* Read in the LED report from the host */
				uint8_t LEDStatus = Endpoint_Read_8();

				Endpoint_ClearOUT();
				Endpoint_ClearStatusStage();

				/* Process the incoming LED report */
				Keyboard_ProcessLEDReport(LEDStatus);
			}

			break;
	}
}

/** Processes a given Keyboard LED report from the host, and sets the board LEDs to match. Since the Keyboard
 *  LED report can be sent through either the control endpoint (via a HID SetReport request) or the HID OUT
 *  endpoint, the processing code is placed here to avoid duplicating it and potentially having different
 *  behavior depending on the method used to sent it.
 */
void Keyboard_ProcessLEDReport(const uint8_t LEDStatus)
{
	/* HID keyboards should only turn on num/caps/scroll lock LEDs when the
	 * host says so. This callback is used to update the state of
	 * num/caps/scroll lock LEDs.
	 * But the powerbook keyboard has no LEDs. So we don't do anything here. */
}

/** Keyboard task. This generates the next keyboard HID report for the host, and transmits it via the
 *  keyboard IN endpoint when the host is ready for more data. Additionally, it processes host LED status
 *  reports sent to the device via the keyboard OUT reporting endpoint.
 */
void Keyboard_HID_Task(void)
{
	uint8_t UsedKeyCodes = 0; /* current number of scan codes in report */
	uint16_t ScanCode; /* needs to be uint16_t so that we can loop over all 256 scan codes */
	uint8_t i;

	/* Device must be connected and configured for the task to run */
	if (USB_DeviceState != DEVICE_STATE_Configured)
		return;
	
	if (KeyboardSuppressPolling == 0)
	{
		KeyboardScanMatrix();

		/* Build the report */
		memset(&KeyboardReportData, 0, sizeof(KeyboardReportData));
		for (ScanCode = 1; ScanCode < 256; ScanCode++)
		{
			if (KeyPressed[ScanCode])
			{
				/* Check if it is a modifier key. If it is a modifier key, it
				 * doesn't go into the KeyCode part of the report - it goes in
				 * the Modifier bitfield. */
				if (ScanCode == HID_KEYBOARD_SC_LEFT_CONTROL)
					KeyboardReportData.Modifier |= HID_KEYBOARD_MODIFIER_LEFTCTRL;
				else if (ScanCode == HID_KEYBOARD_SC_LEFT_SHIFT)
					KeyboardReportData.Modifier |= HID_KEYBOARD_MODIFIER_LEFTSHIFT;
				else if (ScanCode == HID_KEYBOARD_SC_LEFT_ALT)
					KeyboardReportData.Modifier |= HID_KEYBOARD_MODIFIER_LEFTALT;
				else if (ScanCode == HID_KEYBOARD_SC_LEFT_GUI)
					KeyboardReportData.Modifier |= HID_KEYBOARD_MODIFIER_LEFTGUI;
				else if (ScanCode == HID_KEYBOARD_SC_RIGHT_CONTROL)
					KeyboardReportData.Modifier |= HID_KEYBOARD_MODIFIER_RIGHTCTRL;
				else if (ScanCode == HID_KEYBOARD_SC_RIGHT_SHIFT)
					KeyboardReportData.Modifier |= HID_KEYBOARD_MODIFIER_RIGHTSHIFT;
				else if (ScanCode == HID_KEYBOARD_SC_RIGHT_ALT)
					KeyboardReportData.Modifier |= HID_KEYBOARD_MODIFIER_RIGHTALT;
				else if (ScanCode == HID_KEYBOARD_SC_RIGHT_GUI)
					KeyboardReportData.Modifier |= HID_KEYBOARD_MODIFIER_RIGHTGUI;
				else
				{
					/* Not a modifier key:
					 * Put up to MAX_KEYS_PRESSED scan codes into the report */
					if (UsedKeyCodes < MAX_KEYS_PRESSED)
					{
						KeyboardReportData.KeyCode[UsedKeyCodes++] = ScanCode;
					}
					else
					{
						/* Too many keys being pressed simultaneously. HID specification says
						 * that all scan codes must be HID_KEYBOARD_SC_ERROR_ROLLOVER. */
						for (i = 0; i < 6; i++)
						{
							KeyboardReportData.KeyCode[i] = HID_KEYBOARD_SC_ERROR_ROLLOVER;
						}
						break;
					}
				}
			}
		}
		
		/* Only scan keyboard once per HID report. */
		KeyboardSuppressPolling = 1;
	}

	/* Select the Keyboard Report Endpoint */
	Endpoint_SelectEndpoint(KEYBOARD_IN_EPADDR);

	/* Check if Keyboard Endpoint Ready for Read/Write */
	if (Endpoint_IsReadWriteAllowed())
	{
		/* Reset KeyboardSuppressPolling to resume scanning keyboard matrix. */
		KeyboardSuppressPolling = 0;
		
		/* Write Keyboard Report Data */
		Endpoint_Write_Stream_LE(&KeyboardReportData, sizeof(KeyboardReportData), NULL);

		/* Finalize the stream transfer to send the last packet */
		Endpoint_ClearIN();
	}

	/* Select the Keyboard LED Report Endpoint */
	Endpoint_SelectEndpoint(KEYBOARD_OUT_EPADDR);

	/* Check if Keyboard LED Endpoint Ready for Read/Write */
	if (Endpoint_IsReadWriteAllowed())
	{
		/* Read in and process the LED report from the host */
		Keyboard_ProcessLEDReport(Endpoint_Read_8());

		/* Handshake the OUT Endpoint - clear endpoint and ready for next report */
		Endpoint_ClearOUT();
	}
}

/** Mouse task. This generates the next mouse HID report for the host, and transmits it via the
 *  mouse IN endpoint when the host is ready for more data.
 */
void Mouse_HID_Task(void)
{
	/* Device must be connected and configured for the task to run */
	if (USB_DeviceState != DEVICE_STATE_Configured)
		return;
	
	/* The trackball seems to respond most smoothly if it is continuously polled,
	 * as opposed to only polling once per report. */
	ADBPollMouse();

	/* Select the Mouse Report Endpoint */
	Endpoint_SelectEndpoint(MOUSE_IN_EPADDR);

	/* Check if Mouse Endpoint Ready for Read/Write */
	if (Endpoint_IsReadWriteAllowed())
	{
		/* Build the Mouse Report. */
		memset(&MouseReportData, 0, sizeof(MouseReportData));
		MouseReportData.Button = Button1State | (Button2State << 1);
		MouseReportData.X = (int8_t)AccumulatedX;
		MouseReportData.Y = (int8_t)AccumulatedY;
		/* Reset AccumulatedX/AccumulatedY so that ADBPollMouse() will begin
		 * accumulating from 0 again. */
		AccumulatedX = 0;
		AccumulatedY = 0;
		
		/* Write Mouse Report Data */
		Endpoint_Write_Stream_LE(&MouseReportData, sizeof(MouseReportData), NULL);

		/* Finalize the stream transfer to send the last packet */
		Endpoint_ClearIN();
	}
}

