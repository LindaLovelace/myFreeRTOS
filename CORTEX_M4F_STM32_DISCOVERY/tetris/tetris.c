#include <string.h>

#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_usart.h"
#include "stm32f4xx_rng.h"

#include "stm32f429i_discovery.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_ioe.h"
#include "stm32f429i_discovery_l3gd20.h"

#define DBG

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

static int field[16][12] = {{0}};
static BLOCK_T cur_block = {0}, last_block = {0};

#ifdef DBG
/** Debug Functions **/
static void dbg_puts(char *s)
{
	while(*s) {
		while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
		USART_SendData(USART1, *s);
		s++;
	}
	while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
	USART_SendData(USART1, '\n');
	while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
	USART_SendData(USART1, '\r');
}

static void USARTInit(void)
{
	/* --------------------------- System Clocks Configuration -----------------*/
	/* USART1 clock enable */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	/* GPIOA clock enable */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	/*-------------------------- GPIO Configuration ----------------------------*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	/* Connect USART pins to AF */
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);   // USART1_TX
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);  // USART1_RX

	USART_InitTypeDef USART_InitStructure;
	/* USARTx configuration ------------------------------------------------------
	*  USARTx configured as follow:
	*  *  - BaudRate = 9600 baud
	*  *  - Word Length = 8 Bits
	*  *  - One Stop Bit
	*  *  - No parity
	*  *  - Hardware flow control disabled (RTS and CTS signals)
	*  *  - Receive and transmit enabled
	*/
	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);
	USART_Cmd(USART1, ENABLE);
}

static char* itoa(int value, char* result, int base)
{
	if (base < 2 || base > 36) {
		*result = '\0';
		return result;
	}
	char *ptr = result, *ptr1 = result, tmp_char;
	int tmp_value;

	do {
		tmp_value = value;
		value /= base;
		*ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
	} while (value);

	if (tmp_value < 0) *ptr++ = '-';
	*ptr-- = '\0';
	while (ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr-- = *ptr1;
		*ptr1++ = tmp_char;
	}
	return result;
}
#endif

/** Block Operations **/
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
	LCD_DrawFullRect(x * 20, y * 20, 20, 20);
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

	if(type == EMPTY) {
		return;
	}

	for(int i = 0; i < 4; i++) {
		for(int j = 0; j < 4; j++) {
			if((block_type[type][direction] & mask)
			   && (field[y + i][x + j] != BLOCK)) {
				field[y + i][x + j] = EMPTY;
			}
			mask >>= 1;
		}
	}
}

static int BlockCorrupt(BLOCK_T block)
{
	uint16_t mask = 0x8000;
	uint16_t y = block.y;
	uint16_t x = block.x;
	uint16_t type = block.type;
	uint16_t direction = block.direction;

	for(int i = 0; i < 4; i++) {
		for(int j = 0; j < 4; j++) {
			if((block_type[type][direction] & mask)
			   && (field[y + i][x + j] != EMPTY)
			   && (field[y + i][x + j] != BLOCK)) {
				return 1;
			}
			mask >>= 1;
		}
	}
	return 0;
}

static int BlockNew(BLOCK_T *block)
{
	uint32_t rnd;

	while(!RNG_GetFlagStatus(RNG_FLAG_DRDY));
	rnd = RNG_GetRandomNumber();

	block->y = 0;
	block->x = 3;
	block->type = rnd % 7 + 1;
	block->direction = 1;

	/* Check corruption with existing block */
	if(BlockCorrupt(*block)) {
		return -1;
	}

	return 0;
}

static int BlockReachBottom(BLOCK_T block)
{
	uint16_t mask = 0x8000;
	uint16_t y = block.y;
	uint16_t x = block.x;
	uint16_t type = block.type;
	uint16_t direction = block.direction;

	for(int i = 0; i < 4; i++) {
		for(int j = 0; j < 4; j++) {
			if((block_type[type][direction] & mask)
			   && !(block_type[type][direction] & mask >> 4
			        && i != 3)
			   && (field[y + i + 1][x + j] != EMPTY)) {
				return 1;
			}

			mask >>= 1;
		}
	}

	return 0;
}

/* Internal Initialize Functions*/
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

/* FieldInit
 *   Surrounding block initialize
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

static void IOEInit(void)
{
	IOE_Config();

	/* TODO: Use interrupt instead of polling */
	//IOE_TPITConfig();
}

