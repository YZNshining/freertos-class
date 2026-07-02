/**
  *********************************************************************
  * @file    main.c
  * @author  FreeRTOS Serial Protocol Demo
  * @version V1.0
  * @brief   FreeRTOS + UART Protocol + LCD Display
  *********************************************************************
  */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "event_groups.h"
#include "bsp_led.h"
#include "bsp_debug_usart.h"
#include "./lcd/bsp_lcd.h"
#include "./sdram/bsp_sdram.h"
#include "./font/fonts.h"
#include <stdio.h>
#include <string.h>

/**************************** 协议常量定义 ********************************/
#define FRAME_TIMEOUT_MS         200
#define MAX_FRAME_DATA_LEN       512
#define FRAME_READY_SIGNAL       0x100
#define UART_RX_QUEUE_LEN        540

/**************************** 队列句柄 ********************************/
QueueHandle_t xUartRxQueue = NULL;

/**************************** 事件组句柄 ********************************/
static EventGroupHandle_t xCommEventGroup;
#define FRAME_RECEIVED_BIT      (1 << 0)

/**************************** 任务句柄 ********************************/
static TaskHandle_t AppTaskCreate_Handle = NULL;
TaskHandle_t xProtocolTask_Handle = NULL;
static TaskHandle_t xResponseTask_Handle = NULL;

/**************************** 软件定时器句柄 ********************************/
TimerHandle_t xFrameTimeoutTimer = NULL;

/**************************** 全局变量 ********************************/
static uint8_t g_frame_buf[MAX_FRAME_DATA_LEN];
static uint16_t g_frame_len = 0;
static uint8_t g_disp_line = 0;
#define MAX_DISP_LINES   10

/**************************** 函数声明 ********************************/
static void AppTaskCreate(void);
static void Protocol_Analysis_Task(void* pvParameters);
static void Response_Task(void* pvParameters);
static void BSP_Init(void);
static void FrameTimeoutCallback(TimerHandle_t xTimer);
static void DisplayFrameData(void);

/*****************************************************************
  * @brief  主函数
  ****************************************************************/
int main(void)
{
  BaseType_t xReturn = pdPASS;

  BSP_Init();

  xUartRxQueue = xQueueCreate(UART_RX_QUEUE_LEN, sizeof(uint16_t));
  if(xUartRxQueue == NULL)
  {
    return -1;
  }

  xCommEventGroup = xEventGroupCreate();
  if(xCommEventGroup == NULL)
  {
    return -1;
  }

  xFrameTimeoutTimer = xTimerCreate("FrameTimeout",
                                     pdMS_TO_TICKS(FRAME_TIMEOUT_MS),
                                     pdFALSE,
                                     (void*)0,
                                     FrameTimeoutCallback);
  if(xFrameTimeoutTimer == NULL)
  {
    return -1;
  }

  xTimerStart(xFrameTimeoutTimer, 0);

  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
  NVIC_SetPriority(USART1_IRQn, 5);

  xReturn = xTaskCreate((TaskFunction_t)AppTaskCreate,
                        (const char*)"AppTaskCreate",
                        (uint16_t)512,
                        (void*)NULL,
                        (UBaseType_t)1,
                        (TaskHandle_t*)&AppTaskCreate_Handle);
  if(pdPASS == xReturn)
    vTaskStartScheduler();
  else
    return -1;

  while(1);
}

/***********************************************************************
  * @ 函数名  ： AppTaskCreate
  **********************************************************************/
static void AppTaskCreate(void)
{
  taskENTER_CRITICAL();

  xTaskCreate((TaskFunction_t)Protocol_Analysis_Task,
              (const char*)"Protocol_Analysis",
              (uint16_t)1024,
              (void*)NULL,
              (UBaseType_t)2,
              (TaskHandle_t*)&xProtocolTask_Handle);

  xTaskCreate((TaskFunction_t)Response_Task,
              (const char*)"Response_Task",
              (uint16_t)512,
              (void*)NULL,
              (UBaseType_t)2,
              (TaskHandle_t*)&xResponseTask_Handle);

  vTaskDelete(AppTaskCreate_Handle);

  taskEXIT_CRITICAL();
}

/***********************************************************************
  * @ 函数名  ： FrameTimeoutCallback
  * @ 说明    ： 200ms无新字节，插入FRAME_READY_SIGNAL到队列
  **********************************************************************/
