/**
  ************************************************************************
  * @file    main.c
  * @author  fire
  * @version V1.0
  * @date    2018-xx-xx
  * @brief   FreeRTOS多任务调度基础实验
  *           5个任务调度实验
  ************************************************************************
  * @attention
  *
  * 实验平台:野火 STM32 系列 开发板
  * 
  * 论坛    :http://www.firebbs.cn
  * 淘宝    :https://fire-stm32.taobao.com
  *
  ************************************************************************
  */
  
/*
*************************************************************************
*                             包含头文件
*************************************************************************
*/
#include "FreeRTOS.h"
#include "task.h"

/*
*************************************************************************
*                              全局变量
*************************************************************************
*/
portCHAR flag1;
portCHAR flag2;
portCHAR flag3;
portCHAR flag4;
portCHAR flag5;

extern List_t pxReadyTasksLists[ configMAX_PRIORITIES ];

/*
*************************************************************************
*                        任务控制块 & STACK 
*************************************************************************
*/
TaskHandle_t Task1_Handle;
#define TASK1_STACK_SIZE                    128
StackType_t Task1Stack[TASK1_STACK_SIZE];
TCB_t Task1TCB;

TaskHandle_t Task2_Handle;
#define TASK2_STACK_SIZE                    128
StackType_t Task2Stack[TASK2_STACK_SIZE];
TCB_t Task2TCB;

TaskHandle_t Task3_Handle;
#define TASK3_STACK_SIZE                    128
StackType_t Task3Stack[TASK3_STACK_SIZE];
TCB_t Task3TCB;

TaskHandle_t Task4_Handle;
#define TASK4_STACK_SIZE                    128
StackType_t Task4Stack[TASK4_STACK_SIZE];
TCB_t Task4TCB;

TaskHandle_t Task5_Handle;
#define TASK5_STACK_SIZE                    128
StackType_t Task5Stack[TASK5_STACK_SIZE];
TCB_t Task5TCB;

/*
*************************************************************************
*                               任务声明 
*************************************************************************
*/
void delay (uint32_t count);
void Task1_Entry( void *p_arg );
void Task2_Entry( void *p_arg );
void Task3_Entry( void *p_arg );
void Task4_Entry( void *p_arg );
void Task5_Entry( void *p_arg );


/*
************************************************************************
*                                main函数
************************************************************************
*/
int main(void)
{	
    /* 硬件初始化 */
	/* 与硬件相关的初始化放在这里，比如GPIO初始化等 */
    
    /* 
     * 任务优先级设计说明（tick=10ms）：
     * - Task3: 优先级4（最高），每30ms周期，高电平10ms，需打断Task1和Task2
     * - Task1: 优先级3，每1ms翻转1次（软件延时实现），然后阻塞让出CPU
     * - Task2: 优先级3，每2ms翻转1次（软件延时实现），然后阻塞让出CPU
     * - Task5: 优先级2，每10ms翻转1次
     * - Task4: 优先级1（最低），每10ms翻转1次
     */
    
    /* 创建任务3 - 最高优先级，需要打断Task1和Task2 */
    Task3_Handle = xTaskCreateStatic( (TaskFunction_t)Task3_Entry,   
					                  (char *)"Task3",               
					                  (uint32_t)TASK3_STACK_SIZE ,   
					                  (void *) NULL,                 
                                      (UBaseType_t) 4,               
					                  (StackType_t *)Task3Stack,     
					                  (TCB_t *)&Task3TCB );          
    
    /* 创建任务1 - 高优先级 */
    Task1_Handle = xTaskCreateStatic( (TaskFunction_t)Task1_Entry,   
					                  (char *)"Task1",               
					                  (uint32_t)TASK1_STACK_SIZE ,   
					                  (void *) NULL,                 
                                      (UBaseType_t) 3,               
					                  (StackType_t *)Task1Stack,     
					                  (TCB_t *)&Task1TCB );          
                                
    /* 创建任务2 - 与任务1同优先级，交替运行 */
    Task2_Handle = xTaskCreateStatic( (TaskFunction_t)Task2_Entry,   
					                  (char *)"Task2",               
					                  (uint32_t)TASK2_STACK_SIZE ,   
					                  (void *) NULL,                 
                                      (UBaseType_t) 3,               
					                  (StackType_t *)Task2Stack,     
					                  (TCB_t *)&Task2TCB );          
    
    /* 创建任务5 - 中等优先级 */
    Task5_Handle = xTaskCreateStatic( (TaskFunction_t)Task5_Entry,   
					                  (char *)"Task5",               
					                  (uint32_t)TASK5_STACK_SIZE ,   
					                  (void *) NULL,                 
                                      (UBaseType_t) 2,               
					                  (StackType_t *)Task5Stack,     
					                  (TCB_t *)&Task5TCB );          
    
    /* 创建任务4 - 最低优先级 */
    Task4_Handle = xTaskCreateStatic( (TaskFunction_t)Task4_Entry,   
					                  (char *)"Task4",               
					                  (uint32_t)TASK4_STACK_SIZE ,   
					                  (void *) NULL,                 
                                      (UBaseType_t) 1,               
					                  (StackType_t *)Task4Stack,     
					                  (TCB_t *)&Task4TCB );          
    
    /* 启动任务调度器前先关闭中断 */                                  
    portDISABLE_INTERRUPTS();
                                      
    /* 启动任务调度器，初始化成功则不返回 */
    vTaskStartScheduler();                                      
    
    for(;;)
	{
		/* 系统启动成功不会到达这里 */
	}
}

