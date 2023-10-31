/*
 * MIT License -- Copyright (c) 2023 James Edward Hume -- See LICENSE file in
 * repository root.
 *
 * This is a really junk, hence JunkOS, example of the most basic type of OS
 * possible... I'm just goofing around really!.
 *
 * Here, every "thread" is really just a function that when called, runs to
 * completion and is never pre-empted. The thread run function is called
 * every time the thread is set as being runnable. This could, for example,
 * be in response to an interrupt.
 *
 * For example, when transmitting serial data, the send thread writes a char
 * to the data register and stops. When the data register empty interrupt
 * fires, it makes the thread runnable again. The thread function will then,
 * at some point, be again called by the scheduler and it can send the next
 * bit of data and so on...
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "junkos_scheduler.h"
#include "stm32f401xe.h"

void send_data_task(void);
void led_task(void);


#define JUNKOS_TASK_DATA ((junkos_task_id_t) 1)
#define JUNKOS_TASK_LED  ((junkos_task_id_t) 2)

junkos_task_t junkos_tasks[] =	{
	{ .run = send_data_task, .id = JUNKOS_TASK_DATA, .priority = 1, .auto_run = true },
	{ .run = led_task,       .id = JUNKOS_TASK_LED,  .priority = 1, .auto_run = false },
};

#define JUNKOS_NUM_TASKS (sizeof(junkos_tasks)/sizeof(junkos_tasks[0]))

void gpio_a_clk_ena(void)
{
	static bool is_enabled = false;

	if (!is_enabled)
	{
		/* Enable clock to GPIO_A bank */
		RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
		is_enabled = true;
	}
}

void led_init(void)
{
	gpio_a_clk_ena();

	/* Set PA5 to be a general purpose push-pull output type */
	/* Make it push pull */
	GPIOA->OTYPER &= GPIO_OTYPER_OT5;

	/* Make it a general purpose output */
	GPIOA->MODER &= ~GPIO_MODER_MODER5;
	GPIOA->MODER |= GPIO_MODER_MODER5_0;
}


void led_set(const bool is_on)
{
	if (is_on)
	{
		GPIOA->ODR |= GPIO_ODR_OD5;
	}
	else
	{
		GPIOA->ODR &= ~GPIO_ODR_OD5;
	}
}

void systick_init(void)
{
	// The HSI clock signal is generated from an internal 16 MHz RC oscillator.
	// After a system reset, the HSI oscillator is selected as the system clock.
	//
	// The RCC feeds the external clock of the Cortex System Timer (SysTick) with the AHB clock
	// (HCLK) divided by 8. The SysTick can work either with this clock or with the Cortex clock
	// (HCLK), configurable in the SysTick control and status register
	//
	// The SysTick calibration value is fixed to 10500 (0x2904), which gives a reference time base of 1 ms
	// with the SysTick clock set to 10.5 MHz (HCLK/8, with HCLK set to 84 MHz).
	//
	// ^^ From TM32F401xB/C and STM32F401xD/E advanced Arm®-based 32-bit MCUs - Reference manual
	//
	//
	// So. HSI by default is 16Mhz and this feeds the SYSCLK. Then div by AHB_PRESC (default is 1)
	// and then maybe by 8 ,depending on which clock feeds it, to get systick
	// So 16Mhz / 1 == 16Mhz
	//
	// SysTick X ticks per second. So 10ms = X / 100 ticks per second. So need to set count down
	// value to 16,000,000 / 100 == 160,000
	//
	// The RELOAD value is calculated according to its use. For example, to generate a multi-shot
	// timer with a period of N processor clock cycles, use a RELOAD value of N-1. If the SysTick
	// interrupt is required every 100 clock pulses, set RELOAD to 99
	SysTick_Config(16000000);
	__NVIC_EnableIRQ(SysTick_IRQn);
}

void led_task(void)
{
	static struct {
		bool led_on;
	} context = { false };

	context.led_on = !context.led_on;
	led_set(context.led_on);
}

void SysTick_Handler()
{
	junkos_scheduler_set_task_runnable(JUNKOS_TASK_LED);
}

#define GPIO_AFRL_AFSEL2_AF7 (GPIO_AFRL_AFSEL2_0 | GPIO_AFRL_AFSEL2_1 | GPIO_AFRL_AFSEL2_2)
#define GPIO_AFRL_AFSEL3_AF7 (GPIO_AFRL_AFSEL3_0 | GPIO_AFRL_AFSEL3_1 | GPIO_AFRL_AFSEL3_2)

#define GPIO_MODER_ALT_FUNCTION_MODE 2

