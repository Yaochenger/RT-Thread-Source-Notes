/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-27     liYony       the first version
 */

#include "board.h"
#include <rtdevice.h>
#include <drv_usart.h>

#ifdef RT_USING_SERIAL

//#define DRV_DEBUG
#define LOG_TAG              "drv.uart"
#include <drv_log.h>

#if !defined(BSP_USING_UART1) && !defined(BSP_USING_UART2) && !defined(BSP_USING_UART3) && !defined(BSP_USING_UART4) && \
    !defined(BSP_USING_UART5) && !defined(BSP_USING_UART6) && !defined(BSP_USING_UART7) && !defined(BSP_USING_UART8)
    #error "Please define at least one BSP_USING_UARTx"
    /* this driver can be disabled at menuconfig -> RT-Thread Components -> Device Drivers */
#endif

enum  /* ���ڱ��  */
{
#ifdef BSP_USING_UART1
    UART1_INDEX,
#endif
#ifdef BSP_USING_UART2
    UART2_INDEX,
#endif
#ifdef BSP_USING_UART3
    UART3_INDEX,
#endif
#ifdef BSP_USING_UART4
    UART4_INDEX,
#endif
#ifdef BSP_USING_UART5
    UART5_INDEX,
#endif
#ifdef BSP_USING_UART6
    UART6_INDEX,
#endif
#ifdef BSP_USING_UART7
    UART7_INDEX,
#endif
#ifdef BSP_USING_UART8
    UART8_INDEX,
#endif
};

/* If you want to use other serial ports, please follow UART1 to complete other
  serial ports. For clock configuration,  */
/* �������ýṹ�� */
/*
 * ����ʱ��,�ܽ�ʱ��
 * ���ڹܽ�
 * �ܽ���ӳ��
 * */
static struct ch32_uart_hw_config uart_hw_config[] =
{
#ifdef BSP_USING_UART1
    {
        /* clock configuration, please refer to ch32v30x_rcc.h */
        RCC_APB2Periph_USART1, RCC_APB2Periph_GPIOA,
        /* GPIO  configuration : TX_Port,TX_Pin, RX_Port,RX_Pin */
        GPIOA, GPIO_Pin_9, /* Tx */GPIOA, GPIO_Pin_10, /* Rx */
        /* Whether to enable port remapping, you can refer to ch32v30x_gpio.h file,
        for example, USART1 needs to be turned on, you can use GPIO_Remap_USART1 */
        GPIO_Remap_NONE,
    },
#endif
#ifdef BSP_USING_UART2
    {
        /* clock configuration, please refer to ch32v30x_rcc.h */
        RCC_APB1Periph_USART2, RCC_APB2Periph_GPIOA,
        /* GPIO  configuration : TX_Port,TX_Pin, RX_Port,RX_Pin */
        GPIOA, GPIO_Pin_2, /* Tx */GPIOA, GPIO_Pin_3, /* Rx */
        /* Whether to enable port remapping, you can refer to ch32v30x_gpio.h file,
        for example, USART1 needs to be turned on, you can use GPIO_Remap_USART1 */
        GPIO_Remap_NONE,
    },
#endif
#ifdef BSP_USING_UART3
    {
        /* clock configuration, please refer to ch32v30x_rcc.h */
        RCC_APB1Periph_USART3, RCC_APB2Periph_GPIOB,
        /* GPIO  configuration : TX_Port,TX_Pin, RX_Port,RX_Pin */
        GPIOB, GPIO_Pin_10, /* Tx */GPIOB, GPIO_Pin_11, /* Rx */
        /* Whether to enable port remapping, you can refer to ch32v30x_gpio.h file,
        for example, USART1 needs to be turned on, you can use GPIO_Remap_USART1 */
        GPIO_Remap_NONE,
    },
#endif
#ifdef BSP_USING_UART4
    {
        /* clock configuration, please refer to ch32v30x_rcc.h */
        RCC_APB1Periph_UART4, RCC_APB2Periph_GPIOC,
        /* GPIO  configuration : TX_Port,TX_Pin, RX_Port,RX_Pin */
        GPIOC, GPIO_Pin_10, /* Tx */GPIOC, GPIO_Pin_11, /* Rx */
        /* Whether to enable port remapping, you can refer to ch32v30x_gpio.h file,
        for example, USART1 needs to be turned on, you can use GPIO_Remap_USART1 */
        GPIO_Remap_NONE,
    },
#endif
#ifdef BSP_USING_UART5
    {
        /* clock configuration, please refer to ch32v30x_rcc.h */
        RCC_APB1Periph_UART5, RCC_APB2Periph_GPIOC|RCC_APB2Periph_GPIOD,
        /* GPIO  configuration : TX_Port,TX_Pin, RX_Port,RX_Pin */
        GPIOC, GPIO_Pin_12, /* Tx */GPIOD, GPIO_Pin_2, /* Rx */
        /* Whether to enable port remapping, you can refer to ch32v30x_gpio.h file,
        for example, USART1 needs to be turned on, you can use GPIO_Remap_USART1 */
        GPIO_Remap_NONE,
    },
#endif
#ifdef BSP_USING_UART6
    {
        /* clock configuration, please refer to ch32v30x_rcc.h */
        RCC_APB1Periph_UART6, RCC_APB2Periph_GPIOC,
        /* GPIO  configuration : TX_Port,TX_Pin, RX_Port,RX_Pin */
        GPIOC, GPIO_Pin_0, /* Tx */GPIOC, GPIO_Pin_1, /* Rx */
        /* Whether to enable port remapping, you can refer to ch32v30x_gpio.h file,
        for example, USART1 needs to be turned on, you can use GPIO_Remap_USART1 */
        GPIO_Remap_NONE,
    },
#endif
#ifdef BSP_USING_UART7
    {
        /* clock configuration, please refer to ch32v30x_rcc.h */
        RCC_APB1Periph_UART7, RCC_APB2Periph_GPIOC,
        /* GPIO  configuration : TX_Port,TX_Pin, RX_Port,RX_Pin */
        GPIOC, GPIO_Pin_2, /* Tx */GPIOC, GPIO_Pin_3, /* Rx */
        /* Whether to enable port remapping, you can refer to ch32v30x_gpio.h file,
        for example, USART1 needs to be turned on, you can use GPIO_Remap_USART1 */
        GPIO_Remap_NONE,
    },
#endif
#ifdef BSP_USING_UART8
    {
        /* clock configuration, please refer to ch32v30x_rcc.h */
        RCC_APB1Periph_UART8, RCC_APB2Periph_GPIOC,
        /* GPIO  configuration : TX_Port,TX_Pin, RX_Port,RX_Pin */
        GPIOC, GPIO_Pin_4, /* Tx */GPIOC, GPIO_Pin_5, /* Rx */
        /* Whether to enable port remapping, you can refer to ch32v30x_gpio.h file,
        for example, USART1 needs to be turned on, you can use GPIO_Remap_USART1 */
        GPIO_Remap_NONE,
    },
#endif
};
/* �����豸����
 * ��1�������豸����
 * ��2�����ڼĴ�������ַ
 * ��3�������ж�Դ  */
