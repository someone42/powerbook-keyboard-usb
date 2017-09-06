/** \file
 *
 *  This file contains code which scans the keyboard switch matrix.
 *  In a nutshell, this assumes the keyboard consists of a bunch of row lines,
 *  and a bunch of column lines, with each key attached to a row and a column
 *  line. Pressing a key connects its row and column lines. Thus the keyboard
 *  switch matrix can be scanned by applying a voltage to a row line, and
 *  checking if that voltage appears in any column lines.
 *  Rows and column lines may not correspond to physical rows and columns.
 *
 *  To use this code, call KeyboardInit() once, then call KeyboardScanMatrix()
 *  periodically. KeyPressed will be updated with key states.
 *
 *  This file is licensed as described by the file BSD.txt
 */

#include <stdint.h>
#include <util/delay.h>
#include <LUFA/Drivers/USB/USB.h>
#include "KeyboardSwitchMatrix.h"
#include "Util.h"

/** Number of rows in keyboard switch matrix. This doesn't necessarily
 *  correspond to the actual number of physical rows. */
#define MATRIX_ROWS				8
/** Number of columns in keyboard switch matrix. This doesn't necessarily
 *  correspond to the actual number of physical columns. */
#define MATRIX_COLUMNS			16

/** Number of rows to scan per HID report. This will determine how quickly
 *  key presses/releases will be detected. See the HID descriptor for the
 *  HID report interval (in milliseconds). Setting this too low will cause
 *  the keyboard to respond slowly. Setting this too high may cause key
 *  bounce issues. */
#define ROWS_PER_REPORT			2

/** This is used to unambiguously specify a connection to an external pin. */
struct GPIOPin
{
	uint8_t port; /* 0 = PORTA, 1 = PORTB, 2 = PORTC etc. */
	uint8_t num; /* 0 = PA0, PB0, PC1 etc., 1 = PA1, PB1, PC1 etc. */
};

/** Which external pins the rows are connected to. */
const struct GPIOPin RowPins[MATRIX_ROWS] = {
	{2, 0}, /* PC0 */
	{2, 1}, /* PC1 */
	{2, 2}, /* PC2 */
	{2, 3}, /* PC3 */
	{2, 4}, /* PC4 */
	{2, 5}, /* PC5 */
	{2, 6}, /* PC6 */
	{2, 7}  /* PC7 */
};

/** Which external pins the columns are connected to. */
const struct GPIOPin ColumnPins[MATRIX_COLUMNS] = {
	{1, 5}, /* PB5 */
	{1, 4}, /* PB4 */
	{1, 3}, /* PB3 */
	{1, 2}, /* PB2 */
	{1, 1}, /* PB1 */
	{1, 0}, /* PB0 */
	{4, 7}, /* PE7 */
	{4, 6}, /* PE6 */
	{5, 0}, /* PF0 */
	{5, 1}, /* PF1 */
	{5, 2}, /* PF2 */
	{5, 3}, /* PF3 */
	{5, 4}, /* PF4 */
	{5, 5}, /* PF5 */
	{5, 6}, /* PF6 */
	{5, 7}  /* PF7 */
};

/** Keyboard switch matrix that describes which switches connect a given
 *  row/column. For example, if the driver detects that row 2 is connected
 *  to column 5, then KeyboardMatrix[2][5] describes the key that was
 *  pressed.
 *  The values are HID keyboard standard report scan codes. 0x00 represents
 *  no key at that location.
 */