/*
*************************************************************************
*                               任务实现
*************************************************************************
*/
/* 软件延时 - 简单的循环延时 */
void delay (uint32_t count)
{
	for(; count!=0; count--);
}

/* 
 * 任务1：控制flag1，每1ms翻转1次
 * 优先级：3，与任务2同优先级交替运行
 * 使用软件延时delay实现1ms延时
 * 然后调用vTaskDelay(1)进入阻塞状态，让低优先级任务有机会运行
 * 
 * 执行流程：
 * 1. flag1=1
 * 2. 软件延时1ms
 * 3. vTaskDelay(1)阻塞10ms（让出CPU给Task4/Task5）
 * 4. flag1=0
 * 5. 软件延时1ms
 * 6. vTaskDelay(1)阻塞10ms
 * 7. 循环...
 */
void Task1_Entry( void *p_arg )
{
	for( ;; )
	{
		flag1 = 1;
        delay( 1000 );
//        vTaskDelay( 1 );
		
		flag1 = 0;
        delay( 1000 );
//        vTaskDelay( 1 );      
	}
}

/* 
 * 任务2：控制flag2，每2ms翻转1次
 * 优先级：3，与任务1同优先级交替运行
 * 使用软件延时delay实现2ms延时
 * 然后调用vTaskDelay(1)进入阻塞状态，让低优先级任务有机会运行
 */
void Task2_Entry( void *p_arg )
{
	for( ;; )
	{
		flag2 = 1;
        delay( 2000 );
//        vTaskDelay( 1 );
		
		flag2 = 0;
        delay( 2000 );
//        vTaskDelay( 1 );       
	}
}

/* 
 * 任务3：控制flag3，每30ms一个周期
 * 高电平维持10ms，其余时间（20ms）低电平
 * 优先级：4（最高），可打断任务1和任务2
 * 使用vTaskDelay实现
 */
void Task3_Entry( void *p_arg )
{
	for( ;; )
	{
        flag3 = 1;
        vTaskDelay( 1 );
		
		flag3 = 0;
        vTaskDelay( 2 );       
	}
}

/* 
 * 任务4：控制flag4，每10ms翻转1次
 * 优先级：1（最低），只有在其他任务都阻塞时才能运行
 */
void Task4_Entry( void *p_arg )
{
	for( ;; )
	{
		flag4 = 1;
        vTaskDelay( 1 );
		
		flag4 = 0;
        vTaskDelay( 1 );       
	}
}

/* 
 * 任务5：控制flag5，每10ms翻转1次
 * 优先级：2，中等优先级
 */
void Task5_Entry( void *p_arg )
{
	for( ;; )
	{
		flag5 = 1;
        vTaskDelay( 1 );
		
		flag5 = 0;
        vTaskDelay( 1 );       
	}
}


/* 获取空闲任务内存 */
StackType_t IdleTaskStack[configMINIMAL_STACK_SIZE];
TCB_t IdleTaskTCB;
void vApplicationGetIdleTaskMemory( TCB_t **ppxIdleTaskTCBBuffer, 
                                    StackType_t **ppxIdleTaskStackBuffer, 
                                    uint32_t *pulIdleTaskStackSize )
{
		*ppxIdleTaskTCBBuffer=&IdleTaskTCB;
		*ppxIdleTaskStackBuffer=IdleTaskStack; 
		*pulIdleTaskStackSize=configMINIMAL_STACK_SIZE;
}
