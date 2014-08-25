#include <string.h>

#include "stm32f429i_discovery.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_ioe.h"

#define LCD_COLOR_GRAY		0xC618
#define LCD_COLOR_ORANGE	0xFC00

typedef struct BLOCK_T {
	uint16_t x;
	uint16_t y;
	uint16_t type;
	uint16_t direction;
} BLOCK_T;

enum BLOCK_TYPE {
	BLOCK = -1,
	EMPTY,
	TYPE_I = 1,
	TYPE_J,
	TYPE_L,
	TYPE_O,
	TYPE_S,
	TYPE_T,
	TYPE_Z
};

static const uint16_t
block_color[8] = {0,
                  LCD_COLOR_CYAN,
                  LCD_COLOR_BLUE,
                  LCD_COLOR_ORANGE,
                  LCD_COLOR_YELLOW,
                  LCD_COLOR_GREEN,
                  LCD_COLOR_MAGENTA,
                  LCD_COLOR_RED};
/* Block Type
 *   Type_I:              Type_J:
 *   0000 0100 0000 0010  0010 0000 0000 0000
 *   1111 0100 0000 0010  0010 0100 0110 1110
 *   0000 0100 1111 0010  0110 0111 0100 0010
 *   0000 0100 0000 0010  0000 0000 0100 0000
 *
 *   Type_L:              Type_O:
 *   0100 0000 0000 0000  0000 0000 0000 0000
 *   0100 0111 0110 0010  0110 0110 0110 0110
 *   0110 0100 0010 1110  0110 0110 0110 0110
 *   0000 0000 0010 0000  0000 0000 0000 0000
 *
 *   Type_S:              Type_T:
 *   0000 0000 0000 0000  0000 0000 0000 0000
 *   0110 0100 0110 0100  0111 0010 0100 0100
 *   1100 0110 1100 0110  0010 0110 1110 0110
 *   0000 0010 0000 0010  0000 0010 0000 0100
 *
 *   Type_Z:
 *   0000 0000 0000 0000
 *   0110 0010 0110 0010
 *   0011 0110 0011 0110
 *   0000 0100 0000 0100
 */
static const uint16_t
block_type[8][4] = {{0, 0, 0, 0},
                    {0x0F00, 0x4444, 0x00F0, 0x2222},
                    {0x2260, 0x0470, 0x0644, 0x0E20},
                    {0x4460, 0x0740, 0x0622, 0x02E0},
                    {0x0660, 0x0660, 0x0660, 0x0660},
                    {0x06C0, 0x0462, 0x06C0, 0x0462},
                    {0x0720, 0x0262, 0x04E0, 0x0464},
                    {0x0630, 0x0264, 0x0630, 0x0264}};

static int field[16][12] = {0};

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

/* BlockDraw
 *   Screen size = 240 * 320
 *   Block size = 20 * 20
 *   Total block = 12 * 16
 *   Block index x from 0 to 11
 *   Block index y from 0 to 15
 */
static void BlockDraw(uint16_t y, uint16_t x, uint16_t color)
{
	LCD_SetColors(LCD_COLOR_WHITE, LCD_COLOR_BLACK);
	LCD_DrawFullRect(x * 20, y * 20, 20, 20);
	LCD_SetColors(color, LCD_COLOR_BLACK);
	LCD_DrawFullRect(x * 20 + 1, y * 20 + 1, 18, 18);
}

static void BlockErase(uint16_t y, uint16_t x)
{
	LCD_SetColors(LCD_COLOR_BLACK, LCD_COLOR_BLACK);
	LCD_DrawFullRect(x * 20, y * 20, 20, 200);
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
		BlockDraw(i, 0, LCD_COLOR_GRAY);
		BlockDraw(i, 11, LCD_COLOR_GRAY);
		field[i][0] = BLOCK;
		field[i][11] = BLOCK;

		if(i < 12) {
			BlockDraw(15, i, LCD_COLOR_GRAY);
			field[15][i] = BLOCK;
		}
	}
}

static void BlockAdd(BLOCK_T block)
{
	uint16_t mask = 0x8000;
	uint16_t y = block.y;
	uint16_t x = block.x;
	uint16_t type = block.type;
	uint16_t direction = block.direction;

	for(int i = 0; i < 4; i++) {
		for(int j = 0; j < 4; j++) {
			if(block_type[type][direction] & mask) {
				field[y + i][x + j] = type;
			}
			mask >>= 1;
		}
	}
}

static void BlockRemove(BLOCK_T block)
{
	uint16_t mask = 0x8000;
	uint16_t y = block.y;
	uint16_t x = block.x;
	uint16_t type = block.type;
	uint16_t direction = block.direction;

	for(int i = 0; i < 4; i++) {
		for(int j = 0; j < 4; j++) {
			if(block_type[type][direction] & mask) {
				field[y + i][x + j] = EMPTY;
			}
			mask >>= 1;
		}
	}
}

static void BlockNew(BLOCK_T *block)
{
	block->y = 0;
	block->x = 3;
	block->type = TYPE_I;
	block->direction = 0;
}

static void UpdateScreen(void)
{
	for(int i = 0; i <= 14; i++) {
		for(int j = 1; j <= 10; j++) {
			if(field[i][j] == EMPTY) {
				BlockErase(i, j);
			}
			else {
				BlockDraw(i, j, block_color[field[i][j]]);
			}
		}
	}
}

void TetrisInit(void)
{
	LCDInit();
	FieldInit();
}

void TetrisGameLoop(void)
{
	static BLOCK_T cur_block, last_block;
	static falling = 0;

	if(!falling) {
		BlockNew(&cur_block);
		falling = 1;
	}

	BlockRemove(last_block);
	BlockAdd(cur_block);

	UpdateScreen();

	memcpy(&last_block, &cur_block, sizeof(BLOCK_T));
	cur_block.y++;
}