static const uint8_t KeyboardMatrix[MATRIX_ROWS][MATRIX_COLUMNS] = {
	// Row 1
	{0x00, HID_KEYBOARD_SC_EQUAL_AND_PLUS, HID_KEYBOARD_SC_5_AND_PERCENTAGE, HID_KEYBOARD_SC_4_AND_DOLLAR,
	0x00, 0x00, 0x00, 0x00,
	0x00, HID_KEYBOARD_SC_LEFT_GUI, HID_KEYBOARD_SC_CAPS_LOCK, HID_KEYBOARD_SC_ESCAPE,
	HID_KEYBOARD_SC_LEFT_SHIFT, HID_KEYBOARD_SC_LEFT_ALT, HID_KEYBOARD_SC_LEFT_CONTROL, HID_KEYBOARD_SC_6_AND_CARET},
	// Row 2
	{HID_KEYBOARD_SC_U, HID_KEYBOARD_SC_RETURN, HID_KEYBOARD_SC_SEMICOLON_AND_COLON, HID_KEYBOARD_SC_L,
	HID_KEYBOARD_SC_RIGHT_ARROW, HID_KEYBOARD_SC_D, HID_KEYBOARD_SC_UP_ARROW, 0x00,
	HID_KEYBOARD_SC_LEFT_ARROW, HID_KEYBOARD_SC_LEFT_GUI, HID_KEYBOARD_SC_CAPS_LOCK, HID_KEYBOARD_SC_BACKSPACE,
	HID_KEYBOARD_SC_LEFT_SHIFT, HID_KEYBOARD_SC_LEFT_ALT, HID_KEYBOARD_SC_LEFT_CONTROL, HID_KEYBOARD_SC_APOSTROPHE_AND_QUOTE},
	// Row 3
	{0x00, HID_KEYBOARD_SC_O, HID_KEYBOARD_SC_OPENING_BRACKET_AND_OPENING_BRACE, HID_KEYBOARD_SC_BACKSLASH_AND_PIPE,
	0x00, 0x00, 0x00, 0x00,
	0x00, HID_KEYBOARD_SC_LEFT_GUI, HID_KEYBOARD_SC_CAPS_LOCK, HID_KEYBOARD_SC_3_AND_HASHMARK,
	HID_KEYBOARD_SC_LEFT_SHIFT, HID_KEYBOARD_SC_LEFT_ALT, HID_KEYBOARD_SC_LEFT_CONTROL, HID_KEYBOARD_SC_9_AND_OPENING_PARENTHESIS},
	// Row 4
	{HID_KEYBOARD_SC_B, HID_KEYBOARD_SC_DOT_AND_GREATER_THAN_SIGN, HID_KEYBOARD_SC_COMMA_AND_LESS_THAN_SIGN, HID_KEYBOARD_SC_J,
	HID_KEYBOARD_SC_F, 0x00, HID_KEYBOARD_SC_DOWN_ARROW, HID_KEYBOARD_SC_S,
	HID_KEYBOARD_SC_A, HID_KEYBOARD_SC_LEFT_GUI, HID_KEYBOARD_SC_CAPS_LOCK, HID_KEYBOARD_SC_H,
	HID_KEYBOARD_SC_LEFT_SHIFT, HID_KEYBOARD_SC_LEFT_ALT, HID_KEYBOARD_SC_LEFT_CONTROL, HID_KEYBOARD_SC_SLASH_AND_QUESTION_MARK},
	// Row 5
	{HID_KEYBOARD_SC_ENTER, 0x00, HID_KEYBOARD_SC_P, HID_KEYBOARD_SC_K,
	HID_KEYBOARD_SC_R, HID_KEYBOARD_SC_E, HID_KEYBOARD_SC_W, HID_KEYBOARD_SC_Q,
	HID_KEYBOARD_SC_TAB, HID_KEYBOARD_SC_LEFT_GUI, HID_KEYBOARD_SC_CAPS_LOCK, HID_KEYBOARD_SC_I,
	HID_KEYBOARD_SC_LEFT_SHIFT, HID_KEYBOARD_SC_LEFT_ALT, HID_KEYBOARD_SC_LEFT_CONTROL, HID_KEYBOARD_SC_CLOSING_BRACKET_AND_CLOSING_BRACE},
	// Row 6
	{0x00, HID_KEYBOARD_SC_0_AND_CLOSING_PARENTHESIS, HID_KEYBOARD_SC_Y, HID_KEYBOARD_SC_G,
	0x00, 0x00, 0x00, 0x00,
	0x00, HID_KEYBOARD_SC_LEFT_GUI, HID_KEYBOARD_SC_CAPS_LOCK, HID_KEYBOARD_SC_2_AND_AT,
	HID_KEYBOARD_SC_LEFT_SHIFT, HID_KEYBOARD_SC_LEFT_ALT, HID_KEYBOARD_SC_LEFT_CONTROL, HID_KEYBOARD_SC_8_AND_ASTERISK},
	// Row 7
	{0x00, HID_KEYBOARD_SC_MINUS_AND_UNDERSCORE, HID_KEYBOARD_SC_T, HID_KEYBOARD_SC_GRAVE_ACCENT_AND_TILDE,
	0x00, 0x00, 0x00, 0x00,
	0x00, HID_KEYBOARD_SC_LEFT_GUI, HID_KEYBOARD_SC_CAPS_LOCK, HID_KEYBOARD_SC_1_AND_EXCLAMATION,
	HID_KEYBOARD_SC_LEFT_SHIFT, HID_KEYBOARD_SC_LEFT_ALT, HID_KEYBOARD_SC_LEFT_CONTROL, HID_KEYBOARD_SC_7_AND_AMPERSAND},
	// Row 8
	{HID_KEYBOARD_SC_SPACE, 0x00, HID_KEYBOARD_SC_M, HID_KEYBOARD_SC_N,
	HID_KEYBOARD_SC_V, HID_KEYBOARD_SC_C, HID_KEYBOARD_SC_X, HID_KEYBOARD_SC_Z,
	0x00, HID_KEYBOARD_SC_LEFT_GUI, HID_KEYBOARD_SC_CAPS_LOCK, 0x00,
	HID_KEYBOARD_SC_LEFT_SHIFT, HID_KEYBOARD_SC_LEFT_ALT, HID_KEYBOARD_SC_LEFT_CONTROL, 0x00}
};

