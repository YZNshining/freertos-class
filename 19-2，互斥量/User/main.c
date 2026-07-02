/**
  *********************************************************************
  * @file    main.c
  * @author  fire
  * @version V1.0
  * @date    2018-xx-xx
  * @brief   FreeRTOS V9.0.0 + STM32 信号量和互斥量实验
  *********************************************************************
  */ 

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "bsp_led.h"
#include "bsp_debug_usart.h"
#include "bsp_key.h"

static TaskHandle_t AppTaskCreate_Handle = NULL;
static TaskHandle_t Key_Task_Handle = NULL;
static TaskHandle_t LowPriority_Task_Handle = NULL;
static TaskHandle_t MidPriority_Task_Handle = NULL;
static TaskHandle_t HighPriority_Task_Handle = NULL;

SemaphoreHandle_t MuxSem_Handle = NULL;
SemaphoreHandle_t BinarySem_Handle = NULL;

#define MODE_NONE    0
#define MODE_BINARY  1
#define MODE_MUTEX   2
static uint8_t g_sync_mode = MODE_NONE;

static void AppTaskCreate(void);
static void Key_Task(void* pvParameters);
static void LowPriority_Task(void* pvParameters);
static void MidPriority_Task(void* pvParameters);
static void HighPriority_Task(void* pvParameters);
static void BSP_Init(void);

int main(void)
{	
  BaseType_t xReturn = pdPASS;
  BSP_Init();
  
  printf("这是一个[野火]-STM32全系列开发板-FreeRTOS信号量与互斥量实验！\r\n");
  printf("按键1：二值信号量模式（观察优先级翻转）\r\n");
  printf("按键2：互斥量模式（解决优先级翻转）\r\n\r\n");
  
  xReturn = xTaskCreate(AppTaskCreate, "AppTaskCreate", 512, NULL, 1, &AppTaskCreate_Handle);
  if(pdPASS == xReturn)
    vTaskStartScheduler();
  else
    return -1;  
  
  while(1);    
}

static void AppTaskCreate(void)
{
  taskENTER_CRITICAL();
  
  MuxSem_Handle = xSemaphoreCreateMutex();
  if(NULL != MuxSem_Handle)
    printf("互斥量创建成功!\r\n");

  BinarySem_Handle = xSemaphoreCreateBinary();
  if(NULL != BinarySem_Handle)
    printf("二值信号量创建成功!\r\n");
  
  xSemaphoreGive(BinarySem_Handle);
  xSemaphoreGive(MuxSem_Handle);
  
  xTaskCreate(Key_Task, "Key_Task", 512, NULL, 5, &Key_Task_Handle);
  printf("按键扫描任务创建成功!\r\n");
  
  xTaskCreate(LowPriority_Task, "LowPriority_Task", 512, NULL, 2, &LowPriority_Task_Handle);
  printf("低优先级任务创建成功!\r\n");
  
  xTaskCreate(MidPriority_Task, "MidPriority_Task", 512, NULL, 3, &MidPriority_Task_Handle);
  printf("中优先级任务创建成功!\r\n");
  
  xTaskCreate(HighPriority_Task, "HighPriority_Task", 512, NULL, 4, &HighPriority_Task_Handle);
  printf("高优先级任务创建成功!\r\n\r\n");
  
  vTaskDelete(AppTaskCreate_Handle);
  taskEXIT_CRITICAL();
}

static void Key_Task(void* pvParameters)
{
  while (1)
  {
    if(Key_Scan(KEY1_GPIO_PORT, KEY1_PIN) == KEY_ON)
    {
      printf("\r\n========================================\r\n");
      printf("按键1按下，切换到二值信号量模式\r\n");
      printf("========================================\r\n");
      g_sync_mode = MODE_BINARY;
      xSemaphoreGive(BinarySem_Handle);
    }
    
    if(Key_Scan(KEY2_GPIO_PORT, KEY2_PIN) == KEY_ON)
    {
      printf("\r\n========================================\r\n");
      printf("按键2按下，切换到互斥量模式\r\n");
      printf("========================================\r\n");
      g_sync_mode = MODE_MUTEX;
      xSemaphoreGive(MuxSem_Handle);
    }
    
    vTaskDelay(20);
  }
}

static void LowPriority_Task(void* pvParameters)
{	
  static uint32_t i;
  BaseType_t xReturn = pdPASS;
  
  while (1)
  {
    if(g_sync_mode == MODE_BINARY)
    {
      printf("LowPriority_Task 获取二值信号量\r\n");
      xReturn = xSemaphoreTake(BinarySem_Handle, portMAX_DELAY);
      if(pdTRUE == xReturn)
      {
        printf("LowPriority_Task Running\r\n");
        for(i = 0; i < 4000000; i++) { taskYIELD(); }
        printf("LowPriority_Task 释放二值信号量!\r\n");
        xSemaphoreGive(BinarySem_Handle);
      }
      LED1_TOGGLE;
    }
    else if(g_sync_mode == MODE_MUTEX)
    {
      printf("LowPriority_Task 获取互斥量\r\n");
      xReturn = xSemaphoreTake(MuxSem_Handle, portMAX_DELAY);
      if(pdTRUE == xReturn)
      {
        printf("LowPriority_Task Running\r\n");
        for(i = 0; i < 4000000; i++) { taskYIELD(); }
        printf("LowPriority_Task 释放互斥量!\r\n");
        xSemaphoreGive(MuxSem_Handle);
      }
      LED1_TOGGLE;
    }
    
    vTaskDelay(1000);
  }
}

static void MidPriority_Task(void* pvParameters)
{	 
  while (1)
  {
    if(g_sync_mode != MODE_NONE)
    {
      printf("MidPriority_Task Running\r\n");
    }
    vTaskDelay(1000);
  }
}

static void HighPriority_Task(void* pvParameters)
{	
  BaseType_t xReturn = pdTRUE;
  
  while (1)
  {
    if(g_sync_mode == MODE_BINARY)
    {
      printf("HighPriority_Task 获取二值信号量\r\n");
      xReturn = xSemaphoreTake(BinarySem_Handle, portMAX_DELAY);
      if(pdTRUE == xReturn)
      {
        printf("HighPriority_Task Running\r\n");
        LED1_TOGGLE;
        printf("HighPriority_Task 释放二值信号量!\r\n");
        xSemaphoreGive(BinarySem_Handle);
      }
    }
    else if(g_sync_mode == MODE_MUTEX)
    {
      printf("HighPriority_Task 获取互斥量\r\n");
      xReturn = xSemaphoreTake(MuxSem_Handle, portMAX_DELAY);
      if(pdTRUE == xReturn)
      {
        printf("HighPriority_Task Running\r\n");
        LED1_TOGGLE;
        printf("HighPriority_Task 释放互斥量!\r\n");
        xSemaphoreGive(MuxSem_Handle);
      }
    }
    
    vTaskDelay(1000);
  }
}

static void BSP_Init(void)
{
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
  LED_GPIO_Config();
  Debug_USART_Config();
  Key_GPIO_Config();
}