static struct ch32_uart_config uart_config[] =
{
#ifdef BSP_USING_UART1
    {
        "uart1",
        USART1,
        USART1_IRQn,
    },
#endif
#ifdef BSP_USING_UART2
    {
        "uart2",
        USART2,
        USART2_IRQn,
    },
#endif
#ifdef BSP_USING_UART3
    {
        "uart3",
        USART3,
        USART3_IRQn,
    },
#endif
#ifdef BSP_USING_UART4
    {
        "uart4",
        UART4,
        UART4_IRQn,
    },
#endif
#ifdef BSP_USING_UART5
    {
        "uart5",
        UART5,
        UART5_IRQn,
    },
#endif
#ifdef BSP_USING_UART6
    {
        "uart6",
        UART6,
        UART6_IRQn,
    },
#endif
#ifdef BSP_USING_UART7
    {
        "uart7",
        UART7,
        UART7_IRQn,
    },
#endif
#ifdef BSP_USING_UART8
    {
        "uart8",
        UART8,
        UART8_IRQn,
    },
#endif
};
/* ���ڳ�ʼ�� ����*/
static struct ch32_uart uart_obj[sizeof(uart_config) / sizeof(uart_config[0])] = {0};
/* �������ú���
 * ��1�������豸  rt_serial_device
 * ��2����������  serial_configure */
