/**
  *********************************************************************
  * @file    main.c
  * @author  fire
  * @version V1.6 (Simplified)
  * @brief   FreeRTOS memory management comparison experiment
  *          Mode1: heap1 vs heap2 (free capability)
  *          Mode2: heap2 vs heap4 (fragment merge) – two-block method
  *********************************************************************
  */

#include "FreeRTOS.h"
#include "task.h"
#include "bsp_led.h"
#include "bsp_debug_usart.h"
#include "bsp_key.h"

/* Task handles */
static TaskHandle_t AppTaskCreate_Handle = NULL;
static TaskHandle_t LED_Task_Handle = NULL;
static TaskHandle_t Memory_Task_Handle = NULL;

/* Global memory pointers */
static uint8_t *Test_Ptr1 = NULL;
static uint8_t *Test_Ptr2 = NULL;
static uint8_t *Test_Ptr3 = NULL;   /* Not used in Mode2, kept for compatibility */

/* Experiment mode selection */
#define TEST_MODE_HEAP1_VS_HEAP2   1
#define TEST_MODE_HEAP2_VS_HEAP4   2

/* Set current experiment mode */
#define CURRENT_TEST_MODE  TEST_MODE_HEAP2_VS_HEAP4   /* Change as needed */

/* Mode1 parameters */
#define ALLOC_SIZE_BIG     (20*1024)

/* Mode2 parameters: two large blocks, release one to test merge */
#define BLOCK_A_SIZE       (12*1024)   /* 12 KB */
#define BLOCK_B_SIZE       (12*1024)   /* 12 KB */
#define LARGE_ALLOC_SIZE   (15*1024)   /* 15 KB, requires merging of adjacent free blocks */

/******************** Function declarations ********************/
static void AppTaskCreate(void);
static void LED_Task(void* pvParameters);
static void Memory_Task(void* pvParameters);
static void BSP_Init(void);
static void PrintMemoryStatus(uint8_t stage);

int main(void)
{
  BaseType_t xReturn = pdPASS;

  BSP_Init();
  printf("===== FreeRTOS Memory Management Experiment =====\r\n");
  #if CURRENT_TEST_MODE == TEST_MODE_HEAP1_VS_HEAP2
    printf("Mode: Compare heap1 vs heap2 (memory free)\r\n");
    printf("  heap1: NO free, heap2: free works\r\n");
  #elif CURRENT_TEST_MODE == TEST_MODE_HEAP2_VS_HEAP4
    printf("Mode: Compare heap2 vs heap4 (fragment merge)\r\n");
    printf("  heap2: no merge, heap4: adjacent free blocks merge\r\n");
  #endif
  printf("Keys: KEY1 -> Alloc   KEY2 -> Free\r\n");
  printf("================================================\r\n");

  xReturn = xTaskCreate((TaskFunction_t)AppTaskCreate,
                        "AppTaskCreate",
                        512, NULL, 1,
                        &AppTaskCreate_Handle);
  if(xReturn == pdPASS)
    vTaskStartScheduler();
  else
    return -1;

  while(1);
}

static void AppTaskCreate(void)
{
  taskENTER_CRITICAL();

  if(xTaskCreate((TaskFunction_t)LED_Task, "LED_Task",
                 512, NULL, 2, &LED_Task_Handle) == pdPASS)
    printf("LED_Task created.\r\n");

  if(xTaskCreate((TaskFunction_t)Memory_Task, "Memory_Task",
                 512, NULL, 3, &Memory_Task_Handle) == pdPASS)
    printf("Memory_Task created.\r\n");
  else
    printf("Memory_Task creation failed!\r\n");

  vTaskDelete(AppTaskCreate_Handle);
  taskEXIT_CRITICAL();
}

static void LED_Task(void* parameter)
{
  while(1)
  {
    LED1_TOGGLE;
    vTaskDelay(1000);
  }
}

/**
  * @brief  Print current memory block status
  */
