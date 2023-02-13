/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-27     liYony       the first version
 */

#ifndef __DRV_USART_H__
#define __DRV_USART_H__
#include <rtthread.h>
#include "rtdevice.h"
#include <rthw.h>

/* Do not use GPIO_Remap*/
#define GPIO_Remap_NONE 0

/* ch32 hardware config class */
struct ch32_uart_hw_config         /* 串口设备硬件配置 */
{
    rt_uint32_t uart_periph_clock; /* 串口时钟 */
    rt_uint32_t gpio_periph_clock; /* 管脚时钟 */
    GPIO_TypeDef *tx_gpio_port;    /* 发送管脚 */
    rt_uint16_t tx_gpio_pin;       /* 具体管脚 */
    GPIO_TypeDef *rx_gpio_port;    /* 接收管脚 */
    rt_uint16_t rx_gpio_pin;       /* 具体管脚 */
    rt_uint32_t remap;
};

/* ch32 config class */
struct ch32_uart_config       /* 串口设备软件配置 */
{
    const char *name;         /* 串口设备名称*/
    USART_TypeDef *Instance;  /* 串口设备寄存器地址 */
    IRQn_Type irq_type;       /* 串口设备中断号 */
};

/* ch32 uart dirver class */
struct ch32_uart                            /* 串口设备配置 */
{
    struct ch32_uart_hw_config *hw_config;  /* 串口设备硬件配置 */
    struct ch32_uart_config *config;        /* 串口设备软件配置 */
    USART_InitTypeDef Init;                 /* 串口属性配置 */
    struct rt_serial_device serial;         /* 串口设备对象 */
};

int rt_hw_usart_init(void);
#endif