/** Some of the switches have diodes in them, which makes them immune to
 *  ghosting. This macro is used to suppress ghost detection for certain
 *  columns. 0 = first column, 1 = second column etc.
 *  This is currently set to cover all the keys which are connected to all
 *  rows: GUI, Caps Lock, Shift, Alt/Option, and Control. Ghost detection
 *  must be suppressed for those keys, otherwise ghosting will occur whenever
 *  another key is pressed simultaneously. */
#define IS_GHOST_FREE_COLUMN(x)		(((x) == 9) || ((x) == 10) || ((x) == 12) || ((x) == 13) || ((x) == 14))

/** Raw keyboard matrix state, keeping track of which switches in the keyboard
 *  matrix are currently pressed. This is "raw" in the sense that de-ghosting
 *  hasn't been applied yet. */
static uint8_t RawSwitchPressed[MATRIX_ROWS][MATRIX_COLUMNS];
/** Total number of raw switch presses in each row. */
static uint8_t TotalInRow[MATRIX_ROWS];
/** Total number of raw switch presses in each column. */
static uint8_t TotalInColumn[MATRIX_COLUMNS];
/** Whether a row has a ghost. 0 = no ghost, 1 = has ghost. If a row has a
 *  ghost then presses in that row will be ignored. */
static uint8_t RowHasGhost[MATRIX_ROWS];
/** Whether a column has a ghost. 0 = no ghost, 1 = has ghost. If a column has
 *  a ghost then presses in that column will be ignored. */
static uint8_t ColumnHasGhost[MATRIX_COLUMNS];
/** Keeps track of which keys are pressed (1) or not pressed (0).
 *  This is indexed by (HID keyboard report) scan code. This is the
 *  post-processed version, which should be ghost-free. */
uint8_t KeyPressed[256];
/** Current keyboard matrix row that is being scanned. If no row is being
 *  scanned right now, then this is the next row to be scanned. */
static uint8_t CurrentRow;

/** Initialise hardware which scans keyboard switch matrix. */
void KeyboardInit(void)
{
	uint8_t i;

	/* Set row pins as input, with pull-ups enabled.
	 * When a row is scanned, the appropriate pin will be driven low.
	 * It is important that rows are only pulled up (and not driven high),
	 * otherwise conflicts could arise when two keys in the same column are
	 * pressed simultaneously, creating a short across row pins. */
	for (i = 0; i < MATRIX_ROWS; i++)
	{
		SetPortPinDirection(RowPins[i].port, RowPins[i].num, 0);
	}
	
	/* Set column pins as input, with pull-ups enabled. */
	MCUCR &= ~0x10; /* ensure that PUD (pull-up disable) is clear */
	for (i = 0; i < MATRIX_COLUMNS; i++)
	{
		SetPortPinDirection(ColumnPins[i].port, ColumnPins[i].num, 0);
	}
}

/** Check to see if any current key presses are possibly creating a
 *  ghost situation. A ghost situation is where certain combinations
 *  of simultaneous key presses causes spurious ghost presses to appear.
 *  This will update RowHasGhost and ColumnHasGhost accordingly. */
