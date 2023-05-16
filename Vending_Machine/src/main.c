/**
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** \file Trabalho3.ext
* \brief Implementaçao de Máquinas de estado em Sistemas Embutidos (usando Zephyr)
* 
*
* After one blank line follows the detailed description
* which can span through several lines
*
* \author Rodolfo Oliveira, NºMec: 88919
* \date 16 de Maio de 2023
* \bug Nenhum bug conhecido
*/

#include <zephyr.h>
#include <zephyr/device.h> /* device_is_ready and device struct */
#include <zephyr/devicetree.h> /* DT_NODELABEL() */
#include <zephyr/sys/printk.h> /* printk */
#include <zephyr/drivers/gpio.h> /* GPIO api */

/* Use a "big" sleep time to reduce CPU load (button detection int activated, not polled) */
#define SLEEP_TIME_MS   60*1000 

/** @brief Definiçao de array dos pinos usados
	* set de pins used 
	* buttons 1-4 on board (11,12,24,25)
	* buttons 5-8 connected labeled A0...A3 (gpio pin 3,4,28,29) 
	* array usado para identificaçao de qual butao foi clicado para identificar um evento */
const uint8_t buttons_pins[] = { 11,12,24,25,3,4,28,29};

//----------------------------------------------------------------
/** @brief Definicao de eventos
 	*  Enumeraçao de possiveis eventos criados pelos sistema */
typedef enum {
    NONE, ADD1, ADD2, ADD5, ADD10, UP, DOWN, SEL, RET
} Event;

/** @brief Definicao de Estados
 	*  Enumeraçao de estados do sistema */
typedef enum{
    MENU, MOVIES, UPDATE_CREDIT
} States;

/** @brief Variavel para identificar qual filme apresentar no estado Movie
 *  flag para manter o movie_idx caso seja 1 mantem movie idx, caso 0 pode alterar o movie idx */
static volatile int same_movie = 1;

/** @brief Variavel eventos para tomar o valor do evento ocorrido
 * definir variavel evento que tera um valor consoante o evento ocorrido*/
static volatile Event eventos = NONE;
/** @brief Variavel estado para tomar o valor do estado seguinte 
 * definir variavel estado que irá tomar um valor consoante o evento ocorrido e estado atual */
static volatile States estado = MENU;
/** @brief Variavel Credito  
 * definir variavel credito que terá a quantia de credito introduzida pelo utilizador */
static volatile int Credito = 0;

/* Listas de filmes, horas e preço */
/** @brief Quantidade de filmes */
static volatile int num_movie = 5;

/** @brief Lista de nomes dos Filmes 
 * array com os nomes dos filmes que podem ser comprados
*/
static volatile char Movie[]= {'A', 'A', 'A', 'B', 'B'};
/** @brief Lista das Horas 
 * array com as horas das sessoes dos filmes 
*/
static volatile char Hora[] = {19, 21, 23, 19, 21}; 
/** @brief Lista de Preços
 * array com os preços relacionado a cada filme e sessao
*/
static volatile int Preco[]= {9, 11, 9, 10, 12};

/** @brief variavel de identificaçao da posiçao da lista de filmes
 * Aponta para o filme selecionado e a ser apresentado no Estado movie 
*/
static volatile int movie_idx = 0;



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