void uart_init(void)
{
	/* The GPIO_A module is required. */
	gpio_a_clk_ena();

	/* Configure PA2 to be the UART2_TX pin and and PA3 to be the UART2_RX pin. */
	GPIOA->MODER &= ~(GPIO_MODER_MODER2 | GPIO_MODER_MODER3);
	GPIOA->MODER |= (GPIO_MODER_ALT_FUNCTION_MODE << GPIO_MODER_MODER2_Pos) | (GPIO_MODER_ALT_FUNCTION_MODE << GPIO_MODER_MODER3_Pos);
	GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL2 | GPIO_AFRL_AFSEL3);
	GPIOA->AFR[0] |= GPIO_AFRL_AFSEL2_AF7 | GPIO_AFRL_AFSEL3_AF7;

	/* Enable the clock to the USART 2 module */
	RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

	USART2->CR1 &= ~USART_CR1_M;     /* 8 data bits, 1 start and n stop bit(s). */
	USART2->CR2 &= ~USART_CR2_STOP;  /* 1 stop bit */
	USART2->CR1 &= ~USART_CR1_OVER8; /* x16 over sampling for robustness to clock deviations */

	/* From 19.3.4: Tx/Rx Baud =             f_ck
	 *                           ----------------------------
	 *                           (8 * (2 - OVER8) * USARTDIV)
	 *
	 * and from figure 167: USARTDIV = DIV_Mantissa + (DIV_Fraction / 8 * (2 - OVER8))
	 *
	 * USARTDIV is an unsigned fixed point number that is coded on the USART_BRR register.
     *     When OVER8=0, the fractional part is coded on 4 bits and programmed by the
     *                   DIV_fraction[3:0] bits in the USART_BRR register
     *     When OVER8=1, the fractional part is coded on 3 bits and programmed by the
     *                   DIV_fraction[2:0] bits in the USART_BRR register, and bit DIV_fraction[3]
     *                   must be kept cleared.
     *
     * I want OVER8 === 0 to increase the tolerance of the receiver to clock deviations, limiting
     * the max speed to f_pclk/16.
     *
     * Initial
     *
     * In my case USARTDIV = DIV_Mantissa + (DIV_Fraction / 16)
     * Thus
     *            Tx/Rx Baud =                     16 MHz
	 *                           ------------------------------------------
	 *                           (16 * (DIV_Mantissa + (DIV_Fraction / 16)))
	 *
	 *                       =                     1 MHz
	 *                           ------------------------------------------
	 *                               (DIV_Mantissa + (DIV_Fraction / 16))
     *
     * Therefore for a particular baud rate...
     *
     *           DIV_Mantissa + (DIV_Fraction / 16) =  1 MHz
     *                                                 ----------
     *                                                    Baud
     *
     * So for a baud of 9600, for example
     *
     *           DIV_Mantissa + (DIV_Fraction / 16) = 1,000,000 / 9,600 = 104.1666....
     *
     *  Therefore DIV_Mantissa = 104
     *  DIV_Fraction = 0.16666... 0.16666... = F / 16 => F = 2.66666,. round to 3
     *
     *  So USARTDIV = (104 << 4) | 3
	 */
	USART2->BRR = ((104 << USART_BRR_DIV_Mantissa_Pos) & USART_BRR_DIV_Mantissa_Msk) |
			      ((3 << USART_BRR_DIV_Fraction_Pos) & USART_BRR_DIV_Fraction_Msk);

	USART2->CR1 |= USART_CR1_UE;     /* Enable the USART */
}


void USART2_IRQHandler()
{
	if (USART2->SR & USART_SR_TXE)
	{
		__NVIC_DisableIRQ(USART2_IRQn); // Otherwise we'll continually get this until the next character write...
		junkos_scheduler_set_task_runnable(JUNKOS_TASK_DATA);
	}
}


void uart_send_start(void)
{
	USART2->CR1 |= USART_CR1_TE;
	USART2->CR1 |= USART_CR1_TXEIE;
}


void uart_send_char(const uint8_t c)
{
	/* Wait for the transmit data register to be empty */
	//while((USART2->SR & USART_SR_TXE) == 0) { }

	/* TX1 == 1, so data has been transferred to the shift register, data register now empty */

	/* Write to the data register - the register bits 31:4 must be kept at their reset value
	 * so do a read/write - could be better and just read those bits once on init but being lazy */
	uint32_t dr_val = USART2->DR;
	dr_val &= ~(uint32_t)0xF;
	dr_val |= c;
	USART2->DR = dr_val;
	__NVIC_EnableIRQ(USART2_IRQn);
}

void uart_send_stop(void)
{
	/* Wait for the transmit data register to be empty */
	//while((USART2->SR & USART_SR_TXE) == 0) { }

	/* Wait for transmission complete */
	while((USART2->SR & USART_SR_TC) == 0) { }

	/* Disable the transmitter */
	USART2->CR1 &= ~USART_CR1_TXEIE;
	USART2->CR1 &= ~USART_CR1_TE;
	__NVIC_DisableIRQ(USART2_IRQn);
}

void send_data_task(void)
{
	static struct {
		bool initialised;
		unsigned int idx;
	} context = {false, 0};

	if (!context.initialised)
	{
		uart_send_start();
		context.initialised = true;
	}
	uart_send_char('1' + (char)context.idx);

	context.idx += 1;
	if (context.idx >= 9)
	{
		context.idx = 0;
	}
}



int main(void)
{
	junkos_scheduler_init(junkos_tasks, JUNKOS_NUM_TASKS);

	led_init();
	led_set(false);
	uart_init();
	systick_init();

	junkos_scheduler();
}
