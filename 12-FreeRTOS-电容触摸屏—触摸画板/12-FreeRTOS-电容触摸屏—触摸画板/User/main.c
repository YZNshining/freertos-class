/**
  *********************************************************************
  * @file    main.c
  * @author  fire
  * @version V1.0
  * @date    2018-xx-xx
  * @brief   FreeRTOS V9.0.0 + STM32 按键显示例程
  *********************************************************************
  * @attention
  *
  * 实验平台:野火 STM32 全系列开发板 
  * 论坛    :http://www.firebbs.cn
  * 淘宝    :https://fire-stm32.taobao.com
  *
  **********************************************************************
  */ 

/*
*************************************************************************
*                             包含的头文件
*************************************************************************
*/ 
/* FreeRTOS头文件 */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
/* 开发板硬件bsp头文件 */
#include "bsp_led.h"
#include "bsp_debug_usart.h"
#include "bsp_key.h"
#include "./sdram/bsp_sdram.h"
#include "./lcd/bsp_lcd.h"
#include "./touch/gt9xx.h"
#include "./touch/palette.h"
#include <string.h>
#include <stdio.h>

/**************************** 任务句柄 ********************************/
static TaskHandle_t AppTaskCreate_Handle = NULL;
static TaskHandle_t Display_Task_Handle = NULL;
static TaskHandle_t KEY_Task_Handle = NULL;

/********************************** 内核对象句柄 *********************************/
static EventGroupHandle_t Event_Handle = NULL;

/******************************* 全局变量声明 ************************************/
#define EVENT_KEY1  (0x01 << 0)  /* KEY1按下事件 */
#define EVENT_KEY2  (0x01 << 1)  /* KEY2按下事件 */
#define EVENT_CLEAR (0x01 << 2)  /* 清屏事件 */

#define CHAR_HEIGHT  24   /* Font16x24字体高度 */
#define MAX_LINES    ((LCD_PIXEL_HEIGHT - 72) / CHAR_HEIGHT)  /* 17行 */

/* 当前显示行计数器 */
static uint8_t current_line = 0;

/* 触摸Clear按钮标志 */
static volatile uint8_t clear_button_pressed = 0;

/* Clear按键区域定义 - 屏幕底部居中，192x72像素 */
#define CLEAR_BTN_WIDTH      192                       /* 24*8=192 */
#define CLEAR_BTN_HEIGHT     72                        /* 24*3=72 */
#define CLEAR_BTN_Y          (LCD_PIXEL_HEIGHT - CLEAR_BTN_HEIGHT)
#define CLEAR_BTN_X          ((LCD_PIXEL_WIDTH - CLEAR_BTN_WIDTH) / 2)

/*
*************************************************************************
*                             函数声明
*************************************************************************
*/
static void AppTaskCreate(void);
static void Display_Task(void* pvParameters);
static void KEY_Task(void* pvParameters);
static void BSP_Init(void);
static void Display_ClearAll(void);
static void Display_DrawClearButton(void);
static void Display_NextLine(char* text);
static uint8_t IsTouchInClearButton(uint16_t x, uint16_t y);

/*****************************************************************
  * @brief  主函数
  ****************************************************************/
int main(void)
{	
  BaseType_t xReturn = pdPASS;
  
  BSP_Init();
  
  printf("\n=== 按键显示例程 ===\n");
  printf("KEY1: 显示 'KEY1 is pressed.'\n");
  printf("KEY2: 显示 'KEY2 is pressed.'\n");
  printf("触摸底部Clear按钮: 清屏\n\n");

  /* 创建事件组 */
  Event_Handle = xEventGroupCreate();
  if(NULL == Event_Handle)
  {
    printf("事件组创建失败！\n");
    return -1;
  }
  
  /* 创建AppTaskCreate任务 */
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
  
  /* 创建Display_Task任务 */
  xReturn = xTaskCreate((TaskFunction_t )Display_Task,
                        (const char*    )"Display_Task",
                        (uint16_t       )512,
                        (void*          )NULL,
                        (UBaseType_t    )2,
                        (TaskHandle_t*  )&Display_Task_Handle);
  if(pdPASS == xReturn)
    printf("Display_Task创建成功!\r\n");
    
  /* 创建KEY_Task任务 */
  xReturn = xTaskCreate((TaskFunction_t )KEY_Task,
                        (const char*    )"KEY_Task",
                        (uint16_t       )512,
                        (void*          )NULL,
                        (UBaseType_t    )3,
                        (TaskHandle_t*  )&KEY_Task_Handle);
  if(pdPASS == xReturn)
    printf("KEY_Task创建成功!\r\n");
  
  vTaskDelete(AppTaskCreate_Handle);
  
  taskEXIT_CRITICAL();
}


