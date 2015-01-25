#include <3ds.h>
#include "font.h"
#include "NeoPop-SDL.h"

void drawPixel(u8* buffer, int x, int y, int color)
{
	int xx = x + 1, yy = y + 1;
	u32 v = ((xx * 240) - yy) * 3;
	buffer[v++] = (color >> 24) & 0xff; // R
	buffer[v++] = (color >> 16) & 0xff; // G
	buffer[v++] = (color >>  8) & 0xff; // B
}

void drawChar(u8* buffer, int x, int y, int w, int h, int ascii, int color)
{
	if (ascii < 32 || ascii > 126) return;

	unsigned char* bitmap = letters[ascii - 32];
	u16 i, j, mask;
	for (i = 0; i < w; i++)
	{
		mask = 1 << (w - i - 1);
		for (j = 0; j < h; j++)
		{
			if (mask & bitmap[h - j - 1])
			{
				drawPixel(buffer, x+i, y+j, color);
			}
		}
	}
}

void drawString(u8* buffer, char* str, int x, int y, int color)
{
	while (*str) {
		drawChar(buffer, x, y, CHAR_WIDTH, CHAR_HEIGHT, *str, color);
		x += CHAR_WIDTH;
		str++;
	}
}