static void FrameTimeoutCallback(TimerHandle_t xTimer)
{
  uint16_t signal = FRAME_READY_SIGNAL;

  (void)xTimer;

  if(g_frame_len > 0)
  {
    xQueueSend(xUartRxQueue, &signal, 0);
  }
}

/***********************************************************************
  * @ 函数名  ： Protocol_Analysis_Task (任务一)
  * @ 说明    ： 从队列读字节累积到缓冲区
  *             收到FRAME_READY_SIGNAL时作为完整一包处理
  **********************************************************************/
static void Protocol_Analysis_Task(void* parameter)
{
  uint16_t rx_value;
  uint8_t user_data_len;

  (void)parameter;

  while(1)
  {
    if(xQueueReceive(xUartRxQueue, &rx_value, portMAX_DELAY) != pdPASS)
      continue;

    if(rx_value == FRAME_READY_SIGNAL)
    {
      xTimerStop(xFrameTimeoutTimer, 0);

      if(g_frame_len >= 3
          && g_frame_buf[0] == 0xAA
          && g_frame_buf[g_frame_len - 1] == 0x55)
      {
        user_data_len = g_frame_buf[1];
        if((uint16_t)(user_data_len + 3) == g_frame_len)
        {
          DisplayFrameData();
          xEventGroupSetBits(xCommEventGroup, FRAME_RECEIVED_BIT);
        }
      }

      g_frame_len = 0;
    }
    else
    {
      if(g_frame_len < MAX_FRAME_DATA_LEN)
      {
        g_frame_buf[g_frame_len++] = (uint8_t)rx_value;
      }
    }
  }
}

/***********************************************************************
  * @ 函数名  ： Response_Task (任务二)
  * @ 说明    ： 收到一包后回发 0xAA 0x01 0x01 0x55
  **********************************************************************/
static void Response_Task(void* parameter)
{
  EventBits_t uxBits;
  uint8_t response[] = {0xAA, 0x01, 0x01, 0x55};
  uint8_t i;

  (void)parameter;

  while(1)
  {
    uxBits = xEventGroupWaitBits(xCommEventGroup,
                                  FRAME_RECEIVED_BIT,
                                  pdTRUE,
                                  pdFALSE,
                                  portMAX_DELAY);

    if(uxBits & FRAME_RECEIVED_BIT)
    {
      for(i = 0; i < sizeof(response); i++)
      {
        Usart_SendByte(DEBUG_USART, response[i]);
      }

      LED1_TOGGLE;
    }
  }
}

/***********************************************************************
  * @ 函数名  ： DisplayFrameData
  * @ 说明    ： LCD显示接收到的数据（一包全部字节）
  **********************************************************************/
static void DisplayFrameData(void)
{
  char disp_buf[64];
  uint8_t user_len = g_frame_buf[1];
  uint8_t i;
  int offset = 0;

  LCD_SetFont(&Font16x24);

  if(g_disp_line >= MAX_DISP_LINES)
  {
    LCD_Clear(LCD_COLOR_BLACK);
    g_disp_line = 0;
  }

  LCD_SetColors(LCD_COLOR_WHITE, LCD_COLOR_BLACK);
  LCD_ClearLine(LINE(g_disp_line));
  offset = snprintf(disp_buf, sizeof(disp_buf), "Received Data: ");
  for(i = 0; i < user_len && (offset + 5) < (int)sizeof(disp_buf); i++)
  {
    offset += snprintf(disp_buf + offset, sizeof(disp_buf) - offset,
                       "0x%02X ", g_frame_buf[2 + i]);
  }
  LCD_DisplayStringLine(LINE(g_disp_line), (uint8_t*)disp_buf);

  g_disp_line++;
}

/***********************************************************************
  * @ 函数名  ： BSP_Init
  **********************************************************************/
static void BSP_Init(void)
{
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

  LED_GPIO_Config();
  Debug_USART_Config();

  LCD_Init();
  LCD_LayerInit();
  LTDC_Cmd(ENABLE);
  LCD_SetLayer(LCD_BACKGROUND_LAYER);
  LCD_Clear(LCD_COLOR_BLACK);
  LCD_SetLayer(LCD_FOREGROUND_LAYER);
  LCD_SetTransparency(0xFF);
  LCD_Clear(LCD_COLOR_BLACK);
}

/********************************END OF FILE****************************/
