/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-09-09     WCH        the first version
 */
#ifndef DRV_ADC_H__
#define DRV_ADC_H__
typedef struct
{
    ADC_TypeDef                   *Instance;
    ADC_InitTypeDef                Init;
}ADC_HandleTypeDef;


typedef struct
{
    uint32_t     Channel;  /* 转换通道 */
    uint32_t     Rank;     /* 转换顺序 */
    uint32_t     SamplingTime; /* 采样时间 */
    uint32_t     Offset;   /* NULL */
}ADC_ChannelConfTypeDef;


#endif
