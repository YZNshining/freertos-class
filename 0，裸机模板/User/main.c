/**
  ******************************************************************************
  * @file    main.c
  * @author  fire
  * @version V1.0
  * @date    2015-xx-xx
  * @brief   FreeRTOS移植实验 - LED闪烁任务
  ******************************************************************************
  * @attention
  *
  * 实验平台:野火  STM32 F429 开发板
  * 论坛    :http://www.firebbs.cn
  * 淘宝    :https://fire-stm32.taobao.com
  *
  ******************************************************************************
  */
  
#include "stm32f4xx.h"
#include "./led/bsp_led.h"
#include "./key/bsp_key.h" 
#include "FreeRTOS.h"
#include "task.h"

static TaskHandle_t AppTaskCreate_Handle = NULL;
static TaskHandle_t LED_Task_Handle = NULL;

static void AppTaskCreate(void);
static void LED_Task(void* pvParameters);
static void BSP_Init(void);

int main(void)
{	
  BaseType_t xReturn = pdPASS;

  BSP_Init();
  
  xReturn = xTaskCreate((TaskFunction_t )AppTaskCreate,
                        (const char*    )"AppTaskCreate",
                        (uint16_t       )512,
                        (void*          )NULL,
                        (UBaseType_t    )1,
                        (TaskHandle_t*  )&AppTaskCreate_Handle); 

  if(pdPASS == xReturn)
    vTaskStartScheduler();
  else
    return -1;  
  
  while(1);    
}

static void AppTaskCreate(void)
{
  BaseType_t xReturn = pdPASS;
  
  taskENTER_CRITICAL();
  
  xReturn = xTaskCreate((TaskFunction_t )LED_Task,
                        (const char*    )"LED_Task",
                        (uint16_t       )512,
                        (void*          )NULL,
                        (UBaseType_t    )2,
                        (TaskHandle_t*  )&LED_Task_Handle);
  if(pdPASS == xReturn)  
  vTaskDelete(AppTaskCreate_Handle);
  
  taskEXIT_CRITICAL();
}

static void LED_Task(void* parameter)
{
    uint8_t count = 0;
    
    while (1)
    {
        for(count = 0; count < 3; count++)
        {
            LED1_ON;
            vTaskDelay(1000);
            
            LED1_OFF;
            vTaskDelay(1000);
        }
        
        vTaskDelay(1000);
    }
}

static void BSP_Init(void)
{
	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );
	
	LED_GPIO_Config();

	Debug_USART_Config();
  
}
/*********************************************END OF FILE**********************/
