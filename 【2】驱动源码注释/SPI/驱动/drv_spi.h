/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-5      SummerGift   first version
 */

#ifndef __DRV_SPI_H__
#define __DRV_SPI_H__

#include <rtthread.h>
#include "rtdevice.h"
#include <rthw.h>
#include "board.h"

/* SPI总线CS控制块 */
struct ch32_hw_spi_cs
{
    GPIO_TypeDef* GPIOx;
    uint16_t GPIO_Pin;
};
/* SPI总线配置 */
struct ch32_spi_config
{
    SPI_TypeDef *Instance;
    char *bus_name;
};
/* SPI设备（挂）控制块 */
struct ch32_spi_device
{
    rt_uint32_t pin;
    char *bus_name;
    char *device_name;
};

#define SPI_USING_RX_DMA_FLAG   (1<<0)
#define SPI_USING_TX_DMA_FLAG   (1<<1)

/* SPI总线控制块 */
typedef struct _SPI_HandleType
{
    SPI_TypeDef                *Instance;
    SPI_InitTypeDef            Init;
    uint8_t                    *pTxBuffPtr;
    uint16_t                   TxXferSize;
    volatile uint16_t          TxXferCount;
    uint8_t                    *pRxBuffPtr;    /*!< Pointer to SPI Rx transfer Buffer        */
    uint16_t                   RxXferSize;     /*!< SPI Rx Transfer size                     */
    volatile uint16_t          RxXferCount;    /*!< SPI Rx Transfer Counter                  */
}SPI_HandleTypeDef;

/* SPI总线设备 */
struct ch32_spi
{
    SPI_HandleTypeDef handle;
    struct ch32_spi_config *config;
    struct rt_spi_configuration *cfg;
    struct rt_spi_bus spi_bus;
};

/* SPI设备（挂）挂载函数 */
rt_err_t rt_hw_spi_device_attach(const char *bus_name, const char *device_name, GPIO_TypeDef* cs_gpiox, uint16_t cs_gpio_pin);

#endif /*__DRV_SPI_H__ */
