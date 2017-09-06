/** \file
 *
 *  Defines things exported by KeyboardSwitchMatrix.c
 *
 *  This file is licensed as described by the file BSD.txt
 */

#ifndef _KEYBOARD_SWITCH_MATRIX_H_
#define _KEYBOARD_SWITCH_MATRIX_H_

/* Exported Variables: */
extern uint8_t KeyPressed[256];

/* Function Prototypes: */
extern void KeyboardInit(void);
extern void KeyboardScanMatrix(void);

#endif // #ifndef _KEYBOARD_SWITCH_MATRIX_H_
