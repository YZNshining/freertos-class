/**
  *********************************************************************
  * @file    main.c
  * @author  FreeRTOS Storage Task Demo
  * @version V1.0
  * @brief   FreeRTOS + EEPROM + SPI Flash + LCD
  *********************************************************************
  */ 

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "bsp_led.h"
#include "bsp_debug_usart.h"
#include "bsp_key.h"
#include "./exti/bsp_exti.h"
#include "./flash/bsp_spi_flash.h"
#include "./i2c/bsp_i2c_ee.h"
#include "./lcd/bsp_lcd.h"
#include "./sdram/bsp_sdram.h"
#include "./font/fonts.h"
#include <stdio.h>
#include <string.h>

/**************************** 事件组句柄 ********************************/
EventGroupHandle_t xStorageEventGroup;

#define KEY1_EVENT_BIT    (1 << 0)
#define KEY2_EVENT_BIT    (1 << 1)

/**************************** 任务句柄 ********************************/
static TaskHandle_t AppTaskCreate_Handle = NULL;
static TaskHandle_t Write_Task_Handle = NULL;
static TaskHandle_t Read_Task_Handle = NULL;
static TaskHandle_t Reset_Task_Handle = NULL;

/**************************** 全局变量 ********************************/
static uint32_t g_press_cnt = 0;
static uint32_t g_log_cnt = 0;

#define EEPROM_DATA_ADDR    0x00
#define EEPROM_DATA_SIZE    10

#define FLASH_LOG_SECTOR    0x000000
#define FLASH_COUNT_OFFSET  0
#define FLASH_LOG_OFFSET    100
#define FLASH_LOG_ENTRY_SIZE 100
#define MAX_LOG_ENTRIES     39
#define SECTOR_BUF_SIZE     4096

static uint8_t g_sector_buf[SECTOR_BUF_SIZE];

/**************************** 函数声明 ********************************/
static void AppTaskCreate(void);
static void Write_Task(void* pvParameters);
static void Read_Task(void* pvParameters);
static void Reset_Task(void* pvParameters);
static void BSP_Init(void);
static void RecoverState(void);
static void WriteFlashLogs(void);
static void ResetAllMemory(void);
static void DisplayEEPROM(void);
static void DisplayLogs(void);
static void DisplayNoLogs(void);

/*****************************************************************
  * @brief  主函数
  ****************************************************************/
int main(void)
{	
  BaseType_t xReturn = pdPASS;
  
  BSP_Init();
  
  xStorageEventGroup = xEventGroupCreate();
  if(xStorageEventGroup == NULL)
  {
    printf("EventGroup create failed!\r\n");
    return -1;
  }

  RecoverState();
  
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
  NVIC_SetPriority(EXTI0_IRQn, 5);
  NVIC_SetPriority(EXTI15_10_IRQn, 5);
  
  printf("FreeRTOS Storage Task Demo\r\n");
  printf("KEY1: EEPROM+Flash Write  KEY2: Read&Display  KEY1+KEY2: Reset\r\n");
  
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
  BaseType_t xReturn = pdPASS;
  
  taskENTER_CRITICAL();
  
  xReturn = xTaskCreate((TaskFunction_t)Write_Task,
                        (const char*)"Write_Task",
                        (uint16_t)512,
                        (void*)NULL,
                        (UBaseType_t)2,
                        (TaskHandle_t*)&Write_Task_Handle);
  if(pdPASS == xReturn)
    printf("Write_Task created!\r\n");
  
  xReturn = xTaskCreate((TaskFunction_t)Read_Task,
                        (const char*)"Read_Task",
                        (uint16_t)1024,
                        (void*)NULL,
                        (UBaseType_t)2,
                        (TaskHandle_t*)&Read_Task_Handle);
  if(pdPASS == xReturn)
    printf("Read_Task created!\r\n");
  
  xReturn = xTaskCreate((TaskFunction_t)Reset_Task,
                        (const char*)"Reset_Task",
                        (uint16_t)512,
                        (void*)NULL,
                        (UBaseType_t)3,
                        (TaskHandle_t*)&Reset_Task_Handle);
  if(pdPASS == xReturn)
    printf("Reset_Task created!\r\n");
  
  vTaskDelete(AppTaskCreate_Handle);
  
  taskEXIT_CRITICAL();
}