static void L3GD20Init(void)
{
	L3GD20_InitTypeDef L3GD20_InitStruct;

	L3GD20_InitStruct.Power_Mode = L3GD20_MODE_ACTIVE;
	L3GD20_InitStruct.Output_DataRate = L3GD20_OUTPUT_DATARATE_1;
	L3GD20_InitStruct.Axes_Enable = L3GD20_AXES_ENABLE;
	L3GD20_InitStruct.Band_Width = L3GD20_BANDWIDTH_4;
	L3GD20_InitStruct.BlockData_Update = L3GD20_BlockDataUpdate_Continous;
	L3GD20_InitStruct.Endianness = L3GD20_BLE_LSB;
	L3GD20_InitStruct.Full_Scale = L3GD20_FULLSCALE_250;

	L3GD20_Init(&L3GD20_InitStruct);

	L3GD20_FilterConfigTypeDef L3GD20_FilterStructure;
	L3GD20_FilterStructure.HighPassFilter_Mode_Selection = L3GD20_HPM_NORMAL_MODE_RES;
	L3GD20_FilterStructure.HighPassFilter_CutOff_Frequency = L3GD20_HPFCF_0;
	L3GD20_FilterConfig(&L3GD20_FilterStructure);
	L3GD20_FilterCmd(L3GD20_HIGHPASSFILTER_ENABLE);
}

static void RNGInit(void)
{
	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);
	RNG_Cmd(ENABLE);
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

static void ClearLine(int y)
{
	for(int i = y; i > 0; i--) {
		for(int j = 1; j <= 10; j++) {
			field[i][j] = field[i - 1][j];
		}
	}

	for(int j = 1; j <= 10; j++) {
		field[0][j] = EMPTY;
	}
}

static void CheckLine(void)
{
	for(int i = 0; i <= 14; i++) {
		for(int j = 1; j <= 10; j++) {
			if(field[i][j] == EMPTY) {
				break;
			}

			if(j == 10) {
				ClearLine(i);
			}
		}
	}

	UpdateScreen();
}

void TetrisInit(void)
{
#ifdef DBG
	USARTInit();
#endif
	LCDInit();
	FieldInit();
	IOEInit();
	L3GD20Init();
	RNGInit();

	BlockNew(&cur_block);
}

void TetrisGameLoop(void)
{
	BlockRemove(last_block);
	BlockAdd(cur_block);

	if(BlockReachBottom(cur_block)) {
		CheckLine();
		last_block.type = EMPTY;

		if(BlockNew(&cur_block) == -1) {
			BlockAdd(cur_block);
			UpdateScreen();

			LCD_SetTextColor(LCD_COLOR_BLUE);
			LCD_DisplayStringLine(LCD_LINE_0, (unsigned char *)"GameOver!");
			while(1);
		}
	}
	else {
		memcpy(&last_block, &cur_block, sizeof(BLOCK_T));
		cur_block.y++;
	}

	UpdateScreen();
}

int TetrisTouchPanel(void)
{
	BLOCK_T new_block;

	if(IOE_TP_GetState()->TouchDetected) {
		new_block.y = cur_block.y;
		new_block.x = cur_block.x;
		new_block.type = cur_block.type;
		new_block.direction = cur_block.direction == 3 ? 0 : cur_block.direction + 1;

		BlockRemove(last_block);
		if(BlockCorrupt(new_block)) {
			BlockAdd(last_block);
		}
		else {
			BlockAdd(new_block);
			memcpy(&cur_block, &new_block, sizeof(BLOCK_T));
			UpdateScreen();
		}

		return 1;
	}

	return 0;
}

void TetrisL3GD20(void)
{
	static float axis_y = 0;
	static int countdown = 0;
	BLOCK_T new_block;
	uint8_t tmp[6] = {0};
	int16_t a[3] = {0};
	uint8_t tmpreg = 0;

	L3GD20_Read(&tmpreg, L3GD20_CTRL_REG4_ADDR, 1);
	L3GD20_Read(tmp, L3GD20_OUT_X_L_ADDR, 6);

	/* check in the control register 4 the data alignment (Big Endian or Little Endian)*/
	if (!(tmpreg & 0x40)) {
		for (int i = 0; i < 3; i++) {
			a[i] = (int16_t)(((uint16_t)tmp[2 * i + 1] << 8) | (uint16_t)tmp[2 * i]);
			axis_y += (float)a[i] / 114.285F;
		}
	} else {
		for (int i = 0; i < 3; i++) {
			a[i] = (int16_t)(((uint16_t)tmp[2 * i] << 8) | (uint16_t)tmp[2 * i + 1]);
			axis_y += (float)a[i] / 114.285F;
		}
	}

	char tmpstr[16];
	itoa((int)axis_y, tmpstr, 10);
	dbg_puts(tmpstr);
	if(axis_y >= 300) {
		new_block.y = cur_block.y;
		new_block.x = cur_block.x + 1;
		new_block.type = cur_block.type;
		new_block.direction = cur_block.direction;
	}
	else if(axis_y <= -300) {
		new_block.y = cur_block.y;
		new_block.x = cur_block.x - 1;
		new_block.type = cur_block.type;
		new_block.direction = cur_block.direction;
	}
	else {
		return;
	}

	if(countdown--) {
		return;
	}

	BlockRemove(last_block);
	if(BlockCorrupt(new_block)) {
		BlockAdd(last_block);
	}
	else {
		BlockAdd(new_block);
		memcpy(&cur_block, &new_block, sizeof(BLOCK_T));
		UpdateScreen();
	}
	countdown = 10;
}

uint32_t L3GD20_TIMEOUT_UserCallback(void)
{
	return 0;
}
