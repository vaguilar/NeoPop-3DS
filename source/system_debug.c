#include "NeoPop-SDL.h"

_u16 head = 0;
_u16 tail = 0;
_u16 col  = 0;

void system_debug_print(char *msg)
{
	_u32 i;
	for(i = 0; msg[i]; i++) {
		
		if (col >= DEBUG_SCREEN_COLS || msg[i] == '\n') {
			col = 0;
			head = (head + 1) % DEBUG_SCREEN_ROWS;

			if (head == tail)
				tail = (tail + 1) % DEBUG_SCREEN_ROWS;
		}

		if (msg[i] != '\n') {
			debug_buffer[head][col] = msg[i];
			col++;
		}

	}

}

void system_debug_println(char *msg)
{
	system_debug_print(msg);
	system_debug_print("\n");
	col = 0;
}

void system_debug_printf(char *str, ...)
{
	char buffer[256] = {0};
	va_list vl;

	va_start(vl, str);
	vsnprintf (buffer, 255, str, vl);
	va_end(vl);

	system_debug_print(buffer);
}

void system_debug_clear()
{
	_u32 r, c;
	for (r = 0; r < DEBUG_SCREEN_ROWS; r++) {
		for (c = 0; c < DEBUG_SCREEN_COLS; c++) {
			debug_buffer[r][c] = 0;
		}
	}

	head = 0;
	tail = 0;
	col = 0;
}