/**********************************************************************
  * @ 函数名  ： Write_Task  (EEPROM + Flash 写入任务)
  ********************************************************************/
static void Write_Task(void* parameter)
{
  EventBits_t uxBits;
  uint8_t ee_data;
  uint8_t ee_addr;
  
  while (1)
  {
    uxBits = xEventGroupWaitBits(xStorageEventGroup,
                                  KEY1_EVENT_BIT,
                                  pdTRUE,
                                  pdFALSE,
                                  portMAX_DELAY);
    
    if(uxBits & KEY1_EVENT_BIT)
    {
      xEventGroupClearBits(xStorageEventGroup, KEY2_EVENT_BIT);
      
      ee_addr = (uint8_t)(g_press_cnt % EEPROM_DATA_SIZE);
      ee_data = (uint8_t)(g_press_cnt % 256);
      
      I2C_EE_BufferWrite(&ee_data, ee_addr, 1);
      
      printf("EEPROM Write: Addr=0x%02X, Data=0x%02X\r\n", ee_addr, ee_data);
      
      g_press_cnt++;
      g_log_cnt++;
      
      if(g_log_cnt > MAX_LOG_ENTRIES)
      {
        g_log_cnt = MAX_LOG_ENTRIES;
      }
      
      WriteFlashLogs();
      
      LED1_TOGGLE;
      
      vTaskDelay(pdMS_TO_TICKS(50));
      while(GPIO_ReadInputDataBit(KEY1_INT_GPIO_PORT, KEY1_INT_GPIO_PIN) == 0)
      {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      vTaskDelay(pdMS_TO_TICKS(50));
      EXTI->IMR |= KEY1_INT_EXTI_LINE;
    }
  }
}

/**********************************************************************
  * @ 函数名  ： Read_Task  (EEPROM + Flash 读取及显示任务)
  ********************************************************************/
static void Read_Task(void* parameter)
{
  EventBits_t uxBits;
  
  while (1)
  {
    uxBits = xEventGroupWaitBits(xStorageEventGroup,
                                  KEY2_EVENT_BIT,
                                  pdTRUE,
                                  pdFALSE,
                                  portMAX_DELAY);
    
    if(uxBits & KEY2_EVENT_BIT)
    {
      xEventGroupClearBits(xStorageEventGroup, KEY1_EVENT_BIT);
      
      DisplayEEPROM();
      DisplayLogs();
      
      LED2_TOGGLE;
      
      vTaskDelay(pdMS_TO_TICKS(50));
      while(GPIO_ReadInputDataBit(KEY2_INT_GPIO_PORT, KEY2_INT_GPIO_PIN) == 0)
      {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      vTaskDelay(pdMS_TO_TICKS(50));
      EXTI->IMR |= KEY2_INT_EXTI_LINE;
    }
  }
}

/**********************************************************************
  * @ 函数名  ： Reset_Task  (存储器重置任务)
  ********************************************************************/
static void Reset_Task(void* parameter)
{
  while (1)
  {
    if(GPIO_ReadInputDataBit(KEY1_INT_GPIO_PORT, KEY1_INT_GPIO_PIN) == 0 &&
       GPIO_ReadInputDataBit(KEY2_INT_GPIO_PORT, KEY2_INT_GPIO_PIN) == 0)
    {
      vTaskDelay(pdMS_TO_TICKS(30));
      
      if(GPIO_ReadInputDataBit(KEY1_INT_GPIO_PORT, KEY1_INT_GPIO_PIN) == 0 &&
         GPIO_ReadInputDataBit(KEY2_INT_GPIO_PORT, KEY2_INT_GPIO_PIN) == 0)
      {
        xEventGroupClearBits(xStorageEventGroup, KEY1_EVENT_BIT | KEY2_EVENT_BIT);
        
        EXTI->IMR &= ~(KEY1_INT_EXTI_LINE | KEY2_INT_EXTI_LINE);
        
        ResetAllMemory();
        
        DisplayEEPROM();
        DisplayNoLogs();
        
        LED1(ON);
        LED2(ON);
        vTaskDelay(500);
        LED1(OFF);
        LED2(OFF);
        
        printf("Memory Reset Complete!\r\n");
        
        while(GPIO_ReadInputDataBit(KEY1_INT_GPIO_PORT, KEY1_INT_GPIO_PIN) == 0 ||
              GPIO_ReadInputDataBit(KEY2_INT_GPIO_PORT, KEY2_INT_GPIO_PIN) == 0)
        {
          vTaskDelay(pdMS_TO_TICKS(10));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        
        EXTI->IMR |= (KEY1_INT_EXTI_LINE | KEY2_INT_EXTI_LINE);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(30));
  }
}

/***********************************************************************
  * @ 函数名  ： BSP_Init
  *********************************************************************/
static void BSP_Init(void)
{
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
  
  LED_GPIO_Config();
  Debug_USART_Config();
  Key_GPIO_Config();
  EXTI_Key_Config();
  SPI_FLASH_Init();
  I2C_EE_Init();
  
  LCD_Init();
  LCD_LayerInit();
  LTDC_Cmd(ENABLE);
  LCD_SetLayer(LCD_BACKGROUND_LAYER);
  LCD_Clear(LCD_COLOR_BLACK);
  LCD_SetLayer(LCD_FOREGROUND_LAYER);
  LCD_SetTransparency(0xFF);
  LCD_Clear(LCD_COLOR_BLACK);
  LCD_SetFont(&Font16x24);
  
  LCD_SetColors(LCD_COLOR_WHITE, LCD_COLOR_BLUE);
  LCD_DisplayStringLine(LINE(0), (uint8_t*)"  Storage Task Demo");
  LCD_SetColors(LCD_COLOR_YELLOW, LCD_COLOR_BLACK);
  LCD_DisplayStringLine(LINE(2), (uint8_t*)"  KEY1: Write EEPROM+Flash");
  LCD_DisplayStringLine(LINE(3), (uint8_t*)"  KEY2: Read & Display");
  LCD_DisplayStringLine(LINE(4), (uint8_t*)"  KEY1+KEY2: Reset Memory");
}

/***********************************************************************
  * @ 函数名  ： RecoverState
  * @ 说明    ： 上电恢复上次的 press_count 和 log_count
  *********************************************************************/
static void RecoverState(void)
{
  uint8_t ee_buf[EEPROM_DATA_SIZE];
  uint8_t flash_count_buf[4];
  uint32_t max_val = 0;
  uint8_t i;
  uint8_t found = 0;
  
  I2C_EE_BufferRead(ee_buf, EEPROM_DATA_ADDR, EEPROM_DATA_SIZE);
  for(i = 0; i < EEPROM_DATA_SIZE; i++)
  {
    if(ee_buf[i] != 0xFF)
    {
      found = 1;
      if(ee_buf[i] >= max_val)
      {
        max_val = ee_buf[i];
      }
    }
  }
  if(found)
  {
    g_press_cnt = max_val + 1;
  }
  else
  {
    g_press_cnt = 0;
  }
  
  SPI_FLASH_BufferRead(flash_count_buf, FLASH_LOG_SECTOR + FLASH_COUNT_OFFSET, 4);
  if(flash_count_buf[0] == 0xFF && flash_count_buf[1] == 0xFF &&
     flash_count_buf[2] == 0xFF && flash_count_buf[3] == 0xFF)
  {
    g_log_cnt = 0;
  }
  else
  {
    g_log_cnt = flash_count_buf[0] | ((uint32_t)flash_count_buf[1] << 8) |
                ((uint32_t)flash_count_buf[2] << 16) | ((uint32_t)flash_count_buf[3] << 24);
  }
  
  printf("Recover: press_cnt=%lu, log_cnt=%lu\r\n", g_press_cnt, g_log_cnt);
}

/***********************************************************************
  * @ 函数名  ： WriteFlashLogs
  * @ 说明    ： 将日志写入 SPI Flash
  *********************************************************************/
static void WriteFlashLogs(void)
{
  uint8_t log_buf[FLASH_LOG_ENTRY_SIZE];
  uint8_t count_buf[4];
  uint8_t ee_addr;
  uint8_t ee_data;
  uint32_t i;
  
  ee_addr = (uint8_t)((g_press_cnt - 1) % EEPROM_DATA_SIZE);
  ee_data = (uint8_t)((g_press_cnt - 1) % 256);
  
  memset(log_buf, 0, FLASH_LOG_ENTRY_SIZE);
  snprintf((char*)log_buf, FLASH_LOG_ENTRY_SIZE,
           "The user wrote byte 0x%02X at EEPROM address 0x%02X", ee_data, ee_addr);
  
  memset(g_sector_buf, 0xFF, SECTOR_BUF_SIZE);
  
  count_buf[0] = (uint8_t)(g_log_cnt & 0xFF);
  count_buf[1] = (uint8_t)((g_log_cnt >> 8) & 0xFF);
  count_buf[2] = (uint8_t)((g_log_cnt >> 16) & 0xFF);
  count_buf[3] = (uint8_t)((g_log_cnt >> 24) & 0xFF);
  memcpy(&g_sector_buf[FLASH_COUNT_OFFSET], count_buf, 4);
  
  for(i = 0; i < g_log_cnt - 1; i++)
  {
    SPI_FLASH_BufferRead(&g_sector_buf[FLASH_LOG_OFFSET + i * FLASH_LOG_ENTRY_SIZE],
                         FLASH_LOG_SECTOR + FLASH_LOG_OFFSET + i * FLASH_LOG_ENTRY_SIZE,
                         FLASH_LOG_ENTRY_SIZE);
  }
  
  memcpy(&g_sector_buf[FLASH_LOG_OFFSET + (g_log_cnt - 1) * FLASH_LOG_ENTRY_SIZE],
         log_buf, FLASH_LOG_ENTRY_SIZE);
  
  SPI_FLASH_SectorErase(FLASH_LOG_SECTOR);
  
  SPI_FLASH_BufferWrite(g_sector_buf, FLASH_LOG_SECTOR, SECTOR_BUF_SIZE);
  
  printf("Flash Log[%lu]: %s\r\n", g_log_cnt, log_buf);
}

/***********************************************************************
  * @ 函数名  ： ResetAllMemory
  * @ 说明    ： 重置EEPROM(前10字节)和Flash日志区为0xFF
  *********************************************************************/
static void ResetAllMemory(void)
{
  uint8_t ee_buf[EEPROM_DATA_SIZE];
  uint8_t count_buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t i;
  
  g_press_cnt = 0;
  g_log_cnt = 0;
  
  for(i = 0; i < EEPROM_DATA_SIZE; i++)
  {
    ee_buf[i] = 0xFF;
  }
  I2C_EE_BufferWrite(ee_buf, EEPROM_DATA_ADDR, EEPROM_DATA_SIZE);
  
  SPI_FLASH_SectorErase(FLASH_LOG_SECTOR);
  SPI_FLASH_BufferWrite(count_buf, FLASH_LOG_SECTOR + FLASH_COUNT_OFFSET, 4);
  
  printf("EEPROM and Flash Log area reset to 0xFF\r\n");
}

/***********************************************************************
  * @ 函数名  ： DisplayEEPROM
  * @ 说明    ： 在LCD上显示EEPROM前10个地址的内容
  *********************************************************************/
static void DisplayEEPROM(void)
{
  uint8_t ee_buf[EEPROM_DATA_SIZE];
  char disp_buf[100];
  uint8_t i;
  int offset;
  
  I2C_EE_BufferRead(ee_buf, EEPROM_DATA_ADDR, EEPROM_DATA_SIZE);
  
  LCD_SetColors(LCD_COLOR_WHITE, LCD_COLOR_BLUE);
  LCD_ClearLine(LINE(0));
  LCD_DisplayStringLine(LINE(0), (uint8_t*)" EEPROM:");
  
  LCD_SetColors(LCD_COLOR_WHITE, LCD_COLOR_BLACK);
  LCD_ClearLine(LINE(1));
  offset = 0;
  for(i = 0; i < EEPROM_DATA_SIZE; i++)
  {
    offset += snprintf(disp_buf + offset, sizeof(disp_buf) - offset,
                       "0x%02X ", ee_buf[i]);
  }
  LCD_DisplayStringLine(LINE(1), (uint8_t*)disp_buf);
  
  LCD_ClearLine(LINE(2));
  
  printf("EEPROM ");
  for(i = 0; i < EEPROM_DATA_SIZE; i++)
  {
    printf("0x%02X ", ee_buf[i]);
  }
  printf("\r\n");
}

/***********************************************************************
  * @ 函数名  ： DisplayLogs
  * @ 说明    ： 在LCD上显示最近5条Flash日志
  *********************************************************************/
static void DisplayLogs(void)
{
  char disp_buf[100];
  uint8_t log_buf[FLASH_LOG_ENTRY_SIZE];
  uint32_t disp_cnt;
  uint32_t start_idx;
  uint32_t i;
  uint8_t line;
  
  disp_cnt = g_log_cnt;
  if(disp_cnt > 5)
  {
    disp_cnt = 5;
  }
  
  LCD_SetColors(LCD_COLOR_WHITE, LCD_COLOR_BLUE);
  LCD_ClearLine(LINE(3));
  LCD_DisplayStringLine(LINE(3), (uint8_t*)" Logs:");
  
  if(g_log_cnt == 0)
  {
    DisplayNoLogs();
    return;
  }
  
  if(g_log_cnt > 5)
  {
    start_idx = g_log_cnt - 5;
  }
  else
  {
    start_idx = 0;
  }
  
  printf("Logs: ");
  for(i = 0; i < disp_cnt; i++)
  {
    memset(log_buf, 0, FLASH_LOG_ENTRY_SIZE);
    SPI_FLASH_BufferRead(log_buf,
                         FLASH_LOG_SECTOR + FLASH_LOG_OFFSET + (start_idx + i) * FLASH_LOG_ENTRY_SIZE,
                         FLASH_LOG_ENTRY_SIZE);
    
    line = 4 + (uint8_t)i;
    LCD_SetColors(LCD_COLOR_WHITE, LCD_COLOR_BLACK);
    LCD_ClearLine(LINE(line));
    snprintf(disp_buf, sizeof(disp_buf), " %s.", (char*)log_buf);
    LCD_DisplayStringLine(LINE(line), (uint8_t*)disp_buf);
    
    printf("%s. ", (char*)log_buf);
  }
  printf("\r\n");
  
  for(i = 4 + disp_cnt; i < 10; i++)
  {
    LCD_ClearLine(LINE((uint8_t)i));
  }
}

/***********************************************************************
  * @ 函数名  ： DisplayNoLogs
  * @ 说明    ： 在LCD上显示 "No Logs"
  *********************************************************************/
static void DisplayNoLogs(void)
{
  uint8_t i;
  
  LCD_SetColors(LCD_COLOR_YELLOW, LCD_COLOR_BLACK);
  LCD_DisplayStringLine(LINE(4), (uint8_t*)" No Logs");
  
  for(i = 5; i < 10; i++)
  {
    LCD_ClearLine(LINE(i));
  }
  
  printf("Logs: No Logs\r\n");
}

/********************************END OF FILE****************************/
