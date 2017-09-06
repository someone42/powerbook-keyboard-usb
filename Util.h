/** \file
 *
 *  Defines things exported by Util.c
 *
 *  This file is licensed as described by the file BSD.txt
 */

#ifndef _KEYBOARD_MOUSE_UTIL_H_
#define _KEYBOARD_MOUSE_UTIL_H_

#include <stdint.h>

/* Function Prototypes: */
extern void SetPortPinDirection(const uint8_t Port, const uint8_t Num, const uint8_t IsOutput);
extern void WritePortPin(const uint8_t Port, const uint8_t Num, const uint8_t Val);
extern uint8_t ReadPortPin(const uint8_t Port, const uint8_t Num);
extern void DelayMicroseconds(uint16_t MicroSeconds);

#endif // #ifndef _KEYBOARD_MOUSE_UTIL_H_
