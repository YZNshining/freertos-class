/**
  *********************************************************************
  * @file    main.c
  * @author  fire
  * @version V1.0
  * @date    2018-xx-xx
  * @brief   FreeRTOS V9.0.0 + STM32 LCD显示和触摸控制实验
  *********************************************************************
  * @attention
  *
  * 实验平台:野火 STM32 全系列开发板 
  *
  **********************************************************************
  */ 
 
/*
*************************************************************************
*                             头文件包含
*************************************************************************
*/ 
/* FreeRTOS头文件 */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
/* 硬件bsp头文件 */
#include "bsp_led.h"
#include "bsp_debug_usart.h"
#include "bsp_key.h"
#include "./sdram/bsp_sdram.h"
#include "./lcd/bsp_lcd.h"
#include "./touch/gt9xx.h"
#include "./touch/palette.h"

/**************************** 任务句柄 ********************************/
static TaskHandle_t AppTaskCreate_Handle = NULL;
static TaskHandle_t LCD_Display_Task_Handle = NULL;
static TaskHandle_t KEY_Task_Handle = NULL;

/********************************** 事件组句柄 *************************/
EventGroupHandle_t EventGroupHandle;

/******************************* 事件位定义 ****************************/
#define BIT_KEY1  (1 << 0)
#define BIT_KEY2  (1 << 1)
#define BIT_CLEAR (1 << 2)

/******************************* 全局变量 ******************************/
static uint8_t current_line = 0;
volatile int16_t touch_x = -1;
volatile int16_t touch_y = -1;
volatile uint8_t touch_pressed = 0;

/*
*************************************************************************
*                             函数声明
*************************************************************************
*/
static void AppTaskCreate(void);
static void LCD_Display_Task(void* pvParameters);
static void KEY_Task(void* pvParameters);
static void BSP_Init(void);
void Draw_Clear_Button(void);

/*****************************************************************
  * @brief  主函数
  * @param  无
  * @retval 无
  ****************************************************************/
