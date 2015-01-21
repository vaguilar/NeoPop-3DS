#include <3ds.h>
#include "font.h"

void drawPixel(u8* buffer, int x, int y)
{
	u32 v = ((-y-1)+(x)*240) * 3;
	buffer[v++] = 0xff;
	buffer[v++] = 0xff;
	buffer[v++] = 0xff;
}

void drawChar(u8* buffer, int x, int y, int w, int h, int ascii)
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
				drawPixel(buffer, x+i, y+j);
			}
		}
	}
}

void drawString(u8* buffer, char* str, int x, int y, int char_w, int char_h)
{
	while (*str) {
		drawChar(buffer, x, y, char_w, char_h, *str);
		x += char_w;
		str++;
	}
}
