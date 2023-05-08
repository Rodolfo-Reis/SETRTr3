/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <zephyr/device.h> /* device_is_ready and device struct */
#include <zephyr/devicetree.h> /* DT_NODELABEL() */
#include <zephyr/sys/printk.h> /* printk */
#include <zephyr/drivers/gpio.h> /* GPIO api */

/* Use a "big" sleep time to reduce CPU load (button detection int activated, not polled) */
#define SLEEP_TIME_MS   60*1000 

/* set de pins used */
/* buttons 1-4 on board */
/* buttons 5-8 connected labeled A0...A3 (gpio pin 3,4,28,29) */
const uint8_t buttons_pins[] = { 11,12,24,25,3,4,28,29};

//----------------------------------------------------------------
/* Definicao de variaveis */
/* Definir eventos */
typedef enum {
    NONE, ADD1, ADD2, ADD5, ADD10, UP, DOWN, SEL, RET
} Event;

/* Definir estados */
typedef enum{
    MENU, PAY_CHECK, MOVIES, UPDATE_CREDIT
} States;

/* definir variavel evento */
static volatile Event eventos = NONE;
/* definir variavel estado */
static volatile States estado = MENU;
/* definir variavel credito */
static volatile int Credito = 0;

/* Listas de filmes, horas e preço */
static volatile int num_movie = 5;
static volatile char Movie[]= {'A', 'A', 'A', 'B', 'B'};
static volatile int Hora[] = {19, 21, 23, 19, 21}; 
static volatile int Preco[]= {9, 11, 9, 10, 12};
static volatile int movie_idx = -1;



//---------------------------------------------------------
/* Get node ID for GPIO0, which has leds and buttons */ 
#define GPIO0_NODE DT_NODELABEL(gpio0)

/* Now get the device pointer for GPIO0 */
static const struct device * gpio0_dev = DEVICE_DT_GET(GPIO0_NODE);

/* Define a variable of type static struct gpio_callback, which will latter be used to install the callback
*  It defines e.g. which pin triggers the callback and the address of the function */
static struct gpio_callback button_cb_data;

/* Define a callback function. It is like an ISR (and runs in the cotext of an ISR) */
/* that is called when the button is pressed */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	int i=0;
	/* Identify the button(s) that was(ere) hit*/
	for(i=0; i<sizeof(buttons_pins); i++){		
		if(BIT(buttons_pins[i]) & pins) {
			/* add 1 euro*/
			if(i==0){
				eventos = ADD1;
			}
			/* add 2 euro*/
			else if(i==1){
				eventos = ADD2;
			}
			/* add 5 euro*/
			else if(i==2){
				eventos = ADD5;
			}
			/* add 10 euro*/
			else if(i==3){
				eventos = ADD10;
			}
			/* Up */
			else if(i==4){
				eventos = UP;
			}
			/* Down */
			else if(i==5){
				eventos = DOWN;
			}
			/* Pay check*/
			else if(i==6){
				eventos = SEL;
			}
			/* Return */
			else if(i==7){
				eventos = RET;
			}
			else{
				eventos = NONE;
			}
		}
	} 
}


void main(void)
{
    int i,ret;
    uint32_t pinmask = 0; /* Mask for setting the pins that shall generate interrupts */

	/* Configure the GPIO pins - buttons 1-4 + IOPINS 2,4,28 and 29 for input*/
    for(i=0; i<sizeof(buttons_pins); i++) {
		ret = gpio_pin_configure(gpio0_dev, buttons_pins[i], GPIO_INPUT | GPIO_PULL_UP);
		if (ret < 0) {
			printk("Error: gpio_pin_configure failed for button %d/pin %d, error:%d\n\r", i+1,buttons_pins[i], ret);
			return;
		} else {
			printk("Success: gpio_pin_configure for button %d/pin %d\n\r", i+1,buttons_pins[i]);
		}
	}

    /* Configure the interrupt on the button's pin */
	for(i=0; i<sizeof(buttons_pins); i++) {
		ret = gpio_pin_interrupt_configure(gpio0_dev, buttons_pins[i], GPIO_INT_EDGE_TO_ACTIVE );
		if (ret < 0) {
			printk("Error: gpio_pin_interrupt_configure failed for button %d / pin %d, error:%d", i+1, buttons_pins[i], ret);
			return;
		}
	}
    
    /* Initialize the static struct gpio_callback variable   */
	pinmask=0;
	for(i=0; i<sizeof(buttons_pins); i++) {
		pinmask |= BIT(buttons_pins[i]);
	}
    gpio_init_callback(&button_cb_data, button_pressed, pinmask); 	
	
	/* Add the callback function by calling gpio_add_callback()   */
	gpio_add_callback(gpio0_dev, &button_cb_data);

    while(1){

			//printk("Credito Atual: adawe\n\r");
        switch(estado){
            case MENU:
				//printk("Credito Atual: papap \n\r");
				if (eventos == ADD1 || eventos == ADD2 || eventos == ADD5 || eventos == ADD10){
					estado = UPDATE_CREDIT;
				}
				else if(eventos == RET){
					printk("%d EUR return\n",Credito);
					Credito = 0;
					eventos = NONE;
				}
				else if(eventos == UP || eventos == DOWN){
					estado = MOVIES;
				}
				else if(eventos == SEL){
					estado = PAY_CHECK;
				}
				break;

			case UPDATE_CREDIT:
				if (eventos == ADD1){
					Credito += 1;
					printk("Credito Atual: %d EUR\n\r",Credito);
				}
				if (eventos == ADD2){
					Credito += 2;
					printk("Credito Atual: %d EUR\n\r",Credito);
				}
				if (eventos == ADD5){
					Credito += 5;
					printk("Credito Atual: %d EUR\n\r",Credito);
				}
				if (eventos == ADD10){
					Credito += 10;
					printk("Credito Atual: %d EUR\n\r",Credito);
				}
				estado = MENU;
				eventos = NONE;
				break;

			case MOVIES:
				if(eventos == UP){
					movie_idx = (movie_idx+1)%(num_movie);
					printk("Movie %c, %dH00 session \n", Movie[movie_idx],Hora[movie_idx]);
					printk("Custo: %d EUR\n", Preco[movie_idx]);
					printk("Saldo: %d EUR\n",Credito);
				}
				else if(eventos == DOWN && movie_idx == -1){
					movie_idx = num_movie -1;
					printk("Movie %c, %dH00 session \n", Movie[movie_idx],Hora[movie_idx]);
					printk("Custo: %d EUR\n", Preco[movie_idx]);
					printk("Saldo: %d EUR\n",Credito);
				}
				else{
					movie_idx = (movie_idx-1)%(num_movie);
					printk("Movie %c, %dH00 session \n", Movie[movie_idx],Hora[movie_idx]);
					printk("Custo: %d EUR\n", Preco[movie_idx]);
					printk("Saldo: %d EUR\n",Credito);
				}
				estado = MENU;
				eventos = NONE;
				break;
			
			case PAY_CHECK:
				if(movie_idx < 0 || movie_idx >= num_movie){
					printk("Não tem nenhum filme selecionado! \n");
				}
				else if(Credito >= Preco[movie_idx]){
					printk("Ticket for Movie %c, session %dH00 issued\n",Movie[movie_idx],Hora[movie_idx]);
					/* Atualizar o credito*/
					Credito = Credito - Preco[movie_idx];
					printk("Remaining credit: %d EUR\n",Credito);
				}
				else{
					printk("Not enough credit. Ticket not issued! \n");
				}
				estado = MENU;
				eventos = NONE;
				break;

        }

    }



}