static void PrintMemoryStatus(uint8_t stage)
{
  printf("[Stage %d] --- Memory Block Status ---\r\n", stage);
  printf("  Test_Ptr1 = %s\r\n", Test_Ptr1 ? "Allocated" : "NULL");
  if(Test_Ptr1) printf("    Addr: 0x%08X\r\n", (int)Test_Ptr1);
  printf("  Test_Ptr2 = %s\r\n", Test_Ptr2 ? "Allocated" : "NULL");
  if(Test_Ptr2) printf("    Addr: 0x%08X\r\n", (int)Test_Ptr2);
  printf("  Test_Ptr3 = %s\r\n", Test_Ptr3 ? "Allocated" : "NULL");
  if(Test_Ptr3) printf("    Addr: 0x%08X\r\n", (int)Test_Ptr3);
  printf("  Free heap: %d bytes\r\n", xPortGetFreeHeapSize());
  printf("------------------------------------\r\n");
}

static void Memory_Task(void* parameter)
{
  uint32_t mem_free;
  uint8_t stage = 0;

  while(1)
  {
    /* ========== KEY1: Allocate memory ========== */
    if(Key_Scan(KEY1_GPIO_PORT, KEY1_PIN) == KEY_ON)
    {
      printf("\r\n>>> KEY1 Pressed: Allocate memory <<<\r\n");

      #if CURRENT_TEST_MODE == TEST_MODE_HEAP1_VS_HEAP2
        switch(stage)
        {
          case 0:
            mem_free = xPortGetFreeHeapSize();
            printf("[Stage 0] Initial free: %d bytes\r\n", mem_free);
            printf("[Stage 0] Allocating %d bytes...\r\n", ALLOC_SIZE_BIG);
            Test_Ptr1 = pvPortMalloc(ALLOC_SIZE_BIG);
            if(Test_Ptr1)
            {
              printf("Allocation success, addr: 0x%08X\r\n", (int)Test_Ptr1);
              PrintMemoryStatus(0);
              printf("--- Press KEY2 to free ---\r\n");
              stage = 1;
            }
            else
              printf("Allocation failed!\r\n");
            break;

          case 2:
            mem_free = xPortGetFreeHeapSize();
            printf("[Stage 2] Current free: %d bytes\r\n", mem_free);
            printf("[Stage 2] Allocating %d bytes again...\r\n", ALLOC_SIZE_BIG);
            Test_Ptr1 = pvPortMalloc(ALLOC_SIZE_BIG);
            if(Test_Ptr1)
            {
              printf("Allocation success, addr: 0x%08X\r\n", (int)Test_Ptr1);
              PrintMemoryStatus(2);
              printf("\r\n>>> Observe free memory change:\r\n");
              printf("  If free < initial, mem NOT reclaimed (heap1)\r\n");
              printf("  If free ~ initial, mem reclaimed (heap2)\r\n");
              stage = 0;
            }
            else
            {
              printf("Allocation failed!\r\n");
              stage = 0;
            }
            break;

          default:
            printf("[Hint] Please press KEY2 first to free memory.\r\n");
            break;
        }

      #elif CURRENT_TEST_MODE == TEST_MODE_HEAP2_VS_HEAP4
        switch(stage)
        {
          case 0:   /* Allocate two large blocks */
            mem_free = xPortGetFreeHeapSize();
            printf("[Stage 0] Initial free: %d bytes\r\n", mem_free);
            printf("[Stage 0] Alloc Test_Ptr1 = %dKB\r\n", BLOCK_A_SIZE/1024);
            Test_Ptr1 = pvPortMalloc(BLOCK_A_SIZE);
            printf("[Stage 0] Alloc Test_Ptr2 = %dKB\r\n", BLOCK_B_SIZE/1024);
            Test_Ptr2 = pvPortMalloc(BLOCK_B_SIZE);
            if(Test_Ptr1 && Test_Ptr2)
            {
              printf("  Both blocks allocated.\r\n");
              PrintMemoryStatus(0);
              printf("--- Press KEY2 to free Test_Ptr2 ---\r\n");
              stage = 1;
            }
            else
              printf("  Allocation failed! Please adjust sizes.\r\n");
            break;

          case 2:   /* Try to allocate a block larger than each single free block */
            mem_free = xPortGetFreeHeapSize();
            printf("[Stage 2] After freeing Test_Ptr2, free: %d bytes\r\n", mem_free);
            printf("[Stage 2] Trying to allocate %dKB...\r\n", LARGE_ALLOC_SIZE/1024);
            Test_Ptr1 = pvPortMalloc(LARGE_ALLOC_SIZE);
            if(Test_Ptr1 != NULL)
            {
              printf("[Stage 2] %dKB allocation success! addr: 0x%08X\r\n",
                     LARGE_ALLOC_SIZE/1024, (int)Test_Ptr1);
              PrintMemoryStatus(2);
              printf(">>> Conclusion: Adjacent free blocks were merged (heap4 feature)\r\n");
              vPortFree(Test_Ptr1);
              Test_Ptr1 = NULL;
            }
            else
            {
              printf("[Stage 2] %dKB allocation failed!\r\n", LARGE_ALLOC_SIZE/1024);
              PrintMemoryStatus(2);
              printf(">>> Conclusion: No merge of free blocks (heap2 behavior)\r\n");
            }
            printf("--- Press KEY2 to release remaining memory ---\r\n");
            stage = 3;
            break;

          case 4:   /* Clean up and reset experiment */
            printf("========== Experiment Complete ==========\r\n");
            printf("Switch heap source and recompile to compare.\r\n");
            stage = 0;
            break;

          default:
            printf("[Hint] Please follow the operation order.\r\n");
            break;
        }
      #endif
    }

    /* ========== KEY2: Free memory ========== */
    if(Key_Scan(KEY2_GPIO_PORT, KEY2_PIN) == KEY_ON)
    {
      printf("\r\n>>> KEY2 Pressed: Free memory <<<\r\n");

      #if CURRENT_TEST_MODE == TEST_MODE_HEAP1_VS_HEAP2
        if(stage == 1 && Test_Ptr1 != NULL)
        {
          printf("[Stage 1] Freeing Test_Ptr1\r\n");
          vPortFree(Test_Ptr1);
          Test_Ptr1 = NULL;
          PrintMemoryStatus(1);
          printf("Note: vPortFree does nothing in heap1, free count will not recover.\r\n");
          printf("--- Press KEY1 to allocate again ---\r\n");
          stage = 2;
        }
        else
          printf("[Hint] No memory to free.\r\n");

      #elif CURRENT_TEST_MODE == TEST_MODE_HEAP2_VS_HEAP4
        if(stage == 1 && Test_Ptr2 != NULL)
        {
          printf("[Stage 1] Freeing Test_Ptr2 (%dKB)\r\n", BLOCK_B_SIZE/1024);
          vPortFree(Test_Ptr2);
          Test_Ptr2 = NULL;
          PrintMemoryStatus(1);
          printf("--- Press KEY1 to try allocating %dKB ---\r\n", LARGE_ALLOC_SIZE/1024);
          stage = 2;
        }
        else if(stage == 3)
        {
          if(Test_Ptr1) { vPortFree(Test_Ptr1); Test_Ptr1 = NULL; }
          if(Test_Ptr2) { vPortFree(Test_Ptr2); Test_Ptr2 = NULL; }
          PrintMemoryStatus(3);
          stage = 4;   /* Go to completion */
        }
        else
          printf("[Hint] No memory to free.\r\n");
      #endif
    }

    vTaskDelay(20);
  }
}

static void BSP_Init(void)
{
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
  LED_GPIO_Config();
  Debug_USART_Config();
  Key_GPIO_Config();
}

/********************************END OF FILE****************************/