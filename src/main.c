/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

#include "bsp/board.h"
#include "tusb.h"

#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"

#include "usb_descriptors.h"

//Prototypes
static bool checkButtons(void);
static uint8_t getButtonKeycodes(uint8_t* keycode);


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

enum  {
  BUTTON_PRESS_ACTION_KEYBOARD_KEYS = 1,
  BUTTON_PRESS_ACTION_MOUSE_BUTTONS = 2
};

// static timer
StaticTimer_t blinky_tmdef;
TimerHandle_t blinky_tm;

// static task for usbd
#define USBD_STACK_SIZE     (3*configMINIMAL_STACK_SIZE/2)
StackType_t  usb_device_stack[USBD_STACK_SIZE];
StaticTask_t usb_device_taskdef;

// static task for hid
#define HID_STACK_SZIE      configMINIMAL_STACK_SIZE
StackType_t  hid_stack[HID_STACK_SZIE];
StaticTask_t hid_taskdef;


void led_blinky_cb(TimerHandle_t xTimer);
void usb_device_task(void* param);
void hid_task(void* params);

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+
int main(void)
{
  board_init();
  tusb_init();

  //Init ButtonGP
  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitTypeDef gpioInit;
  gpioInit.Mode = GPIO_MODE_INPUT;
  gpioInit.Pull = GPIO_PULLUP;
  gpioInit.Speed = GPIO_SPEED_FREQ_MEDIUM;
  gpioInit.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9; 
  HAL_GPIO_Init(GPIOB,&gpioInit);  

  // soft timer for blinky
  blinky_tm = xTimerCreateStatic(NULL, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), true, NULL, led_blinky_cb, &blinky_tmdef);
  xTimerStart(blinky_tm, 0);

  // Create a task for tinyusb device stack
  (void) xTaskCreateStatic( usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES-1, usb_device_stack, &usb_device_taskdef);

  // Create HID task
  (void) xTaskCreateStatic( hid_task, "hid", HID_STACK_SZIE, NULL, configMAX_PRIORITIES-2, hid_stack, &hid_taskdef);

  // skip starting scheduler (and return) for ESP32-S2
#if CFG_TUSB_MCU != OPT_MCU_ESP32S2
  vTaskStartScheduler();
  NVIC_SystemReset();
  return 0;
#endif
}

#if CFG_TUSB_MCU == OPT_MCU_ESP32S2
void app_main(void)
{
  main();
}
#endif

// USB Device Driver task
// This top level thread process all usb events and invoke callbacks
void usb_device_task(void* param)
{
  (void) param;

  // RTOS forever loop
  while (1)
  {
    // tinyusb device task
    tud_task();
  }
}

//--------------------------------------------------------------------+
// DaveFunctions
//--------------------------------------------------------------------+
static bool checkButtons()
{
    if(!HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_4))
    {
      return true;
    }
    if(!HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_5))
    {
      return true;
    }
    if(!HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_6))
    {
      return true;
    }
    if(!HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_7))
    {
      return true;
    }
    if(!HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_8))
    {
      return true;
    }
    if(!HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_9))
    {
      return true;
    }
    return false;
}

/*
  return 0 no button pressed
  return 1 button results in Keyboard action
  return 2 button results in Mous action

*/
static uint8_t getButtonKeycodes(uint8_t* keycode)
{
    if(!HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_4))
    {
      keycode[0] = HID_KEY_B;
      return BUTTON_PRESS_ACTION_KEYBOARD_KEYS;
    }
    if(!HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_5))
    {
      keycode[0] = HID_KEY_C;
      return BUTTON_PRESS_ACTION_KEYBOARD_KEYS;
    }
    if(!HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_6))
    {
      keycode[0] = HID_KEY_D;
      return BUTTON_PRESS_ACTION_KEYBOARD_KEYS;
    }

    //Go Back
    if(!HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_7))
    {
      keycode[0] = MOUSE_BUTTON_BACKWARD;
      return BUTTON_PRESS_ACTION_MOUSE_BUTTONS;
    }

    //Toggle Line Comment
    if(!HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_8))
    {      
      keycode[0] = HID_KEY_CONTROL_LEFT;
      keycode[1] = HID_KEY_SHIFT_LEFT;
      keycode[1] = HID_KEY_7;
      return BUTTON_PRESS_ACTION_KEYBOARD_KEYS;
    }

    //Go Foreward
    if(!HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_9))
    {      
      keycode[0] = MOUSE_BUTTON_FORWARD;
      return BUTTON_PRESS_ACTION_MOUSE_BUTTONS;
    }

    return 0;
}


//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), 0);
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_SUSPENDED), 0);
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

void hid_task(void* param)
{
  (void) param;

  while(1)
  {
    // Poll every 10ms
    vTaskDelay(pdMS_TO_TICKS(10));

    uint32_t const btn = board_button_read();

    // Remote wakeup
    if ( tud_suspended() && btn )
    {
      // Wake up host if we are in suspend mode
      // and REMOTE_WAKEUP feature is enabled by host
      tud_remote_wakeup();
    }            
            
    uint8_t keycode[6] = { 0 };
    uint8_t mode;
    mode = getButtonKeycodes(keycode);

    if ( tud_hid_ready() && checkButtons())
    {
      if(mode == BUTTON_PRESS_ACTION_MOUSE_BUTTONS) 
      {
        tud_hid_mouse_report(REPORT_ID_MOUSE, keycode[0], 0, 0, 0, 0);
      }

      else
      {
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
      }
    }
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  // TODO set LED based on CAPLOCK, NUMLOCK etc...
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinky_cb(TimerHandle_t xTimer)
{
  (void) xTimer;
  static bool led_state = false;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
