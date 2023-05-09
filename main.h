#ifndef MAIN_H
#define MAIN_H

#define F_CPU 8000000UL
#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)
#define CRYSTAL_USE 8 //MHz

#define SCOREBOARD 1
#define TIMER_DISPLAY 1

#define DISPLAY_BUFFER 11
#define UART_BAUD_RATE 9600
#define RED_LED _BV(PE2)
#define SAMPLES_MAX 60
#define MAX_DISPLAY_DIGITS 20
#define RF_REPEAT_TIME 4000
//#define MODBUS 0
#define KORN_PIN 3
#define KORN_PORT PORTB
#define KORN_OFF() 	  KORN_PORT &= ~(1<<KORN_PIN)
#define KORN_ON() 	  KORN_PORT |= (1<<KORN_PIN)

#define RELAY_PIN	5
#define RELAY_PORT PORTE
#define RELAY_OFF()  RELAY_PORT &= ~(1<<RELAY_PIN)
#define RELAY_ON()   RELAY_PORT |= (1<<RELAY_PIN)

//Flash fonts
// C$a -$0B o$0C spase $0D (P)$E (b)$0F . $10 (F)$11 (o),$12 (n),$13 (A)$14 (t)$15 (L)$16 (E)$17
#define CHR_C 0x0a
#define DASH 0x0b
#define DEGREE 0x0c
#define SPACE 0x0d
#define CHR_P 0x0e
#define CHR_b 0x0f
#define DOT 0x10
#define CHR_F 0x11
#define CHR_o 0x12
#define CHR_n 0x13
#define CHR_A 0x14
#define CHR_t 0x15
#define CHR_L 0x16
#define CHR_E 0x17

// Menu F labels - Users menu
#define Display_mode 0 //F1
#define Speed 1 //F2
#define Brightness 2 //b
#define light_swhitch 3 //L
#define Temp_decimal 4 //F5
#define Countdown_alarms 5 //F6
#define alarm_lenght 6 //F7
#define Clock_blink_enable 7 //F8
// E menu (fav) - Technicians menu
#define DISPLAY_DIGITS 0 //E1
#define invert_shift 1 //E2
#define TLC_drivers_Enable 2 //E3
//#define Separate_temp 3 //E4
#define Start_game_choice 3 //E4 Choice from RF remote start Game
#define RF_remote 4 //E5 choice of RF remote / Buttons Box
#define Monitor_Mode 5// this is for working with PC only 
#define Centimeter 6 //				E4
#endif



