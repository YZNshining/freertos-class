/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  */

#include "stm32f4xx_it.h"
#include "bsp_debug_usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

extern QueueHandle_t xUartRxQueue;
extern TimerHandle_t xFrameTimeoutTimer;

/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

void NMI_Handler(void) {}

void HardFault_Handler(void) { while(1) {} }

void MemManage_Handler(void) { while(1) {} }

void BusFault_Handler(void) { while(1) {} }

void UsageFault_Handler(void) { while(1) {} }

void DebugMon_Handler(void) {}

extern void xPortSysTickHandler(void);
void SysTick_Handler(void)
{
    #if (INCLUDE_xTaskGetSchedulerState == 1)
      if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
      {
    #endif
        xPortSysTickHandler();
    #if (INCLUDE_xTaskGetSchedulerState == 1)
      }
    #endif
}

/******************************************************************************/
/*                 STM32F4xx Peripherals Interrupt Handlers                   */
/******************************************************************************/

void EXTI0_IRQHandler(void) {}
void EXTI15_10_IRQHandler(void) {}

void USART1_IRQHandler(void)
{
  if(USART_GetITStatus(DEBUG_USART, USART_IT_RXNE) != RESET)
  {
    uint8_t rx_byte;
    uint16_t queue_value;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    rx_byte = (uint8_t)USART_ReceiveData(DEBUG_USART);
    queue_value = (uint16_t)rx_byte;

    xQueueSendFromISR(xUartRxQueue, &queue_value, &xHigherPriorityTaskWoken);

    if(xFrameTimeoutTimer != NULL)
    {
      xTimerResetFromISR(xFrameTimeoutTimer, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