/** @brief Funçao de callback com funcionamento identico a uma interrupçao
 * 
 * Funçao que é chamada quando um botao é primido e irá identificar qual botao foi primido
 * 
 * 
*/
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	int i=0;
	/** Identify the button(s) that was(ere) hit*/
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

	/*Configure the GPIO pins - buttons 1-4 + IOPINS 2,4,28 and 29 for input*/
	/** @brief Configuraçao dos pinos de entrada
	 * 
	 * Os pinos usados para utilizar os butoes têm de ser configurados como entradas
	*/
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
	/** @brief ativaçao de interrupçoes no pinos usados pelos butoes
	 * 
	 * Ativar o modo de interrupçao para os pinos relacionados com os butoes
	*/
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

        switch(estado){
            case MENU:
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
					same_movie = 1;
				}
				// else if(eventos == SEL){}
				break;

			case UPDATE_CREDIT:
				
				// Eventos que fazem a mudança de estado
				// retornar o credito
				if(eventos == RET){
					printk("%d EUR return\n",Credito);
					Credito = 0;
					eventos = NONE;
					estado = MENU;
					break;
				}
				// Fazer pesquisa na lista de filmes
				if(eventos == UP || eventos == DOWN){
					same_movie = 1; // apresenta o filme selecionado ou o primeiro da lista
					estado = MOVIES;
					eventos = NONE;
					break;
				}
				/*verificar se temos algum filme selecionado */
				if(eventos == SEL && same_movie == 1){
					printk("Ainda não selecionou filme\n");
					eventos = NONE;
				}
				/* se tivermos um filme selecionado e o credito menor que o preço do mesmo ficamos neste estado e imprimimos uma mensagem */
				/* para saber que o um filme essta selecionado verificamos a variavel same_movie se esta tiver o valor 0 quer dizer que já 
				passamos pelo menos uma vez pelo estado MOVIE */
				else if(eventos == SEL && same_movie == 0 && Credito < Preco[movie_idx]){
					printk("Not enough Credit. Ticket not issued!\n");
					eventos = NONE;
				}
				/* se tiver filme selecionado e credito suficiente coltamos ao estado menu e necessitamos de escolher novamente um filme*/
				else if(eventos == SEL && same_movie == 0 && Credito >= Preco[movie_idx]){
					printk("Ticket for movie %c, session %c issued!\n",Movie[movie_idx],Hora[movie_idx]);
					Credito = Credito - Preco[movie_idx];
					printk("Remaining credit %d \n",Credito);
					same_movie = 1;
					estado = MENU;
					eventos = NONE;
					break;
				}

				// açoes dentro deste estado 
				// atualizaçao do credito consoante a quantia inserida
				
				if (eventos == ADD1){
					Credito += 1;
					printk("Credito Atual: %d EUR\n\r",Credito);
					eventos = NONE;
				}
				else if (eventos == ADD2){
					Credito += 2;
					printk("Credito Atual: %d EUR\n\r",Credito);
					eventos = NONE;
				}
				else if (eventos == ADD5){
					Credito += 5;
					printk("Credito Atual: %d EUR\n\r",Credito);
					eventos = NONE;
				}
				else if (eventos == ADD10){
					Credito += 10;
					printk("Credito Atual: %d EUR\n\r",Credito);
					eventos = NONE;
				}
				break;

			case MOVIES:
				/* Eventos que fazem mudar deee estado */
				/* inserir coins passa ao estado UPDATE_CREDIT */
				if (eventos == ADD1 || eventos == ADD2 || eventos == ADD5 || eventos == ADD10){
					estado = UPDATE_CREDIT;
					break;
				}

				/* Returno do credito passa ao estado MENU */
				if (eventos == RET){
					printk("%d EUR return\n",Credito);
					Credito = 0;
					eventos = NONE;
					estado = MENU;
					break;
				}

				/* Selecionar a compra de um filme */
				if(eventos == SEL  && Credito < Preco[movie_idx]){
					printk("Not enough Credit. Ticket not issued!\n");
					eventos = NONE;
				}
				/* se tiver filme selecionado e credito suficiente coltamos ao estado menu e necessitamos de escolher novamente um filme*/
				else if(eventos == SEL  && Credito >= Preco[movie_idx]){
					printk("Ticket for movie %c, session %c issued!\n",Movie[movie_idx],Hora[movie_idx]);
					Credito = Credito - Preco[movie_idx];
					printk("Remaining credit %d \n",Credito);
					
					estado = MENU;
					eventos = NONE;
					same_movie = 1;
					break;
				}

				/* Açoes neste estado*/
				/* Manter o movie_idx */
				if(same_movie == 1){
					printk("Movie %c, %dH00 session \n", Movie[movie_idx],Hora[movie_idx]);
					printk("Custo: %d EUR\n", Preco[movie_idx]);
					printk("Saldo: %d EUR\n",Credito);
					eventos = NONE;
					same_movie = 0;
				}
				/* alterar movie idx*/
				else{
					if(eventos == UP){
						movie_idx = (movie_idx+1)%(num_movie);
						printk("Movie %c, %dH00 session \n", Movie[movie_idx],Hora[movie_idx]);
						printk("Custo: %d EUR\n", Preco[movie_idx]);
						printk("Saldo: %d EUR\n",Credito);
						eventos = NONE;
					}
					else if(eventos == DOWN){
						movie_idx = (movie_idx-1);
						if(movie_idx < 0){
							movie_idx = num_movie-1;
						}
						printk("Movie %c, %dH00 session \n", Movie[movie_idx],Hora[movie_idx]);
						printk("Custo: %d EUR\n", Preco[movie_idx]);
						printk("Saldo: %d EUR\n",Credito);	
						eventos = NONE;
					}
				}
				break;
        }
    }
}