static rt_err_t ch32_configure(struct rt_serial_device *serial, struct serial_configure *cfg)
{
    struct ch32_uart *uart; /* �����豸 */
    GPIO_InitTypeDef GPIO_InitStructure={0}; /* �ܽ����� */

    RT_ASSERT(serial != RT_NULL); /* �ж��豸�Ƿ���Ч */
    RT_ASSERT(cfg != RT_NULL);    /* �ж������Ƿ�Ϊ��Ч */

    uart = (struct ch32_uart *) serial->parent.user_data; /* ��ȡ�������� */

    uart->Init.USART_BaudRate             = cfg->baud_rate;                /* ���ڲ����� */
    uart->Init.USART_HardwareFlowControl  = USART_HardwareFlowControl_None;/* ��Ӳ��������  */
    uart->Init.USART_Mode                 = USART_Mode_Rx|USART_Mode_Tx;   /*  ����ģʽ*/

    switch (cfg->data_bits)/* �������ݳ��� */
    {
    case DATA_BITS_8:
        uart->Init.USART_WordLength = USART_WordLength_8b;
        break;
    case DATA_BITS_9:
        uart->Init.USART_WordLength = USART_WordLength_9b;
        break;
    default:
        uart->Init.USART_WordLength = USART_WordLength_8b;
        break;
    }

    switch (cfg->stop_bits)/*  ����ֹͣλ���� */
    {
    case STOP_BITS_1:
        uart->Init.USART_StopBits   = USART_StopBits_1;
        break;
    case STOP_BITS_2:
        uart->Init.USART_StopBits   = USART_StopBits_2;
        break;
    default:
        uart->Init.USART_StopBits   = USART_StopBits_1;
        break;
    }
    switch (cfg->parity)/*  ����У�� */
    {
    case PARITY_NONE:
        uart->Init.USART_Parity    = USART_Parity_No;
        break;
    case PARITY_ODD:
        uart->Init.USART_Parity    = USART_Parity_Odd;
        break;
    case PARITY_EVEN:
        uart->Init.USART_Parity    = USART_Parity_Even;
        break;
    default:
        uart->Init.USART_Parity     = USART_Parity_No;
        break;
    }

    /* UART hardware configuration, including clock and GPIO, etc. */
    /*  ����ʱ�� */
    RCC_APB2PeriphClockCmd(uart->hw_config->gpio_periph_clock, ENABLE);
    if(uart->config->Instance == USART1)
    {
        RCC_APB2PeriphClockCmd(uart->hw_config->uart_periph_clock, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(uart->hw_config->uart_periph_clock, ENABLE);
    }
    /* ������ӳ��  */
    if(uart->hw_config->remap != GPIO_Remap_NONE)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
        GPIO_PinRemapConfig(uart->hw_config->remap, ENABLE);
    }
    /* Ӳ������  */
    GPIO_InitStructure.GPIO_Pin = uart->hw_config->tx_gpio_pin;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(uart->hw_config->tx_gpio_port, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = uart->hw_config->rx_gpio_pin;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(uart->hw_config->rx_gpio_port, &GPIO_InitStructure);
    /*  Ӳ����ʼ�� */
    USART_Init(uart->config->Instance,&uart->Init);
    USART_Cmd(uart->config->Instance, ENABLE);

    return RT_EOK;
}

/*  �豸���� */
static rt_err_t ch32_control(struct rt_serial_device *serial, int cmd, void *arg)
{
    struct ch32_uart *uart;/* ���ڲ����ӿ� */
    RT_ASSERT(serial != RT_NULL);
    uart = (struct ch32_uart *)serial->parent.user_data; /* ��ȡ�豸�������� */
    switch (cmd)
    {
    /* ���ж�  */
    case RT_DEVICE_CTRL_CLR_INT:
        /* disable rx irq */
        NVIC_DisableIRQ(uart->config->irq_type);
        /* disable interrupt */
        USART_ITConfig(uart->config->Instance,USART_IT_RXNE,DISABLE);
        break;
    /* ʹ���ж� */
    case RT_DEVICE_CTRL_SET_INT:
        /* enable rx irq */
        NVIC_EnableIRQ(uart->config->irq_type);
        /* enable interrupt */
        USART_ITConfig(uart->config->Instance, USART_IT_RXNE,ENABLE);
        break;
    }
    return RT_EOK;
}
/*  �������
 *  ��1�������豸  rt_serial_device
 *  ��2������ַ�
 *  */
static int ch32_putc(struct rt_serial_device *serial, char c)
{
    struct ch32_uart *uart;
    RT_ASSERT(serial != RT_NULL);
    uart = (struct ch32_uart *)serial->parent.user_data;/* ��ȡ���ڿ������� */
    while (USART_GetFlagStatus(uart->config->Instance, USART_FLAG_TC) == RESET);
    uart->config->Instance->DATAR = c;
    return 1;
}
/* ���ڽ���
 * ��1�������豸  rt_serial_device
 *  */
static int ch32_getc(struct rt_serial_device *serial)
{
    int ch;
    struct ch32_uart *uart;
    RT_ASSERT(serial != RT_NULL);
    uart = (struct ch32_uart *)serial->parent.user_data;/* ��ȡ���ڿ������� */
    ch = -1;
    if (USART_GetFlagStatus(uart->config->Instance, USART_FLAG_RXNE) != RESET)
    {
        ch = uart->config->Instance->DATAR & 0xff;
    }
    return ch;
}

rt_size_t ch32dma_transmit(struct rt_serial_device *serial, rt_uint8_t *buf, rt_size_t size, int direction)
{
    return RT_EOK;
}

/* �����жϰ�
 * ��1�������豸  rt_serial_device
 *   */
static void uart_isr(struct rt_serial_device *serial)
{
    struct ch32_uart *uart = (struct ch32_uart *) serial->parent.user_data;/* ��ȡ���ڿ������� */
    RT_ASSERT(uart != RT_NULL);
    if (USART_GetITStatus(uart->config->Instance, USART_IT_RXNE) != RESET)
    {
        /* �����жϰ�  */
        rt_hw_serial_isr(serial, RT_SERIAL_EVENT_RX_IND);
        USART_ClearITPendingBit(uart->config->Instance, USART_IT_RXNE);
    }
}
/* ����ops��ʼ�� */
static const struct rt_uart_ops ch32_uart_ops =
{
    ch32_configure,
    ch32_control,
    ch32_putc,
    ch32_getc,
    ch32dma_transmit
};

#ifdef BSP_USING_UART1
void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    uart_isr(&(uart_obj[UART1_INDEX].serial));
    rt_interrupt_leave();
    FREE_INT_SP();
}
#endif

