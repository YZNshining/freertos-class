/**
  *********************************************************************
  * @file    main.c
  * @brief   FreeRTOS v9.0.0 + STM32 - 使用事件组阻塞等待实现触发
  *********************************************************************
  */ 

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "bsp_led.h"
#include "bsp_debug_usart.h"
#include "./tim/bsp_basic_tim.h"
#include "./key/bsp_key.h"
#include "string.h"

/**************************** 任务句柄 ********************************/
static TaskHandle_t AppTaskCreate_Handle = NULL;
static TaskHandle_t LED_Task_Handle = NULL;
static TaskHandle_t CPU_Task_Handle = NULL;
static TaskHandle_t Key_Task_Handle = NULL;
static TaskHandle_t Timer_Task_Handle = NULL;
static TaskHandle_t Judge_Task_Handle = NULL;

/* 事件组（包含所有事件位） */
static EventGroupHandle_t xEventGroup = NULL;

/* 事件位定义 */
#define KEY1_BIT      (1 << 0)  // KEY1 按下
#define KEY2_BIT      (1 << 1)  // KEY2 按下
#define TIMER_BIT     (1 << 2)  // 10秒定时到达
#define KEY_COMBO_BIT (1 << 3)  // KEY1 和 KEY2 都已按下（组合事件）

/*
*************************************************************************
*                             函数声明
*************************************************************************
*/
static void AppTaskCreate(void);
static void LED_Task(void* pvParameters);
static void CPU_Task(void* pvParameters);
static void Key_Task(void* pvParameters);
static void Timer_Task(void* pvParameters);
static void Judge_Task(void* pvParameters);
static void BSP_Init(void);

int main(void)
{	
  BaseType_t xReturn = pdPASS;

  BSP_Init();
  printf("FreeRTOS Event Group Blocking Demo\r\n");
  
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
  taskENTER_CRITICAL();
  
  /* 创建事件组 */
  xEventGroup = xEventGroupCreate();
  
  xTaskCreate((TaskFunction_t )LED_Task,   "LED_Task",   512, NULL, 2, &LED_Task_Handle);
  xTaskCreate((TaskFunction_t )Key_Task,   "Key_Task",   512, NULL, 3, &Key_Task_Handle);
  xTaskCreate((TaskFunction_t )Timer_Task, "Timer_Task", 512, NULL, 3, &Timer_Task_Handle);
  xTaskCreate((TaskFunction_t )Judge_Task, "Judge_Task", 512, NULL, 2, &Judge_Task_Handle);
  xTaskCreate((TaskFunction_t )CPU_Task,   "CPU_Task",   512, NULL, 4, &CPU_Task_Handle);
  
  vTaskDelete(AppTaskCreate_Handle);
  taskEXIT_CRITICAL();
}

/**********************************************************************
  * LED_Task - 阻塞等待 KEY_COMBO_BIT 或 TIMER_BIT，任一触发则翻转LED
  ********************************************************************/
static void LED_Task(void* parameter)
{	
    EventBits_t uxBits;
    const EventBits_t xBitsToWait = KEY_COMBO_BIT | TIMER_BIT;
    
    while (1)
    {
        /* 等待组合事件或定时器事件，任意一个触发即返回，返回前自动清除触发位 */
        uxBits = xEventGroupWaitBits(xEventGroup,
                                     xBitsToWait,
                                     pdTRUE,   /* 退出时清除触发位 */
                                     pdFALSE,  /* 任意位触发即可 */
                                     portMAX_DELAY);
        
            LED1_TOGGLE;
            LED2_TOGGLE;
    }
}

/* 按键任务：仅设置 KEY1_BIT 和 KEY2_BIT，不清除 */
static void Key_Task(void* parameter)
{	
    while (1)
    {
        if(Key_Scan(KEY1_GPIO_PORT, KEY1_PIN) == KEY_ON)
        {
            xEventGroupSetBits(xEventGroup, KEY1_BIT);
        }
        
        if(Key_Scan(KEY2_GPIO_PORT, KEY2_PIN) == KEY_ON)
        {
            xEventGroupSetBits(xEventGroup, KEY2_BIT);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* 定时器任务：每10秒设置 TIMER_BIT */
static void Timer_Task(void* parameter)
{	
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(10000);
    
    xLastWakeTime = xTaskGetTickCount();
    
    while (1)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        xEventGroupSetBits(xEventGroup, TIMER_BIT);
    }
}

/* Judge 任务：阻塞等待 KEY1_BIT 和 KEY2_BIT 同时被置位，然后设置 KEY_COMBO_BIT 并清除 KEY1/KEY2 */
static void Judge_Task(void* parameter)
{
    EventBits_t uxBits;
    const EventBits_t xWaitForBoth = KEY1_BIT | KEY2_BIT;
    
    while (1)
    {
        /* 等待 KEY1 和 KEY2 同时被置位（不清除，等待所有位） */
        uxBits = xEventGroupWaitBits(xEventGroup,
                                     xWaitForBoth,
                                     pdFALSE,   /* 不清除 */
                                     pdTRUE,    /* 等待所有位 */
                                     portMAX_DELAY);
        
        /* 确保两个位都已置位（由于 pdTRUE 设置，满足条件才会返回） */

            /* 设置组合事件位 */
            xEventGroupSetBits(xEventGroup, KEY_COMBO_BIT);
            /* 清除原始按键位，避免重复触发 */
            xEventGroupClearBits(xEventGroup, KEY1_BIT | KEY2_BIT);
    }
}

/* CPU 信息统计任务（仅输出任务列表和运行时间） */
static void CPU_Task(void* parameter)
{	
    uint8_t CPU_RunInfo[400];
    
    while (1)
    {
        memset(CPU_RunInfo, 0, 400);
        vTaskList((char *)&CPU_RunInfo);
        printf("---------------------------------------------\r\n");
        printf("TaskName     State Prio  Stack  TaskNum\r\n");
        printf("%s", CPU_RunInfo);
        printf("---------------------------------------------\r\n");
        
        memset(CPU_RunInfo, 0, 400);
        vTaskGetRunTimeStats((char *)&CPU_RunInfo);
        printf("TaskName     RunCount      Usage\r\n");
        printf("%s", CPU_RunInfo);
        printf("---------------------------------------------\r\n\n");
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void BSP_Init(void)
{
    NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );
    LED_GPIO_Config();
    Debug_USART_Config();
    TIMx_Configuration();
    Key_GPIO_Config();
}

/********************************END OF FILE****************************/