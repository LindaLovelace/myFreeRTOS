/**
  ******************************************************************************
  * @file    Template/main.c
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    20-September-2013
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tetris.h"

#include <stdio.h>
/** @addtogroup Template
  * @{
  */

/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

void vGameLoopTask(void *pvParameters)
{
	while(1) {
		TetrisGameLoop();
		vTaskDelay(50000);
	}
}

void vGameTouchTask(void *pvParameters)
{
	while(1) {
		if(TetrisTouchPanel()) {
			vTaskDelay(10000);
		}
	}
}

void vGameL3GD20(void *pvParameters)
{
	while(1) {
		if(TetrisL3GD20()) {
	//		vTaskDelay(5000);
		}
		vTaskDelay(5000);
	}
}

/* Main Function -------------------------------------------------------------*/
int main( void )
{
	TetrisInit();

	xTaskCreate( vGameLoopTask, (signed char*) "GameLoop", 128, NULL, tskIDLE_PRIORITY + 2, NULL );
	xTaskCreate( vGameTouchTask, (signed char*) "TouchPanel", 128, NULL, tskIDLE_PRIORITY + 1, NULL );
	xTaskCreate( vGameL3GD20, (signed char*) "L3GD20", 128, NULL, tskIDLE_PRIORITY + 1, NULL );

	//Call Scheduler
	vTaskStartScheduler();
}