int main(void)
{	
  BaseType_t xReturn = pdPASS;
  
  /* 硬件初始化 */
  BSP_Init();
  
  printf("FreeRTOS LCD display and touch control experiment\r\n");
  printf("Press KEY1 to display 'KEY1 is pressed.'\r\n");
  printf("Press KEY2 to display 'KEY2 is pressed.'\r\n");
  printf("Touch Clear button to clear screen\r\n");
  
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

/***********************************************************************
  * @brief  创建应用任务
  **********************************************************************/
static void AppTaskCreate(void)
{
  BaseType_t xReturn = pdPASS;
  
  taskENTER_CRITICAL();
  
  /* Create event group */
  EventGroupHandle = xEventGroupCreate();
  if(EventGroupHandle == NULL)
  {
    printf("Create event group failed!\r\n");
    return;
  }
  
  /* Create LCD display task */
  xReturn = xTaskCreate((TaskFunction_t )LCD_Display_Task,
                        (const char*    )"LCD_Display_Task",
                        (uint16_t       )512,
                        (void*          )NULL,
                        (UBaseType_t    )2,
                        (TaskHandle_t*  )&LCD_Display_Task_Handle);
  if(pdPASS == xReturn)
    printf("Create LCD_Display_Task success!\r\n");
    
  /* Create KEY task */
  xReturn = xTaskCreate((TaskFunction_t )KEY_Task,
                        (const char*    )"KEY_Task",
                        (uint16_t       )512,
                        (void*          )NULL,
                        (UBaseType_t    )3,
                        (TaskHandle_t*  )&KEY_Task_Handle); 
  if(pdPASS == xReturn)
    printf("Create KEY_Task success!\r\n");
  
  vTaskDelete(AppTaskCreate_Handle);
  
  taskEXIT_CRITICAL();
}

/**********************************************************************
  * @brief  LCD显示任务 - 负责LCD文字显示和清屏
  ********************************************************************/
static void LCD_Display_Task(void* parameter)
{	
  EventBits_t r_event;
  
  while (1)
  {
    r_event = xEventGroupWaitBits(EventGroupHandle,  
                              BIT_KEY1 | BIT_KEY2 | BIT_CLEAR,
                              pdTRUE,             
                              pdTRUE,
                              portMAX_DELAY);     
    
    if((r_event & BIT_CLEAR) != 0)
    {
      LCD_Clear(LCD_COLOR_BLACK);
      current_line = 0;
      Draw_Clear_Button();
    }
    
    if((r_event & BIT_KEY1) != 0)
    {
      if(current_line < 19)
      {
        LCD_SetTextColor(LCD_COLOR_WHITE);
        LCD_DisplayStringLine(LINE(current_line), (uint8_t *)"KEY1 is pressed.");
        current_line++;
      }
    }
    
    if((r_event & BIT_KEY2) != 0)
    {
      if(current_line < 19)
      {
        LCD_SetTextColor(LCD_COLOR_WHITE);
        LCD_DisplayStringLine(LINE(current_line), (uint8_t *)"KEY2 is pressed.");
        current_line++;
      }
    }
  }
}

/**********************************************************************
  * @brief  按键任务 - 检测物理按键和触摸屏并发送事件
  ********************************************************************/
static void KEY_Task(void* parameter)
{	
  uint16_t clear_button_x = 24 * 8;
  uint16_t clear_button_y = 19 * 24;
  uint16_t clear_button_width = 24 * 3;
  uint16_t clear_button_height = 24;
  
  while (1)
  {
    if( Key_Scan(KEY1_GPIO_PORT,KEY1_PIN) == KEY_ON )
    {
      xEventGroupSetBits(EventGroupHandle, BIT_KEY1);
      printf("KEY1 pressed, send BIT_KEY1 event\r\n");
    } 
    
    if( Key_Scan(KEY2_GPIO_PORT,KEY2_PIN) == KEY_ON )
    {
      xEventGroupSetBits(EventGroupHandle, BIT_KEY2);
      printf("KEY2 pressed, send BIT_KEY2 event\r\n");
    }
    
    if(touch_pressed == 1)
    {
      if(touch_x >= clear_button_x && touch_x <= (clear_button_x + clear_button_width) &&
         touch_y >= clear_button_y && touch_y <= (clear_button_y + clear_button_height))
      {
        xEventGroupSetBits(EventGroupHandle, BIT_CLEAR);
        printf("Touch Clear button, send BIT_CLEAR event\r\n");
      }
      touch_pressed = 0;
    }
    
    vTaskDelay(20);
  }
}

/**********************************************************************
  * @brief  绘制Clear按钮
  ********************************************************************/
void Draw_Clear_Button(void)
{
  uint16_t button_x = 24 * 8;
  uint16_t button_y = 19 * 24;
  uint16_t button_width = 24 * 3;
  uint16_t button_height = 24;
  
  LCD_SetColors(LCD_COLOR_GREY, LCD_COLOR_BLACK);
  LCD_DrawFullRect(button_x, button_y, button_width, button_height);
  
  LCD_SetColors(LCD_COLOR_BLUE, LCD_COLOR_GREY);
  LCD_DrawRect(button_x, button_y, button_width, button_height);
  
  LCD_SetTextColor(LCD_COLOR_WHITE);
  LCD_DisplayStringLine(LINE(19), (uint8_t *)"Clear");
}

/***********************************************************************
  * @brief  硬件初始化
  *********************************************************************/
static void BSP_Init(void)
{
	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );
	
	LED_GPIO_Config();
	Debug_USART_Config();
  Key_GPIO_Config();
  GTP_Init_Panel(); 
	
  LCD_Init();
  LCD_LayerInit();
  LTDC_Cmd(ENABLE);
	
  LCD_SetLayer(LCD_BACKGROUND_LAYER);  
	LCD_Clear(LCD_COLOR_BLACK);
	
  LCD_SetLayer(LCD_FOREGROUND_LAYER); 
  LCD_SetTransparency(0xFF);
  LCD_Clear(LCD_COLOR_BLACK);
	
  LCD_SetFont(&Font16x24);
  LCD_SetTextColor(LCD_COLOR_WHITE);
  
  Draw_Clear_Button();
}

/********************************END OF FILE****************************/
