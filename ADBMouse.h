/** \file
 *
 *  Defines things exported by ADBMouse.c
 *
 *  This file is licensed as described by the file BSD.txt
 */

#ifndef _ADB_MOUSE_H_
#define _ADB_MOUSE_H_

#include <stdint.h>

/* Exported Variables: */
extern int16_t AccumulatedX;
extern int16_t AccumulatedY;
extern uint8_t Button1State;
extern uint8_t Button2State;

/* Function Prototypes: */
extern void ADBMouseInit(void);
extern uint8_t ADBPollMouse(void);

#endif // #ifndef _ADB_MOUSE_H_
