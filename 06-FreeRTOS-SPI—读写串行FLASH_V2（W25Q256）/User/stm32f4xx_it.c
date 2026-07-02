/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  */

#include "stm32f4xx_it.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

extern EventGroupHandle_t xStorageEventGroup;
#define KEY1_EVENT_BIT    (1 << 0)
#define KEY2_EVENT_BIT    (1 << 1)


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

void EXTI0_IRQHandler(void)
{
  if(EXTI_GetITStatus(EXTI_Line0) != RESET)
  {
    EXTI_ClearITPendingBit(EXTI_Line0);
    EXTI->IMR &= ~EXTI_Line0;
    {
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      xEventGroupSetBitsFromISR(xStorageEventGroup, KEY1_EVENT_BIT, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

void EXTI15_10_IRQHandler(void)
{
  if(EXTI_GetITStatus(EXTI_Line13) != RESET)
  {
    EXTI_ClearITPendingBit(EXTI_Line13);
    EXTI->IMR &= ~EXTI_Line13;
    {
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      xEventGroupSetBitsFromISR(xStorageEventGroup, KEY2_EVENT_BIT, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
