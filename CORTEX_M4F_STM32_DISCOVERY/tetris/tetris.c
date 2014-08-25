#include "stm32f429i_discovery.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_ioe.h"

#define LCD_COLOR_GRAY	0xC618

static void LCDInit(void)
{
	LCD_Init();
	IOE_Config();
	LTDC_Cmd( ENABLE );
	LCD_LayerInit();
	LCD_SetLayer( LCD_FOREGROUND_LAYER );
	LCD_Clear( LCD_COLOR_BLACK );
	LCD_SetTextColor( LCD_COLOR_WHITE );
}

/* DrawBlock
 *   Screen size = 240 * 320
 *   Block size = 20 * 20
 *   Total block = 12 * 16
 *   Block index x from 0 to 11
 *   Block index y from 0 to 15
 */
static void DrawBlock(uint16_t x, uint16_t y, uint16_t color)
{
	LCD_SetColors(LCD_COLOR_WHITE, LCD_COLOR_BLACK);
	LCD_DrawFullRect(x * 20, 20 * y, 20, 20);
	LCD_SetColors(color, LCD_COLOR_BLACK);
	LCD_DrawFullRect(x * 20 + 1, 20 * y + 1, 18, 18);
}

/* FieldInit
 *   Surrounding block initilize
 *      0123456789AB
 *    0 X          X
 *    1 X          X
 *    2 X          X
 *    3 X          X
 *    4 X          X
 *    5 X          X
 *    6 X          X
 *    7 X          X
 *    8 X          X
 *    9 X          X
 *   10 X          X
 *   11 X          X
 *   12 X          X
 *   13 X          X
 *   14 X          X
 *   15 XXXXXXXXXXXX  X:Block
 */
static void FieldInit(void)
{
	for(int i = 0; i < 15; i++) {
		DrawBlock(0, i, LCD_COLOR_GRAY);
		DrawBlock(11, i, LCD_COLOR_GRAY);

		if(i < 12) {
			DrawBlock(i, 15, LCD_COLOR_GRAY);
		}
	}
}

void TetrisInit(void)
{
	LCDInit();
	FieldInit();
}