static void Display_Task(void* parameter)
{	
  EventBits_t event_value;
  
  /* 初始化显示 - 清屏并绘制Clear按钮 */
  LCD_SetLayer(LCD_FOREGROUND_LAYER);
  LCD_SetTransparency(0xFF);
  Display_ClearAll();
  
  while (1)
  {
    /* 等待按键事件，100ms超时用于轮询触摸标志 */
    event_value = xEventGroupWaitBits(Event_Handle,
                                       EVENT_KEY1 | EVENT_KEY2,
                                       pdTRUE,
                                       pdFALSE,
                                       pdMS_TO_TICKS(100));
    
    /* 检查触摸Clear按钮标志 */
    if(clear_button_pressed)
    {
      clear_button_pressed = 0;
      printf("Clear! 清屏\n");
      taskENTER_CRITICAL();
      Display_ClearAll();
      current_line = 0;
      taskEXIT_CRITICAL();
    }
    
    /* 处理KEY1事件 */
    if(event_value & EVENT_KEY1)
    {
      printf("KEY1 按下\n");
      taskENTER_CRITICAL();
      Display_NextLine("KEY1 is pressed.");
      taskEXIT_CRITICAL();
    }
    
    /* 处理KEY2事件 */
    if(event_value & EVENT_KEY2)
    {
      printf("KEY2 按下\n");
      taskENTER_CRITICAL();
      Display_NextLine("KEY2 is pressed.");
      taskEXIT_CRITICAL();
    }
  }
}


static void KEY_Task(void* parameter)
{	
  while (1)
  {
    /* 处理物理按键KEY1 */
    if(Key_Scan(KEY1_GPIO_PORT, KEY1_PIN) == KEY_ON)
    {
      xEventGroupSetBits(Event_Handle, EVENT_KEY1);
    }
    
    /* 处理物理按键KEY2 */
    if(Key_Scan(KEY2_GPIO_PORT, KEY2_PIN) == KEY_ON)
    {
      xEventGroupSetBits(Event_Handle, EVENT_KEY2);
    }
    
    vTaskDelay(20);
  }
}


/**
  * @brief  触摸回调函数 - 由GTP_Touch_Down调用
  * @note   gt9xx.c的GTP_Touch_Down中已添加此函数调用
  */
void Touch_Callback(uint16_t x, uint16_t y, uint8_t pressed)
{
  if(pressed == 0)
    return;
    
  if(IsTouchInClearButton(x, y))
  {
    clear_button_pressed = 1;
  }
}


static void BSP_Init(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	
	LED_GPIO_Config();
	Debug_USART_Config();
	Key_GPIO_Config();
	
	/* 初始化触摸屏 */
	GTP_Init_Panel(); 
	
	/* 初始化LCD */
	LCD_Init();
	LCD_LayerInit();
	LTDC_Cmd(ENABLE);
	
	/* 背景层刷黑 */
	LCD_SetLayer(LCD_BACKGROUND_LAYER);  
	LCD_Clear(LCD_COLOR_BLACK);
	
	/* 前景层刷黑 */
	LCD_SetLayer(LCD_FOREGROUND_LAYER); 
	LCD_SetTransparency(0xFF);
	LCD_Clear(LCD_COLOR_BLACK);
	
	/* 不初始化 Palette，避免画板覆盖屏幕 */
}


/**
  * @brief  清屏并重新绘制Clear按钮
  */
static void Display_ClearAll(void)
{
	/* 整屏清为黑色 */
	LCD_Clear(LCD_COLOR_BLACK);
	
	/* 绘制Clear按钮 */
	Display_DrawClearButton();
}


/**
  * @brief  在屏幕底部居中绘制Clear按钮
  * @note   按钮大小: 192x72 (24*8 x 24*3)
  *         灰色背景 + 白色边框 + 白色文字
  */
static void Display_DrawClearButton(void)
{
	/* 按钮背景 */
	LCD_SetTextColor(CL_BUTTON_GREY);
	LCD_DrawFullRect(CLEAR_BTN_X, CLEAR_BTN_Y, CLEAR_BTN_WIDTH, CLEAR_BTN_HEIGHT);
	
	/* 按钮边框 */
	LCD_SetTextColor(CL_WHITE);
	LCD_DrawRect(CLEAR_BTN_X, CLEAR_BTN_Y, CLEAR_BTN_WIDTH, CLEAR_BTN_HEIGHT);
	
	/* 按钮文字 "Clear" 居中 */
	LCD_SetFont(&Font16x24);
	LCD_SetTextColor(CL_WHITE);
	LCD_SetBackColor(CL_BUTTON_GREY);
	LCD_DisplayStringLine_EN_CH(CLEAR_BTN_Y + (CLEAR_BTN_HEIGHT - 24) / 2,
	                            (uint8_t*)"Clear");
}


/**
  * @brief  在下一行显示文本
  */
static void Display_NextLine(char* text)
{
	uint16_t y_pos;
	
	/* 超出最大行数则清屏 */
	if(current_line >= MAX_LINES)
	{
		Display_ClearAll();
		current_line = 0;
	}
	
	y_pos = current_line * CHAR_HEIGHT;
	
	/* 白字黑底 */
	LCD_SetFont(&Font16x24);
	LCD_SetTextColor(CL_WHITE);
	LCD_SetBackColor(CL_BLACK);
	LCD_DisplayStringLine_EN_CH(y_pos, (uint8_t*)text);
	
	current_line++;
}


/**
  * @brief  判断触摸坐标是否在Clear按钮区域内
  */
static uint8_t IsTouchInClearButton(uint16_t x, uint16_t y)
{
	if(x >= CLEAR_BTN_X && x <= (CLEAR_BTN_X + CLEAR_BTN_WIDTH) &&
	   y >= CLEAR_BTN_Y && y <= (CLEAR_BTN_Y + CLEAR_BTN_HEIGHT))
	{
		return 1;
	}
	return 0;
}

/********************************END OF FILE****************************/