static void CheckForGhosts(void)
{
	uint8_t Row, Column;
	uint8_t i, j;

	memset(RowHasGhost, 0, sizeof(RowHasGhost));
	memset(ColumnHasGhost, 0, sizeof(ColumnHasGhost));
	for (Row = 0; Row < MATRIX_ROWS; Row++)
	{
		for (Column = 0; Column < MATRIX_COLUMNS; Column++)
		{
			if (IS_GHOST_FREE_COLUMN(Column))
				continue;
			/* Each switch in the matrix is checked to see if it is provoking a ghosting situation.
			 * A ghosting situation is where 3 keys are simultaneously pressed, where one of those
			 * keys (the corner key) shares a row with another key, and the corner key also shares
			 * a column with another key, like in this example ("x" = key press):
			 * -----------------
			 * ---x------x------
			 * -----------------
			 * ----------x------
			 * -----------------
			 * In the above example the top right press is the corner key.
			 * The next line detects corner keys.
			 */
			if (RawSwitchPressed[Row][Column] && (TotalInRow[Row] >= 2) && (TotalInColumn[Column] >= 2))
			{
				/* If a key is provoking a ghosting situation, then suppress all subsequent key
				 * presses in any row or column that also shares a row or column with the
				 * corner key. Using the above example:
				 * SUPPRESS SUPPRESS
				 *    |      |
				 *    v      v
				 * -----------------
				 * ---x------x------   <- SUPPRESS
				 * -----------------
				 * ----------x------   <- SUPPRESS
				 * -----------------
				 */
				for (i = 0; i < MATRIX_ROWS; i++)
				{
					if (RawSwitchPressed[i][Column])
						RowHasGhost[i] = 1;
				}
				for (j = 0; j < MATRIX_COLUMNS; j++)
				{
					if (IS_GHOST_FREE_COLUMN(j))
						continue;
					if (RawSwitchPressed[Row][j])
						ColumnHasGhost[j] = 1;
				}
			}
		}
	}
}

/** Scan some rows of the keyboard switch matrix, detecting pressed or
 *  released keys. This will update KeyPressed accordingly. */
void KeyboardScanMatrix(void)
{
	uint8_t i;
	uint8_t SwitchPressed;
	uint8_t CurrentColumn;
	uint16_t ScanCode; /* needs to be uint16_t so that we can loop over all 256 scan codes */
	uint8_t SwitchChanged;

	/* Scan ROWS_PER_REPORT rows. */
	for (i = 0; i < ROWS_PER_REPORT; i++)
	{
		/* Scan a row.
		 * A row is activated by driving it low */
		SetPortPinDirection(RowPins[CurrentRow].port, RowPins[CurrentRow].num, 1);
		WritePortPin(RowPins[CurrentRow].port, RowPins[CurrentRow].num, 0);
		_delay_us(100); /* let voltages settle */
		for (CurrentColumn = 0; CurrentColumn < MATRIX_COLUMNS; CurrentColumn++)
		{
			/* Check which column pins are reading low - this indicates a key press. */
			/* Update raw keyboard state. */
			SwitchPressed = 0;
			if (ReadPortPin(ColumnPins[CurrentColumn].port, ColumnPins[CurrentColumn].num) == 0)
			{
				/* Key has been pressed */
				SwitchPressed = 1;
			}
			else if (ScanCode != 0x00) /* ignore row/column combinations with no switch */
			{
				/* Key has been released */
				SwitchPressed = 0;
			}
			SwitchChanged = 0;
			if (!RawSwitchPressed[CurrentRow][CurrentColumn] && SwitchPressed)
			{
				/* Transition from unpressed -> pressed state. */
				TotalInRow[CurrentRow]++;
				TotalInColumn[CurrentColumn]++;
				SwitchChanged = 1;
			}
			if (RawSwitchPressed[CurrentRow][CurrentColumn] && !SwitchPressed)
			{
				/* Transition from pressed -> unpressed state. */
				TotalInRow[CurrentRow]--;
				TotalInColumn[CurrentColumn]--;
				SwitchChanged = 1;
			}
			RawSwitchPressed[CurrentRow][CurrentColumn] = SwitchPressed;
			/* Only check for ghosts if a switch state changed, otherwise the
			 * row scan takes too long and can lag. */
			if (SwitchChanged)
				CheckForGhosts();
			/* Update post-processed keyboard state. */
			ScanCode = KeyboardMatrix[CurrentRow][CurrentColumn];
			if (!SwitchPressed ||
				(!RowHasGhost[CurrentRow] && !ColumnHasGhost[CurrentColumn]))
			{
				KeyPressed[ScanCode] = SwitchPressed;
			}
		}
		/* Deactivate row by driving it high (so that voltages settle quickly),
		 * then returning it back into the pulled-up state. */
		WritePortPin(RowPins[CurrentRow].port, RowPins[CurrentRow].num, 1);
		_delay_us(20); /* let voltages settle */
		SetPortPinDirection(RowPins[CurrentRow].port, RowPins[CurrentRow].num, 0);
		CurrentRow++;
		if (CurrentRow >= MATRIX_ROWS)
		{
			CurrentRow = 0;
		}
	}
}
