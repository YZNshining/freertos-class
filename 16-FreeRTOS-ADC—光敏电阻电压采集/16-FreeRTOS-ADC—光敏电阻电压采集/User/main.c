/**
  *********************************************************************
  * @file    main.c
  * @author  fire
  * @version V1.0
  * @date    2018-xx-xx
  * @brief   FreeRTOS V9.0.0  + STM32  ADC + PWM + LCD
  *********************************************************************
  */

/*
*************************************************************************
*                             包含的头文件
*************************************************************************
*/
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "bsp_led.h"
#include "bsp_debug_usart.h"
#include "./adc/bsp_adc.h"
#include "./sdram/bsp_sdram.h"
#include "./lcd/bsp_lcd.h"

/******************************** 任务句柄 ********************************/
static TaskHandle_t AppTaskCreate_Handle = NULL;
static TaskHandle_t ADC_Task_Handle = NULL;
static TaskHandle_t LED_Task_Handle = NULL;
static TaskHandle_t LCD_Task_Handle = NULL;

/***************************** 全局变量 ***********************************/
extern __IO uint16_t ADC_ConvertedValue;

static float g_display_voltage = 0.0f;
static uint8_t g_display_brightness = 0;

#define ADC_DATA_READY_BIT   (1 << 0)
#define BRIGHTNESS_READY_BIT (1 << 1)

/*
*************************************************************************
*                            函数声明
*************************************************************************
*/
static void AppTaskCreate(void);
static void ADC_Task(void* pvParameters);
static void LED_Task(void* pvParameters);
static void LCD_Task(void* pvParameters);
static void BSP_Init(void);

/*****************************************************************
  * @brief  主函数
  ****************************************************************/
int main(void)
{
  BaseType_t xReturn = pdPASS;

  BSP_Init();

  printf("FreeRTOS ADC + PWM LED Control\r\n\r\n");

  xReturn = xTaskCreate((TaskFunction_t)AppTaskCreate,
                        (const char*)   "AppTaskCreate",
                        (uint16_t)      512,
                        (void*)         NULL,
                        (UBaseType_t)   1,
                        (TaskHandle_t*) &AppTaskCreate_Handle);

  if(pdPASS == xReturn)
    vTaskStartScheduler();
  else
    return -1;

  while(1);
}

/***********************************************************************
  * @函数名  ：AppTaskCreate
  * @功能说明：创建3个应用任务
  **********************************************************************/
static void AppTaskCreate(void)
{
  BaseType_t xReturn = pdPASS;

  taskENTER_CRITICAL();

  xReturn = xTaskCreate((TaskFunction_t)ADC_Task,
                        (const char*)   "ADC_Task",
                        (uint16_t)      256,
                        (void*)         NULL,
                        (UBaseType_t)   3,
                        (TaskHandle_t*) &ADC_Task_Handle);
  if(pdPASS == xReturn)
    printf("ADC Task created!\r\n");

  xReturn = xTaskCreate((TaskFunction_t)LED_Task,
                        (const char*)   "LED_Task",
                        (uint16_t)      256,
                        (void*)         NULL,
                        (UBaseType_t)   3,
                        (TaskHandle_t*) &LED_Task_Handle);
  if(pdPASS == xReturn)
    printf("LED Task created!\r\n");

  xReturn = xTaskCreate((TaskFunction_t)LCD_Task,
                        (const char*)   "LCD_Task",
                        (uint16_t)      512,
                        (void*)         NULL,
                        (UBaseType_t)   2,
                        (TaskHandle_t*) &LCD_Task_Handle);
  if(pdPASS == xReturn)
    printf("LCD Task created!\r\n");

  vTaskDelete(AppTaskCreate_Handle);

  taskEXIT_CRITICAL();
}


/**********************************************************************
  * @函数名  ：ADC_Task
  * @功能说明：AD采集任务 — 100ms读一次，3次平均后通过通知发送
  ********************************************************************/
