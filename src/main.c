/**
  ******************************************************************************
  * @file    main.c
  * @author  Ac6
  * @version V1.0
  * @date    01-December-2013
  * @brief   Default main function.
  ******************************************************************************
*/


#include "main.h"
#include "FreeRTOS.h"
#include "Task.h"
#include "Queue.h"

void *ledRed;
void *ledBlue;
void *ledOrange;
void *ledGreen;

static void vSenderTask( void *pvParameters );
static void vReceiverTask(void *pvParameters);

TaskHandle_t xTask2Handle = NULL;
QueueHandle_t xQueue;

/* Declare two variables of type Data_t that will be passed on the queue. */
static const Data_t xStructsToSend[ 2 ] =
{
	{ 100, eSender1 }, /* Used by Sender1. */
	{ 200, eSender2 } /* Used by Sender2. */
};


int main(void)
{

	BSP_Init();

	xQueue = xQueueCreate( 3, sizeof( Data_t ));

	xTaskCreate( vSenderTask, "Sender1", 1000, &( xStructsToSend[ 0 ] ), 2, NULL );
	xTaskCreate( vSenderTask, "Sender2", 1000, &( xStructsToSend[ 1 ] ), 2, NULL );
	xTaskCreate( vReceiverTask, "Receiver", 1000, NULL, 1, NULL );
	/* --- APPLICATION TASKS CAN BE CREATED HERE --- */
	/* Start the created tasks running. */
	vTaskStartScheduler();
	/* Execution will only reach here if there was insufficient heap to
	start the scheduler. */

	for(;;);
}

static void vSenderTask( void *pvParameters ) {
	BaseType_t xStatus;
	const TickType_t xTicksToWait = pdMS_TO_TICKS( 100 );
	uint8_t txData[50];

	for( ;; ) {

		xStatus = xQueueSendToBack( xQueue, pvParameters, xTicksToWait );

		if( xStatus != pdPASS ) {

			sprintf(txData, "Could not send to the queue.\r\n");
			CONSOLE_SendMsg(txData, (uint16_t)strlen(txData));
		}
		vTaskDelay(1);
	}
}

static void vReceiverTask(void *pvParameters) {

	Data_t xReceivedStructure;
	BaseType_t xStatus;
	uint8_t txData[50];

	for (;;) {

		if (uxQueueMessagesWaiting(xQueue) != 3) {
			sprintf(txData, "Queue should have been full!\r\n");
			CONSOLE_SendMsg(txData, (uint16_t)strlen(txData));
		}
		xStatus = xQueueReceive(xQueue, &xReceivedStructure, 0);
		if (xStatus == pdPASS) {

			if (xReceivedStructure.eDataSource == eSender1) {

				sprintf(txData, "From Sender 1 = %u\r\n", xReceivedStructure.ucValue);
				CONSOLE_SendMsg(txData, (uint16_t)strlen(txData));

			} else {
				sprintf(txData, "From Sender 2 = %u\r\n", xReceivedStructure.ucValue);
				CONSOLE_SendMsg(txData, (uint16_t)strlen(txData));
			}
		} else {

			sprintf(txData, "Could not receive from the queue.\r\n");
			CONSOLE_SendMsg(txData, (uint16_t)strlen(txData));
		}
	}
}


void SW_PressEvent(void){

}