#ifdef BSP_USING_UART2
void USART2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART2_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    uart_isr(&(uart_obj[UART2_INDEX].serial));
    rt_interrupt_leave();
    FREE_INT_SP();
}
#endif

#ifdef BSP_USING_UART3
void USART3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART3_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    uart_isr(&(uart_obj[UART3_INDEX].serial));
    rt_interrupt_leave();
    FREE_INT_SP();
}
#endif

#ifdef BSP_USING_UART4
void UART4_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void UART4_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    uart_isr(&(uart_obj[UART4_INDEX].serial));
    rt_interrupt_leave();
    FREE_INT_SP();
}
#endif

#ifdef BSP_USING_UART5
void UART5_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void UART5_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    uart_isr(&(uart_obj[UART5_INDEX].serial));
    rt_interrupt_leave();
    FREE_INT_SP();
}
#endif

#ifdef BSP_USING_UART6
void UART6_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void UART6_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    uart_isr(&(uart_obj[UART6_INDEX].serial));
    rt_interrupt_leave();
    FREE_INT_SP();
}
#endif

#ifdef BSP_USING_UART7
void UART7_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void UART7_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    uart_isr(&(uart_obj[UART7_INDEX].serial));
    rt_interrupt_leave();
    FREE_INT_SP();
}
#endif

#ifdef BSP_USING_UART8
void UART8_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void UART8_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    uart_isr(&(uart_obj[UART8_INDEX].serial));
    rt_interrupt_leave();
    FREE_INT_SP();
}
#endif

int rt_hw_usart_init(void)
{
    /*  �����豸����ĸ��� */
    rt_size_t obj_num = sizeof(uart_obj) / sizeof(struct ch32_uart);
    /*  �����豸���� :Ĭ������ */
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    /*  ��ʱ���� */
    rt_err_t result = 0;
    /*  ѭ����ʼ�������豸 */
    for (int i = 0; i < obj_num; i++)
    {
        /* ��ʼ�����ڶ���  */
        uart_obj[i].config        = &uart_config[i];     /* �������� */
        uart_obj[i].hw_config     = &uart_hw_config[i];  /* ����Ӳ������ */
        uart_obj[i].serial.ops    = &ch32_uart_ops;      /* ����ops���� */
        uart_obj[i].serial.config = config;              /* �����豸���� */
        /* Hardware initialization is required, otherwise it
        will not be registered into the device framework */
        if(uart_obj[i].hw_config->gpio_periph_clock == 0)
        {
            LOG_E("You did not perform hardware initialization for %s", uart_obj[i].config->name);
            continue;
        }
        if(uart_obj[i].hw_config->uart_periph_clock == 0)
        {
            LOG_E("You did not perform hardware initialization for %s", uart_obj[i].config->name);
            continue;
        }
        /* ע�ᴮ���豸
         * ��1���豸 rt_device
         * ��2���豸����
         * ��3���豸����
         * ��4�������豸��������  */
        result = rt_hw_serial_register(&uart_obj[i].serial, uart_obj[i].config->name,
                                       RT_DEVICE_FLAG_RDWR
                                       | RT_DEVICE_FLAG_INT_RX
                                       , &uart_obj[i]);
        RT_ASSERT(result == RT_EOK);
    }

    return result;
}

#endif /* RT_USING_SERIAL */