static void ADC_Task(void* parameter)
{
  uint32_t adc_sum;
  uint32_t adc_avg;
  float voltage;
  uint32_t voltage_mv;

  printf("\r\n---- ADC Task Start (DMA, 3-sample avg) ----\r\n");

  while(1)
  {
    adc_sum = 0;
    for(int i = 0; i < 3; i++)
    {
      vTaskDelay(pdMS_TO_TICKS(100));
      adc_sum += ADC_ConvertedValue;
    }
    adc_avg = adc_sum / 3;

    voltage = (float)adc_avg / 4096.0f * 3.3f;
    voltage_mv = (uint32_t)(voltage * 1000.0f);

    g_display_voltage = voltage;
    xTaskNotify(LCD_Task_Handle, ADC_DATA_READY_BIT, eSetBits);

    xTaskNotify(LED_Task_Handle, voltage_mv, eSetValueWithOverwrite);
  }
}


/**********************************************************************
  * @函数名  ：LED_Task
  * @功能说明：LED控制任务 — 根据电压值输出PWM线性控制LED亮度
  ********************************************************************/
static void LED_Task(void* parameter)
{
  uint32_t voltage_mv;
  float voltage;
  uint8_t brightness;

  printf("\r\n---- LED Task Start (PWM brightness) ----\r\n");

  while(1)
  {
    xTaskNotifyWait(0, 0xFFFFFFFF, &voltage_mv, portMAX_DELAY);

    voltage = (float)voltage_mv / 1000.0f;

    if(voltage >= 3.3f)
      brightness = 100;
    else if(voltage <= 0.0f)
      brightness = 0;
    else
      brightness = (uint8_t)(voltage / 3.3f * 100.0f);

    LED_SetBrightness(100 - brightness);

    g_display_brightness = brightness;
    xTaskNotify(LCD_Task_Handle, BRIGHTNESS_READY_BIT, eSetBits);
  }
}


/**********************************************************************
  * @函数名  ：LCD_Task
  * @功能说明：LCD显示任务 — 在LCD上显示电压值和亮度百分比
  ********************************************************************/
static void LCD_Task(void* parameter)
{
  uint32_t notify_bits;
  char buf[40];

  printf("\r\n---- LCD Task Start ----\r\n");

  while(1)
  {
    xTaskNotifyWait(0,
                    ADC_DATA_READY_BIT | BRIGHTNESS_READY_BIT,
                    &notify_bits,
                    portMAX_DELAY);

    /* 清除之前显示的行 */
    LCD_ClearLine(LCD_LINE_5);
    LCD_ClearLine(LCD_LINE_6);
    LCD_ClearLine(LCD_LINE_7);

    /* 显示电压值 */
    sprintf(buf, "Voltage : %.2f V", g_display_voltage);
    LCD_DisplayStringLine(LCD_LINE_5, (uint8_t*)buf);

    /* 显示亮度百分比 */
    sprintf(buf, "Brightness : %d %%", g_display_brightness);
    LCD_DisplayStringLine(LCD_LINE_7, (uint8_t*)buf);

    /* 同时输出到串口 */
    printf("--------------------------------------\r\n");
    printf("  Voltage : %.2f V\r\n", g_display_voltage);
    printf("  Brightness : %d %%\r\n", g_display_brightness);
    printf("--------------------------------------\r\n\r\n");
  }
}


/***********************************************************************
  * @函数名  ：BSP_Init
  * @功能说明：板级初始化，包含LCD初始化
  *********************************************************************/
static void BSP_Init(void)
{
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

  LED_GPIO_Config();
  LED_PWM_Init();
  Debug_USART_Config();
  Rheostat_Init();

  /* LCD初始化 */
  LCD_Init();
  LCD_LayerInit();
  LTDC_Cmd(ENABLE);

  LCD_SetLayer(LCD_BACKGROUND_LAYER);
  LCD_Clear(LCD_COLOR_BLACK);

  LCD_SetLayer(LCD_FOREGROUND_LAYER);
  LCD_SetTransparency(0xFF);
  LCD_Clear(LCD_COLOR_BLACK);

  /* 设置前景显示颜色 */
  LCD_SetTextColor(LCD_COLOR_WHITE);
  LCD_SetBackColor(LCD_COLOR_BLACK);

  /* 显示标题 */
  LCD_DisplayStringLine(LCD_LINE_0, (uint8_t*)"ADC + PWM Control");
  LCD_DisplayStringLine(LCD_LINE_2, (uint8_t*)"----------------");
}

/********************************END OF FILE****************************/
