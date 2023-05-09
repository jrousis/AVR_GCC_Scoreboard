/*
 * Scoreboard.c
 *
 * Created: 10/5/2021 9:57:43 μμ
 * Author : user
 */ 
#include "main.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <avr/wdt.h> 
#include <avr/eeprom.h>

volatile int8_t Digits_disp;
#define RS485_PIN 4
#define RS4851_PIN 5
#define RS485_TX_OFF() 	  PORTA &= ~(1<<RS485_PIN)
#define RS485_TX_ON() 	  PORTA |= (1<<RS485_PIN)
#define RS485_1_TX_OFF() 	  PORTA &= ~(1<<RS4851_PIN)
#define RS485_1_TX_ON() 	  PORTA |= (1<<RS4851_PIN)

//Eeprom declarations
uint8_t  EEMEM Slave_address[1]={0x01};
uint8_t  EEMEM Countdown_alarm2[3]={0x00,0x02,0x00};
uint8_t  EEMEM Countdown_alarm1[3]={0x00,0x01,0x00};
uint8_t  EEMEM Countdown2_eep[3]={0x00,0x10,0x00};
uint8_t  EEMEM Countdown1_eep[3]={0x00,0x20,0x00};
uint8_t  EEMEM FAV_eep[8]={4,0,0,0,0,0,0,0};
uint8_t  EEMEM F_eep[8]={5,3,16,8,0,0,0,0};
									//	     0          1          2          3          4          5         6           7          8         9
static const __flash char flash_fonts[] = { 0b11101110,0b00000110,0b01111100,0b00111110,0b10010110,0b10111010,0b11111010,0b00100110,0b11111110,0b10111110,
			0b11101000,0b00010000,0b10110100,0b00000000,0b11110100,0b11011010,0b00000001,0b11110000,0b01011010,0b01010010,0b11110110,0b11011000,0b11001000,0b11111000 };
//	     C$a      -$0B        o$0C   spase $0D     (P)$E     (b)$0F     . $10       (F)$11    (o),$12    (n),$13	 (A)$14	   (t)$15	  (L)$16	  (E)$17
//static const __flash char flash_Device_ID[] = "CLOCK_TIMER_V.1.2";
static const __flash char flash_Device_ID[] = "SCOREBOARD_V.1.2";
	//const __flash char Title[] = "Force Operations" ;
static uint8_t display_out_buf[DISPLAY_BUFFER]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static uint8_t set_clock_buf[6];
static uint8_t timer[3]={0x00,0x00,0x00};
static uint8_t alarm[2]={0x00,0x00};
static uint8_t last_temperature[4];
static uint8_t Score_home=0;
static uint8_t Score_guest=0;
static uint8_t photo_samples[60];
extern uint8_t sram_brigt=16;
int8_t samples_metter=-1;
volatile int8_t shift=0;
volatile int8_t cursor;
volatile int8_t previews_togle=0xff;
volatile int8_t user_instruction=0;
volatile int8_t Button_Key = 0;
volatile int key=-1;
uint16_t Timer_Butt_Minus=0;
uint16_t Timer_Butt_Start=0;
uint16_t Timer_bright=0;
uint16_t Count_bright=0;
uint16_t Timer_devide=0;
uint16_t Timer_devide_photo=0;
uint8_t Program_display=1;
uint8_t Program_display_time=20;
uint8_t Clock_blink=0;
uint8_t Clock_blink_on=0;
uint8_t Timer_blink_on=0;
uint8_t Menu_blink_on=0;
uint8_t Set_countdown_bank=0;
uint8_t Game_on=0;
uint8_t Change_timer_on=0;
uint8_t Speaker_delay_open=0;
//volatile uint8_t Modbus_incoming_buf[32];

#include "../libs/MBI_out.h"
#include "../libs/MBI_out.c"
#include "../libs/rc5.h"
#include "../libs/rc5.c"
#include "../libs/Modbus.h"
#include "../libs/Modbus.c"
#include "../libs/ds18S20.h"
#include "../libs/ds18S20.c"
#include "../libs/rtc.c"
#include "../libs/rtc.h"
#include "../libs/uart.h"
#include "../libs/uart.c"
#include "../libs/rf-buttons.h"
#include "../libs/rf-buttons.c"

void display_init(int8_t);
void display_Clock(void);
void display_Date(void);
void display_temperature(void);
void Up_counter(void);
void timer_display(void);
void F_menu(void);
void F_menu_check(int8_t,char);
void Fav_menu_check(int8_t,char);
void Buzzer(int8_t,int16_t);
void Set_clock(void);
void Change_Timer(void);
void Set_countdown(int8_t);
void Set_date(void);
void Display_set_clock(void);
void Read_temperature(void);
void photo_sample(void);
void show_brightness(void);
void chek_timer_alarms(void);
void update_score_display(void);
void remote_instruction(void);
void rf_instruction(uint8_t);
void Buttons_Score();


void Display_Out(){
	int8_t Len = 0;
	int8_t character;
	int8_t invert= eeprom_read_byte((uint8_t*)FAV_eep + invert_shift);
	if (invert)
	{
		while(Len<Digits_disp){
			character =(flash_fonts[(display_out_buf[Len]&0b01111111)]);
			if((display_out_buf[Len]&0x80)==0x80){character=character|0b00000001;}		
			byte_out(character);
			Len++;
		}
	}
	else
	{
		Len = Digits_disp;
		while(Len>0){
			Len--;
			character =(flash_fonts[(display_out_buf[Len]&0b01111111)]);
			if((display_out_buf[Len]&0x80)==0x80){character=character|0b00000001;}		
			byte_out(character);
		}
	}
	do_rclk();
}

ISR(TIMER0_OVF_vect) {
	wdt_reset();
	Timer_bright++;
	if (Timer_Butt_Minus>0){Timer_Butt_Minus--;}
	if (Timer_Butt_Start>0){Timer_Butt_Start--;}	
	//if (Timer_Butt_Minus==1){KORN_ON();	_delay_ms(400); KORN_OFF();}
	uint8_t TLC_divers = eeprom_read_byte((uint8_t*)FAV_eep + TLC_drivers_Enable);
	uint8_t bright = eeprom_read_byte((uint8_t*)F_eep + Brightness);
	if (bright==0){bright = sram_brigt;}
	if (bright<0)
	bright=16;
	
	if (Timer_bright>=3 && !TLC_divers) {
		Timer_bright=0;
		Count_bright++;		
		if (bright>=Count_bright)
		DISPLAY_ON();
		else
		DISPLAY_OFF();		
		if (Count_bright==16) {Count_bright=0;}		
	}
	else if (Timer_bright>=0xfff0 && TLC_divers){
		TLC_config_byte(bright,eeprom_read_byte((uint8_t*)FAV_eep + DISPLAY_DIGITS));
		Timer_bright=0;
	}
	
	
    Timer_devide++;
    // timer for 500 ms Crystal * 250
    if (Timer_devide >= CRYSTAL_USE * 250) {
        PORTE ^= RED_LED;
        Timer_devide = 0;
		if (Speaker_delay_open)
		{
			Speaker_delay_open--;
			if (!Speaker_delay_open)
			Buzzer(0,0);
		}
		//check limit for relay with hysterisys 
		if ((bright+1) < eeprom_read_byte((uint8_t*)F_eep+light_swhitch)) {
			RELAY_ON();
		}else if ((bright-1) > eeprom_read_byte((uint8_t*)F_eep+light_swhitch)) {
			RELAY_OFF();
		}
		

        if (user_instruction == 0) {
            Program_display_time++;
            int8_t display_eep = eeprom_read_byte((uint8_t * ) F_eep + Display_mode) & 0x0f;
            int8_t Speed_eep = eeprom_read_byte((uint8_t * ) F_eep + Speed) & 0x0f;
            if (Speed_eep > 9) {
                Speed_eep = 9;
            }
            if (Program_display_time > Speed_eep * 2) {
                int8_t display_eep = eeprom_read_byte((uint8_t * ) F_eep + Display_mode) & 0x0f;

                if (Program_display == 1 && display_eep & 1) {
                    //Clock_blink = 0x80;
                    //display_Clock();
					Clock_blink_on = 1;
                    Program_display = 2;
                    Program_display_time = 0;
                } else if (Program_display == 2 && display_eep & 2) {
                    Clock_blink_on = 0;
                    display_Date();
                    Program_display = 3;
                    Program_display_time = 0;
                } else if (Program_display == 3 && display_eep & 4 ) {
                    Clock_blink_on = 0;
					if (bright<14 && !TLC_divers){DISPLAY_OFF();}else{DISPLAY_ON();}
                    display_temperature();
					DISPLAY_ON();
                    Program_display = 1;
                    Program_display_time = 0;
					Clock_blink = 0x00;
                } else {
                    Program_display++;
					//if (eeprom_read_byte((uint8_t*)FAV_eep + Separate_temp)==4)
					//Read_temperature();
                    display_init(0);
                }
            }
            if (Clock_blink_on) {
				int8_t a = eeprom_read_byte((uint8_t * ) F_eep + Clock_blink_enable);
                if (a) {Clock_blink ^= _BV(7);}else{Clock_blink =0x80;}
                display_Clock();
            }
        } else if (Menu_blink_on) {
            //blink for set clock
            Clock_blink ^= _BV(7);
            if (Clock_blink) {
                display_out_buf[cursor - shift] &= 0b10000000;
                display_out_buf[cursor - shift] |= 0x0D;
            } else {
                display_out_buf[cursor - shift] = set_clock_buf[cursor];
            }
            Display_Out();
        } else if (Timer_blink_on) {
			Clock_blink ^= _BV(7);
			timer_display();
		}
    }
	if (!eeprom_read_byte((uint8_t*)F_eep + Brightness))
	{
		Timer_devide_photo++;
		if (Timer_devide_photo >= CRYSTAL_USE * 1500){
			Timer_devide_photo = 0;
			photo_sample();
		}
	}
}

ISR(TIMER2_OVF_vect)
{
	uint8_t a = timer[0], b = timer[1], c = timer[2];
	
	if (user_instruction=='A' | user_instruction=='B')
	{
		c--;	//Counter Down process
		if (c<0) {
			c=0x59;
			b--;
			if (b<0) {
				b=0x59;
				a--;
			}else if ((b&0x0f)>9) {
				b-=6;
			}
		}else if ((c&0x0f)>9) {
			c-=6;
		}
	}
	else
	{
		c++;	//Up counter process 
		if ((c&0x0f)>9)	{
			if (c>=0x5a) {
				b++;
				if ((b&0x0f)>9)	{
					if (b>=0x9a) { // διόρθωση για γήπεδα ποσοσφαίρου 0-99 λεπτά
						//a++;
						//if ((a&0x0f)>9)	{a+=6;}
						b=0;
					}else{b+=6;}				
				}
				c=0;
			}else{c+=6;}			
		}
	}
	
	timer[0]=a; timer[1]=b; timer[2]=c;
/*	if (timer[1]>=0x9A)
	{
		timer[1] = 0;
	}*/
	timer_display();
	if (user_instruction=='A' || user_instruction=='B')
	{
		if (a==0 && b==0 && c==0)
		{
			a=3;
			TCCR2B =0;
			while (a>0)
			{
				Buzzer(1,4000);
				wdt_reset();
				_delay_ms(1000);
				Buzzer(0,4000);
				b=0;
				while (b<4){display_out_buf[b]= 0x0d;b++;}
				Display_Out();
				wdt_reset();
				_delay_ms(1000);
				timer_display();
				a--;
			}
			//End of counter down
			wdt_reset();
			_delay_ms(3000);
			user_instruction = 0;
			display_init(1);
		}
	}
	chek_timer_alarms();
}

int main() {
	wdt_enable(WDTO_8S);
	DDRE = 0b01000011;
	PORTE = 0b00000010;
	PORTB = 0B10010001;
	DDRB = 0b01101001;
	PORTD = 0B11110000;
	DDRD = 0b00000000;
	ADMUX = 0b01100110;
	ADCSRA = 0b11100000;
	ADCSRB = 0b00000100;
	DIDR0 = 0b10111111;
	Digits_disp= eeprom_read_byte((uint8_t*)FAV_eep + DISPLAY_DIGITS);
	if (Digits_disp<2 || Digits_disp>MAX_DISPLAY_DIGITS)
	Digits_disp=4;
	Init_MBI();
	Init_RF();
	uart0_init(BAUD_PRESCALE);
	cli();
	photo_sample();
	DISPLAY_ON();
	DoLEDtest();
	RC5_Init();
	ds1302_init();
	DDRE |= RED_LED;
	TCCR0B = 0b00000010;
	TIMSK0 = 1;
	TIMSK2 = 1;
	ASSR |= 1<<AS2;
	TCCR3A = 0b01000000;
	Buzzer(1,16000);
	_delay_ms(50);
	Buzzer(0,0);
	user_instruction = 0;
	display_init(1);
	//if (eeprom_read_byte((uint8_t*)FAV_eep + Separate_temp)==4)
	//Read_temperature();
	
	//flash_fonts
	/*
	uart0_putc('T');
	uart0_putc('E');
	uart0_putc('S');
	uart0_putc('T');
	*/
	update_score_display();
	sei();
	Display_Out();	
	
	//---------------------------------------------------------------------------
	for (;;) {
		Game_on=0;
		while (user_instruction != 0)
		switch (user_instruction) {
			case ('A'):
				Game_on=1;//Score_home=0;Score_guest=0;
				//display_out_buf[8]=0;display_out_buf[9]=0;display_out_buf[10]=0;
				update_score_display();
				Counter_down(1);
				break;
			case ('B'):
				Game_on=1;//Score_home=0;Score_guest=0;
				//display_out_buf[8]=0;display_out_buf[9]=0;display_out_buf[10]=0;
				update_score_display();
				Counter_down(2);
				break;
			case ('U'):
				Game_on=1;//Score_home=0;Score_guest=0;
				//display_out_buf[8]=0;display_out_buf[9]=0;display_out_buf[10]=0;
				update_score_display();
				Up_counter();
				break;
			case ('S'):
				Set_clock();
				break;
			case ('D'):
				Set_date();
				break;
			case ('O'):
				show_brightness();
				break;
			case ('F'):
				F_menu();
				break;
			case ('E'):
				FAV_menu();
				break;
			case ('C'):
				Set_countdown(Set_countdown_bank);
				if (Set_countdown_bank==2) {
					Set_countdown(Set_countdown_bank);
					Set_countdown_bank=0;
				}else if (Set_countdown_bank==4)
				{
					Set_countdown(Set_countdown_bank);
					Set_countdown_bank=0;
				}
				break;					
		}
	}
	return 0;
}

void display_init(int8_t prog){
	Menu_blink_on=0;
	Change_timer_on=0;
	if (prog!=0){Program_display=prog;}
	Clock_blink_on=0;
	//Clock_blink = 0x00;
	Timer_blink_on=0;
	Program_display_time=20;					
	if (Program_display>3){Program_display=1;}
	Timer_devide=0x1fff;
	update_score_display();
}

void display_Clock(void){
	struct rtc_time ds1302;
	struct rtc_time *rtc;
	rtc = &ds1302;
	rtc->hour_format = H24;
	ds1302_update(rtc);   // update all fields in the struct
	display_out_buf[0]= (rtc->hour&0xf0)>>4;
	display_out_buf[1]= (rtc->hour&0x0f)|Clock_blink;  //adding dot
	display_out_buf[2]= ((rtc->minute&0xf0)>>4)|Clock_blink;  //adding dot;
	display_out_buf[3]= (rtc->minute&0x0f);
	if (Digits_disp==6){
		display_out_buf[2] &=0b01111111;
		display_out_buf[3] |=Clock_blink;
		display_out_buf[4]= (rtc->second&0xf0)>>4;
		display_out_buf[5]= (rtc->second&0x0f);
	}
	Display_Out();
}

void display_Date(void){
	struct rtc_time ds1302;
	struct rtc_time *rtc;
	rtc = &ds1302;
	rtc->hour_format = H24;
	ds1302_update(rtc);   // update all fields in the struct
	if (Digits_disp==6)
	{
		display_out_buf[0]= SPACE;
		display_out_buf[1]= (rtc->date&0xf0)>>4;
		display_out_buf[2]= (rtc->date&0x0f);  //adding dot
		display_out_buf[3]= DASH;
		display_out_buf[4]= ((rtc->month&0xf0)>>4);  //adding dot;
		display_out_buf[5]= (rtc->month&0x0f);
	}else{
		display_out_buf[0]= (rtc->date&0xf0)>>4;
		display_out_buf[1]= (rtc->date&0x0f)|0x80;  //adding dot
		display_out_buf[2]= ((rtc->month&0xf0)>>4);  //adding dot;
		display_out_buf[3]= (rtc->month&0x0f);
		if (eeprom_read_byte((uint8_t*)F_eep + Temp_decimal)==4)
		{
			display_out_buf[4]=last_temperature[0];
			display_out_buf[5]=last_temperature[1];
			display_out_buf[6]=last_temperature[2];
			display_out_buf[7]=last_temperature[3];
		}
	}
	
	Display_Out();
}

void display_temperature(void){
	int8_t temp_res = eeprom_read_byte((uint8_t*)F_eep + Temp_decimal);
	TSDS18x20 DS18x20;
	TSDS18x20 *pDS18x20 = &DS18x20;	
	char buffer[10];	
	// Init DS18B20 sensor
	if (DS18x20_Init(pDS18x20,&PORTB,PB1))
	{		
		display_out_buf[0]=DASH;
		display_out_buf[1]=DASH;
		display_out_buf[2]=DEGREE;
		display_out_buf[3]=CHR_C;
		Display_Out();
		return -1;
	}
	else
	// Set DS18B20 resolution to 9 bits.	
	if (temp_res)
	DS18x20_SetResolution(pDS18x20,CONF_RES_11b);
	else
	DS18x20_SetResolution(pDS18x20,CONF_RES_9b);
	
	DS18x20_WriteScratchpad(pDS18x20);
	// Initiate a temperature conversion and get the temperature reading
	if (DS18x20_MeasureTemperature(pDS18x20))
	{
		dtostrf(DS18x20_TemperatureValue(pDS18x20),9,4,buffer);
		//Here we make display temperature for 6 digits
		if (buffer[1]=='-'){
			display_out_buf[0]=0x0b;
			if (temp_res) {
				display_out_buf[1]=(buffer[2]&0x0f);
				display_out_buf[2]=(buffer[3]&0x0f);
				display_out_buf[3]=0xc;
				//splay_out_buf[3]= 0xc ;
				}else{
				display_out_buf[1]= (buffer[2]&0x0f);
				display_out_buf[2]= (buffer[3]&0x0f);
				display_out_buf[3]= 0xc ;
			}
		}else{
			if (buffer[2]=='-')
			display_out_buf[0]=0x0b;
			else
			display_out_buf[0]=(buffer[2]&0x0f);
			if (temp_res) {
				display_out_buf[1]=(buffer[3]&0x0f)|0x80;
				display_out_buf[2]=(buffer[5]&0x0f);
				display_out_buf[3]= 0xc ;
			}else{
				display_out_buf[1]=(buffer[3]&0x0f);
				display_out_buf[2]= 0xc;
				display_out_buf[3]= 0xa ;
			}
		}		
	Display_Out();
	}
	/*
	display_out_buf[0]=last_temperature[0];
	display_out_buf[1]=last_temperature[1];
	display_out_buf[2]=last_temperature[2];
	display_out_buf[3]=last_temperature[3] ;
	Display_Out(); */
}

void Counter_down(int8_t bank)
{
	key=-1;
	TCNT2 = 0;
	Timer_blink_on=0x80;
	
	if (bank==1)
	{
		timer[0]= eeprom_read_byte((uint8_t*)Countdown1_eep);
		timer[1]= eeprom_read_byte((uint8_t*)Countdown1_eep+1);
		timer[2]= eeprom_read_byte((uint8_t*)Countdown1_eep+2);
	}else if (bank==2)
	{
		timer[0]= eeprom_read_byte((uint8_t*)Countdown2_eep);
		timer[1]= eeprom_read_byte((uint8_t*)Countdown2_eep+1);
		timer[2]= eeprom_read_byte((uint8_t*)Countdown2_eep+2);
	}	
	timer_display();
	while (user_instruction!=0)
	{
		if (key==ok)
		{
			if (TCCR2B==0){
				TCCR2B = 0b00000101; //start timer for seconds
				}else{
				TCCR2B = 0;
			}
			key=-1;
			Buzzer(1,8600);
			_delay_ms(300);
			Buzzer(0,0);
		}
		if (Button_Key)
		Buttons_Score();
		if (key==play)
		Change_Timer();
	}
	TCCR2B = 0;
	Buzzer(1,18600);
	_delay_ms(300);
	Buzzer(0,0);
}

void Up_counter(void)
{
	key=-1;
	TCNT2 = 0;
	timer[0]=0;timer[1]=0;timer[2]=0;
	Timer_blink_on=0x80;
	timer_display();
	while (user_instruction!=0)
	{
		if (key==ok)
		{
			if (TCCR2B==0){
				TCCR2B = 0b00000101; //start timer for seconds
				//key=-1;		
			}else{
				TCCR2B = 0;
			}	
			key=-1;
			Buzzer(1,8600);
			_delay_ms(300);
			Buzzer(0,0);
		}
		if (Button_Key)
		Buttons_Score();
		if (key==play)
		Change_Timer();
	}
	TCCR2B = 0;
	Buzzer(1,18600);
	_delay_ms(300);
	Buzzer(0,0);
}

void timer_display(void)
{
	if (timer[0]==0) //Check if hours in timer
	{
		display_out_buf[0]= (timer[1]&0xf0)>>4;
		display_out_buf[1]= (timer[1]&0x0f)|0x80;  //adding dot
		display_out_buf[2]= (timer[2]&0xf0)>>4;  //adding dot;
		display_out_buf[3]= timer[2]&0x0f;
		Timer_blink_on=0;
	}else{
		display_out_buf[0]= (timer[0]&0xf0)>>4;
		display_out_buf[1]= (timer[0]&0x0f)|Clock_blink;  //adding dot
		display_out_buf[2]= (timer[1]&0xf0)>>4;  //adding dot;
		display_out_buf[3]= timer[1]&0x0f;
		Timer_blink_on=1;
	}
	
	Display_Out();
}

void F_menu(void){
	char F1_array[sizeof(F_eep)];
	int8_t i;
	for (i=0;i<sizeof(F_eep);i++)
	{
		F1_array[i]=eeprom_read_byte((uint8_t*)F_eep+i);
		if (i == Brightness || i == light_swhitch) {
			if (F1_array[i]>16){F1_array[i]=16;};
		}else if (F1_array[i]>9) {
			F1_array[i]=9;
		}		
	}
	i=0;
	set_clock_buf[0]= 0x11; //F character
	set_clock_buf[1]= i+1;
	set_clock_buf[2]= 0x0d; //space char
	set_clock_buf[3]= F1_array[i];//eeprom_read_byte((uint8_t*)&F_eep)&0x0f;
	if (set_clock_buf[3]>9)	{set_clock_buf[3]=7;}
	cursor = 3;
	shift = 0;
	Menu_blink_on=1;
	Display_set_menu();
	user_instruction='f';
	while(user_instruction!=0){
		if(key>=0 && key<10)
		{
			set_clock_buf[cursor]= key;
			if(i==Brightness || i==light_swhitch){
				F1_array[i]=(set_clock_buf[2]*10)+set_clock_buf[3];
				if(cursor<3){cursor=3;}
			}else{
				F1_array[i]=key;
			}
			
			key=-1;
			if(cursor<3){cursor++;}
			Display_set_menu();
		}
		else if (key==right && i<(sizeof(F_eep)-1))
		{
			i++;
			key=-1;
			F_menu_check(i,F1_array[i]);			
			Display_set_menu();
		}
		else if (key==left && i>0)
		{
			i--;
			key=-1;
			F_menu_check(i,F1_array[i]);
			Display_set_menu();
		}
		else if (key==up)
		{
			key=-1;				
			if (i==Brightness && F1_array[i]<16){
				F1_array[i]+=1;		
				set_clock_buf[2]= (F1_array[i]/10)&0x0f;
				set_clock_buf[3]= (F1_array[i]%10)&0x0f;		
			}else if (i==light_swhitch && F1_array[i]<16){
				F1_array[i]+=1;
				set_clock_buf[2]= (F1_array[i]/10)&0x0f;
				set_clock_buf[3]= (F1_array[i]%10)&0x0f;
			}else if (F1_array[i]<9){
				F1_array[i]+=1;
				set_clock_buf[cursor]=F1_array[i];
			}
			
			Display_set_menu();
		}
		else if (key==down)
		{
			key=-1;
			if (i==Brightness && F1_array[i]>0){
				F1_array[i]-=1;
				set_clock_buf[2]= (F1_array[i]/10)&0x0f;
				set_clock_buf[3]= (F1_array[i]%10)&0x0f;
				}else if (i==light_swhitch && F1_array[i]>0){
				F1_array[i]-=1;
				set_clock_buf[2]= (F1_array[i]/10)&0x0f;
				set_clock_buf[3]= (F1_array[i]%10)&0x0f;
				}else if (F1_array[i]>1){
				F1_array[i]-=1;
				set_clock_buf[cursor]=F1_array[i];
			}
			
			Display_set_menu();
		}
		else if (key==ok)
		{
			for (i=0;i<sizeof(F_eep);i++)
			{
				eeprom_write_byte ((uint8_t*) &F_eep[i], F1_array[i]);
			}
			user_instruction=0;
			Timer_bright = 0xfff0;
			display_init(1);
		}
	}
	cursor=0;
	shift=0;
	Clock_blink=0;
}

void FAV_menu(void)
{
	char F1_array[sizeof(FAV_eep)];
	int8_t i;
	for (i=0;i<sizeof(FAV_eep);i++)
	{
		F1_array[i]=eeprom_read_byte((uint8_t*)FAV_eep+i);	
		if (i == 0) {
			if (F1_array[i]>11){F1_array[i]=11;};
		}else if (F1_array[i]>9){
			F1_array[i]=0;
		}		
		
	}
	i=0;
	set_clock_buf[0]= 0x17; //F character
	set_clock_buf[1]= i+1;
	set_clock_buf[2]= 0x0d; //space char
	//set_clock_buf[3]= F1_array[i];//eeprom_read_byte((uint8_t*)&F_eep)&0x0f;
	set_clock_buf[2]= (F1_array[i]/10)&0x0f;
	set_clock_buf[3]= (F1_array[i]%10)&0x0f;
	cursor = 2;
	shift = 0;
	Menu_blink_on=1;
	Display_set_menu();
	user_instruction='e';
	while(user_instruction!=0){
		if(key>=0 && key<10)
		{
			set_clock_buf[cursor]= key;
			F1_array[i]=key;
			
			key=-1;
			//if(cursor<Digits_disp-1){cursor++;}
			Display_set_menu();
		}
		else if (key==right && i<(sizeof(F_eep)-1))
		{
			i++;
			key=-1;
			Fav_menu_check(i,F1_array[i]);
			//set_clock_buf[1]= i+1;
			//set_clock_buf[3]= F1_array[i];		
			Display_set_menu();
		}
		else if (key==left && i>0)
		{
			i--;
			key=-1;
			Fav_menu_check(i,F1_array[i]);
			//set_clock_buf[1]= i+1;
			//set_clock_buf[3]= F1_array[i];
			Display_set_menu();
		}
		else if (key==up)
		{
			key=-1;			
			if (i==0 && F1_array[i]<16){
				F1_array[i]+=1;
				set_clock_buf[2]= (F1_array[i]/10)&0x0f;
				set_clock_buf[3]= (F1_array[i]%10)&0x0f;
				}else if (F1_array[i]<9){
				F1_array[i]+=1;
				set_clock_buf[cursor]=F1_array[i];
			}	
			//F1_array[i]+=1;
			//set_clock_buf[cursor]=F1_array[i];			
			Display_set_menu();
		}
		else if (key==down)
		{
			key=-1;
			if (i==0 && F1_array[i]>0){
				F1_array[i]-=1;
				set_clock_buf[2]= (F1_array[i]/10)&0x0f;
				set_clock_buf[3]= (F1_array[i]%10)&0x0f;
				}else if (F1_array[i]>1){
				F1_array[i]-=1;
				set_clock_buf[cursor]=F1_array[i];
			}
			
			Display_set_menu();
		}
		else if (key==ok)
		{
			for (i=0;i<sizeof(FAV_eep);i++)
			{
				eeprom_write_byte ((uint8_t*) &FAV_eep[i], F1_array[i]);
			}
			Digits_disp= eeprom_read_byte((uint8_t*)FAV_eep + DISPLAY_DIGITS);
			if (Digits_disp<2 || Digits_disp>MAX_DISPLAY_DIGITS)
			Digits_disp=4;
			user_instruction=0;
			display_init(1);
		}
	}
	cursor=0;
	shift=0;
	Clock_blink=0;
}

void F_menu_check(int8_t intex, char val){
	
	if (intex==Brightness){			
		set_clock_buf[0]= 0x0F;
		set_clock_buf[1]= 0x0d;
		set_clock_buf[2]= (val/10)&0x0f;
		set_clock_buf[3]= (val%10)&0x0f;
		cursor = 2;
	}else if (intex==light_swhitch){
		set_clock_buf[0]= 0x16;
		set_clock_buf[1]= 0x0d;
		set_clock_buf[2]= (val/10)&0x0f;
		set_clock_buf[3]= (val%10)&0x0f;
		cursor = 2;
	}else{
		set_clock_buf[0]= 0x11;
		set_clock_buf[1]= intex+1;
		set_clock_buf[2]= 0x0d;
		set_clock_buf[3]= val;
		cursor = 3;
	}
}

void Fav_menu_check(int8_t intex, char val){
	set_clock_buf[0]= 0x17;
	set_clock_buf[1]= intex+1;
	if (intex==0){			
		set_clock_buf[2]= (val/10)&0x0f;
		set_clock_buf[3]= (val%10)&0x0f;
		cursor = 2;
	}else{		
		set_clock_buf[2]= 0x0d;
		set_clock_buf[3]= val;
		cursor = 3;
	}
}

Buzzer(int8_t on,int16_t tone)
{
	if (on)
	{	
		KORN_ON();
		OCR3A = tone; //buzer sound tone
		TCCR3B= (1<<WGM12)+(1<<CS10);
		//OCR3AH= 0x15;
	}else{
		TCCR3B = 0;
		KORN_OFF();
		//TCCR3A = 0;
	}
}

void Display_set_menu(void){
	int i;
	
	for (i=0;i<Digits_disp;i++)
	{
		if (i<4)
		{
			display_out_buf[i]=set_clock_buf[i];
		}else{
			display_out_buf[i]=0xd;
		}
	}
	Display_Out();
}

void Set_clock(void){
	struct rtc_time ds1302;
	struct rtc_time *rtc;
	rtc = &ds1302;
	//int8_t clk_byte;
	ds1302_update(rtc);   // update all fields in the struct
	set_clock_buf[0]= (rtc->hour&0xf0)>>4;
	set_clock_buf[1]= (rtc->hour&0x0f);  //adding dot
	set_clock_buf[2]= ((rtc->minute&0xf0)>>4);  //adding dot;
	set_clock_buf[3]= (rtc->minute&0x0f);
	set_clock_buf[4]= ((rtc->second&0xf0)>>4);
	set_clock_buf[5]= (rtc->second&0x0f);
	Display_set_clock();
	user_instruction='s';
	cursor=0;Menu_blink_on=1;
	while(user_instruction!=0)
	{
		if(key>=0 && key<10)
		{
			set_clock_buf[cursor]= key;
			if (cursor==1 || cursor==2 || cursor==3 )
			{
				set_clock_buf[cursor]|=0x80;
			}
			key=-1;
			if(cursor<sizeof(set_clock_buf)-1){cursor++;}
			Display_set_clock();
		}
		else if (key==left)		//check for left <-
		{
			if(cursor!=0)
			{
				cursor--;
				Display_set_clock();
				key=-1;
			}			
		}
		else if (key==right)		//check for right ->
		{
			if(cursor<sizeof(set_clock_buf)-1)
			{
				cursor++;
				Display_set_clock();
				key=-1;
			}
			
		}
		else if (key==ok)
		{
			struct rtc_time ds1302;
			struct rtc_time *rtc;
			rtc = &ds1302;
			int8_t byte_get;
			rtc->hour_format = H24;
			byte_get=(set_clock_buf[0] & 0x0f)<<4;
			byte_get=byte_get | (set_clock_buf[1] & 0x0f);
			rtc->hour = byte_get;
			byte_get=(set_clock_buf[2] & 0x0f)<<4;
			byte_get=byte_get | (set_clock_buf[3] & 0x0f);
			rtc->minute = byte_get;
			byte_get=(set_clock_buf[4] & 0x0f)<<4;
			byte_get=byte_get | (set_clock_buf[5] & 0x0f);
			rtc->second = byte_get;			
			ds1302_write_time(rtc);
			user_instruction=0;
			display_init(1);
		}
	}
	cursor=0;
	shift=0;
	Clock_blink=0;
	Menu_blink_on=0;
}

void Change_Timer(){
		uint8_t Timer_val=TCCR2B;
		TCCR2B =0;
		Change_timer_on=1;
		set_clock_buf[0]= ( timer[0]&0xf0)>>4;
		set_clock_buf[1]= ( timer[0]&0x0f);  //adding dot
		set_clock_buf[2]= (( timer[1]&0xf0)>>4);  //adding dot;
		set_clock_buf[3]= ( timer[1]&0x0f);
		set_clock_buf[4]= (( timer[2]&0xf0)>>4);
		set_clock_buf[5]= ( timer[2]&0x0f);
		Display_set_clock();
		cursor=0;Menu_blink_on=1;
		while(key!=ok && key!=exit_button)
		{
			if(key>=0 && key<10)
			{
				set_clock_buf[cursor]= key;
				if (cursor==1 || cursor==2 || cursor==3 )
				{
					set_clock_buf[cursor]|=0x80;
				}
				key=-1;
				if(cursor<sizeof(set_clock_buf)-1){cursor++;}
				Display_set_clock();
			}
			else if (key==left)		//check for left <-
			{
				if(cursor!=0)
				{
					cursor--;
					Display_set_clock();
					key=-1;
				}
			}
			else if (key==right)		//check for right ->
			{
				if(cursor<sizeof(set_clock_buf)-1)
				{
					cursor++;
					Display_set_clock();
					key=-1;
				}
				
			}
			else if (key==ok)
			{
				int8_t byte_get;
				byte_get=(set_clock_buf[0] & 0x0f)<<4;
				byte_get=byte_get | (set_clock_buf[1] & 0x0f);
				timer[0] = byte_get;
				byte_get=(set_clock_buf[2] & 0x0f)<<4;
				byte_get=byte_get | (set_clock_buf[3] & 0x0f);
				timer[1] = byte_get;
				byte_get=(set_clock_buf[4] & 0x0f)<<4;
				byte_get=byte_get | (set_clock_buf[5] & 0x0f);
				timer[2] = byte_get;
			}
		}
		cursor=0;
		shift=0;
		Clock_blink=0;
		Menu_blink_on=0;
		TCCR2B=Timer_val;
		update_score_display();
		timer_display();		
		if (key==ok)	{
			Buzzer(1,8600);
			_delay_ms(300);
			Buzzer(0,0);
		}		
		key=-1;
		Change_timer_on=0;
}


void Set_countdown(volatile	int8_t bank)
{
	volatile int8_t eep_add_bank;
	
	switch (bank)
	{
	case (1):
		eep_add_bank = Countdown1_eep;
		break;
	case (2):
		eep_add_bank = Countdown2_eep;
		break;
	case (3):
		eep_add_bank = Countdown_alarm1;
		break;
	case (4):
		eep_add_bank = Countdown_alarm2;
		break;
	}
	
	set_clock_buf[0]= (eeprom_read_byte((uint8_t*)eep_add_bank))>>4;
	set_clock_buf[1]= (eeprom_read_byte((uint8_t*)eep_add_bank)&0x0f); 
	set_clock_buf[2]= (eeprom_read_byte((uint8_t*)eep_add_bank+1)&0xf0)>>4;
	set_clock_buf[3]= (eeprom_read_byte((uint8_t*)eep_add_bank+1)&0x0f);
	set_clock_buf[4]= (eeprom_read_byte((uint8_t*)eep_add_bank+2)&0xf0)>>4;
	set_clock_buf[5]= (eeprom_read_byte((uint8_t*)eep_add_bank+2)&0x0f);
	/*
	if (bank==1)
	{
		set_clock_buf[0]= (eeprom_read_byte((uint8_t*)Countdown1_eep))>>4;
		set_clock_buf[1]= (eeprom_read_byte((uint8_t*)Countdown1_eep)&0x0f); 
		set_clock_buf[2]= (eeprom_read_byte((uint8_t*)Countdown1_eep+1)&0xf0)>>4;
		set_clock_buf[3]= (eeprom_read_byte((uint8_t*)Countdown1_eep+1)&0x0f);
		set_clock_buf[4]= (eeprom_read_byte((uint8_t*)Countdown1_eep+2)&0xf0)>>4;
		set_clock_buf[5]= (eeprom_read_byte((uint8_t*)Countdown1_eep+2)&0x0f);
	}else if (bank==2)
	{
		set_clock_buf[0]= (eeprom_read_byte((uint8_t*)Countdown2_eep))>>4;
		set_clock_buf[1]= (eeprom_read_byte((uint8_t*)Countdown2_eep)&0x0f); 
		set_clock_buf[2]= (eeprom_read_byte((uint8_t*)Countdown2_eep+1)&0xf0)>>4;
		set_clock_buf[3]= (eeprom_read_byte((uint8_t*)Countdown2_eep+1)&0x0f);
		set_clock_buf[4]= (eeprom_read_byte((uint8_t*)Countdown2_eep+2)&0xf0)>>4;
		set_clock_buf[5]= (eeprom_read_byte((uint8_t*)Countdown2_eep+2)&0x0f);
	}*/
	
	user_instruction='c'; //intex for countdown
	cursor=0;Menu_blink_on=1;
	shift=0;
	Display_set_clock();
	while(user_instruction!=0)
	{
		if(key>=0 && key<10)
		{
			set_clock_buf[cursor]= key;
			if (cursor==1 || cursor==2 || cursor==3 )
			{
				set_clock_buf[cursor]|=0x80;
			}
			key=-1;
			if(cursor<sizeof(set_clock_buf)-1){cursor++;}
			Display_set_clock();
		}
		else if (key==left)		//check for left <-
		{
			if(cursor!=0)
			{
				cursor--;
				Display_set_clock();
				key=-1;
			}			
		}
		else if (key==right)		//check for right ->
		{
			if(cursor<sizeof(set_clock_buf)-1)
			{
				cursor++;
				Display_set_clock();
				key=-1;
			}			
		}
		else if (key==ok)
		{
			eeprom_write_byte ((uint8_t*) eep_add_bank, ((set_clock_buf[0] & 0x0f)<<4)|(set_clock_buf[1] & 0x0f));
			eeprom_write_byte ((uint8_t*) eep_add_bank+1, ((set_clock_buf[2] & 0x0f)<<4)|(set_clock_buf[3] & 0x0f));
			eeprom_write_byte ((uint8_t*) eep_add_bank+2, ((set_clock_buf[4] & 0x0f)<<4)|(set_clock_buf[5] & 0x0f));
			/*
			if (bank==1)
			{
				eeprom_write_byte ((uint8_t*) &Countdown1_eep, ((set_clock_buf[0] & 0x0f)<<4)|(set_clock_buf[1] & 0x0f));
				eeprom_write_byte ((uint8_t*) &Countdown1_eep+1, ((set_clock_buf[2] & 0x0f)<<4)|(set_clock_buf[3] & 0x0f));
				eeprom_write_byte ((uint8_t*) &Countdown1_eep+2, ((set_clock_buf[4] & 0x0f)<<4)|(set_clock_buf[5] & 0x0f));
			}else if (bank==2)
			{
				eeprom_write_byte ((uint8_t*) &Countdown2_eep, ((set_clock_buf[0] & 0x0f)<<4)|(set_clock_buf[1] & 0x0f));
				eeprom_write_byte ((uint8_t*) &Countdown2_eep+1, ((set_clock_buf[2] & 0x0f)<<4)|(set_clock_buf[3] & 0x0f));
				eeprom_write_byte ((uint8_t*) &Countdown2_eep+2, ((set_clock_buf[4] & 0x0f)<<4)|(set_clock_buf[5] & 0x0f));
			}*/
			
			user_instruction=0;
			display_init(1);
		}
	}
	display_init(1);
}

void Set_date(void){
	struct rtc_time ds1302;
	struct rtc_time *rtc;
	rtc = &ds1302;
	//int8_t clk_byte;
	ds1302_update(rtc);   // update all fields in the struct
	set_clock_buf[0]= (rtc->date&0xf0)>>4;
	set_clock_buf[1]= (rtc->date&0x0f); 
	set_clock_buf[2]= ((rtc->month&0xf0)>>4);
	set_clock_buf[3]= (rtc->month&0x0f);
	set_clock_buf[4]= ((rtc->year&0xf0)>>4);
	set_clock_buf[5]= (rtc->year&0x0f);
	Display_set_clock();
	user_instruction='d';
	cursor=0;Menu_blink_on=1;
	while(user_instruction!=0)
	{
		if(key>=0 && key<10)
		{
			set_clock_buf[cursor]= key;
			if (cursor==1 || cursor==2 || cursor==3 )
			{
				set_clock_buf[cursor]|=0x80;
			}
			key=-1;
			if(cursor<sizeof(set_clock_buf)-1){cursor++;}
			Display_set_clock();
		}
		else if (key==left)		//check for left <-
		{
			if(cursor!=0)
			{
				cursor--;
				Display_set_clock();
				key=-1;
			}			
		}
		else if (key==right)		//check for right ->
		{
			if(cursor<sizeof(set_clock_buf)-1)
			{
				cursor++;
				Display_set_clock();
				key=-1;
			}
			
		}
		else if (key==ok)
		{
			struct rtc_time ds1302;
			struct rtc_time *date;
			date = &ds1302;
			int8_t byte_get;
			date->hour_format = H24;
			byte_get=(set_clock_buf[0] & 0x0f)<<4;
			byte_get=byte_get | (set_clock_buf[1] & 0x0f);
			date->date = byte_get;
			byte_get=(set_clock_buf[2] & 0x0f)<<4;
			byte_get=byte_get | (set_clock_buf[3] & 0x0f);
			date->month = byte_get;
			byte_get=(set_clock_buf[4] & 0x0f)<<4;
			byte_get=byte_get | (set_clock_buf[5] & 0x0f);
			date->year = byte_get;			
			ds1302_write_date(date);
			user_instruction=0;
			display_init(1);
		}
	}
	display_init(1);
	//cursor=0;
	//shift=0;
	//Clock_blink=0;Menu_blink_on=0;
}

void Display_set_clock(void){
	int i;
	if (Digits_disp==4 && cursor>3 && user_instruction!='f')	{
		shift=2;
		set_clock_buf[1]&= 0b01111111;
		set_clock_buf[2]&= 0b01111111;
	}else if (Digits_disp==4 && cursor>3 && user_instruction!='e') {
		shift=2;
		set_clock_buf[1]&= 0b01111111;
		set_clock_buf[2]&= 0b01111111;
	}else if (user_instruction=='D' || user_instruction=='d') {
		shift=0;
		set_clock_buf[1]|= 0x80;
		set_clock_buf[3]|= 0x80;
	}else if (user_instruction=='c')	{	
		if (cursor<2){shift=0;}			
		set_clock_buf[1]|= 0x80;
		set_clock_buf[3]|= 0x80;
	}else{
		shift=0;
		set_clock_buf[1]|= 0x80;
		set_clock_buf[2]|= 0x80;
		set_clock_buf[3]|= 0x80;
	}
	
	for (i=0;i<6;i++)
	{
		display_out_buf[i]=set_clock_buf[i+shift];
	}
	//if (Digits_disp == 8){
	display_out_buf[6]= SPACE;
	display_out_buf[7]= SPACE;
	//}
	Clock_blink = 0x00;
	Display_Out();
}

void Read_temperature(void){
	TSDS18x20 DS18x20;
	TSDS18x20 *pDS18x20 = &DS18x20;
	char buffer[10];
	//cli();
	if (DS18x20_Init(pDS18x20,&PORTB,PB1))
	{
		return -1;
	}
	else
	// Set DS18B20 resolution to 9 bits.
	if (eeprom_read_byte((uint8_t*)F_eep + Temp_decimal)) //Check the temp resolution
	DS18x20_SetResolution(pDS18x20,CONF_RES_11b);
	else
	DS18x20_SetResolution(pDS18x20,CONF_RES_9b);
	DS18x20_WriteScratchpad(pDS18x20);
	// Initiate a temperature conversion and get the temperature reading
	if (DS18x20_MeasureTemperature(pDS18x20))
	{
		dtostrf(DS18x20_TemperatureValue(pDS18x20),9,4,buffer);
		if (buffer[1]=='-')
		{
			last_temperature[0]=DASH;
			last_temperature[1]=(buffer[2]&0x0f);
			last_temperature[2]=(buffer[3]&0x0f);
			last_temperature[3]=DEGREE;
		}else{
			if (buffer[2]=='-')
			last_temperature[0]=DASH;
			else
			last_temperature[0]=(buffer[2]&0x0f);
			last_temperature[1]=(buffer[3]&0x0f);
			last_temperature[2]=DEGREE;
			last_temperature[3]=CHR_C;
		}
		
	}
	//sei();
}

void photo_sample(void)
{
	uint8_t read_sample = ADCH;
	if (read_sample<13){
		read_sample = 16;
	}else{
		read_sample = 17-(read_sample / 16);
	}
	if (samples_metter==-1){
		samples_metter=0;
		uint8_t eepbright = eeprom_read_byte((uint8_t*)F_eep + Brightness);
		if (eepbright>0)
		{
			sram_brigt = eepbright;
		}else{
			sram_brigt = read_sample;
		}
		
		if (eeprom_read_byte((uint8_t*)FAV_eep + TLC_drivers_Enable))	
		TLC_config_byte(sram_brigt,eeprom_read_byte((uint8_t*)FAV_eep + DISPLAY_DIGITS));
		
	}
	photo_samples[samples_metter] = read_sample; 
	samples_metter++;
	if (samples_metter>=SAMPLES_MAX)
	{
		uint16_t A=0;
		uint8_t i;
		for (i=0;i<SAMPLES_MAX;i++){A += photo_samples[i];}
		samples_metter=0;
		sram_brigt = A/SAMPLES_MAX;
		//uint8_t digits = eeprom_read_byte((uint8_t*)FAV_eep + DISPLAY_DIGITS);
		//TLC_config_byte(sram_brigt,digits);
	} 
}

void show_brightness(void){
	char dec_bright[2]; 
	char dec_address[4];
	uint8_t i;
	itoa(sram_brigt,dec_bright,10);	
	display_out_buf[0]= CHR_o;
	display_out_buf[1]= SPACE;
	if (dec_bright[1]!=0)
	{
		display_out_buf[2]=dec_bright[0]&0x0f;
		display_out_buf[3]=dec_bright[1]&0x0f;
	}else{
		display_out_buf[2]= SPACE;
		display_out_buf[3]=dec_bright[0]&0x0f;
	}
	
	if (Digits_disp==6)	{
		display_out_buf[4]= SPACE;
		display_out_buf[5]= SPACE;
	}else if (Digits_disp>=8){
		itoa(eeprom_read_byte((uint8_t*)Slave_address),dec_address,10);	
		display_out_buf[4]= CHR_A;
		display_out_buf[5]= SPACE;
		display_out_buf[6]= dec_address[0]&0xf;
		if (dec_address[1]){
			display_out_buf[7]= dec_address[1]&0xf;
		}else{
			display_out_buf[7]= SPACE; 
		}
		
	}
	
	
	Display_Out();
	_delay_ms(3000);
	user_instruction=0;
	display_init(1);
}

void chek_timer_alarms(void)
{
	if (alarm[0])
	{
		alarm[1]--;
		if (alarm[1]==0)
		{
			alarm[0]=0;
			Buzzer(0,1000);
		}
	}
	int8_t a = (eeprom_read_byte((uint8_t*)Countdown_alarm1));
	int8_t b = (eeprom_read_byte((uint8_t*)Countdown_alarm1+1));
	int8_t c = (eeprom_read_byte((uint8_t*)Countdown_alarm1+2));
	if (timer[0]== a && timer[1]== b && timer[2]== c)
	{
		alarm[0]=1;
		alarm[1]= (eeprom_read_byte((uint8_t*)F_eep+Countdown_alarms));
		Buzzer(1,16600);
	}else{
		a = (eeprom_read_byte((uint8_t*)Countdown_alarm2));
		b = (eeprom_read_byte((uint8_t*)Countdown_alarm2+1));
		c = (eeprom_read_byte((uint8_t*)Countdown_alarm2+2));
		if (timer[0]== a && timer[1]== b && timer[2]== c)
		{
			alarm[0]=1;
			alarm[1]= (eeprom_read_byte((uint8_t*)F_eep+Countdown_alarms));
			Buzzer(1,8800);
		}
	}
}

void update_score_display(void){
	char str[4];
	itoa(Score_home,str,10);
	if (!str[1]){
		display_out_buf[4]=SPACE;
		display_out_buf[5]=str[0]&0xf;
	}else{
		display_out_buf[4]=str[0]&0xf;
		display_out_buf[5]=str[1]&0xf;
	}
	itoa(Score_guest,str,10);
	display_out_buf[6]=str[0]&0xf;
	if (str[1]){display_out_buf[7]=str[1]&0xf;}else{display_out_buf[7]=SPACE;}
	if (display_out_buf[8]==SPACE){
		display_out_buf[8]=0;display_out_buf[9]=0;display_out_buf[10]=0;
	}
	
}

void remote_instruction(void) {
    uint16_t command;
    /* Poll for new RC5 command */
    if (RC5_NewCommandReceived( & command)) {
        /* Reset RC5 lib so the next command
         * can be decoded. This is a must! */
        RC5_Reset();
        /* Toggle the LED on PB5 */
        PORTE ^= RED_LED;
        /* Do something with the command 
        Perhaps validate the start bits and output
        it via UART... */
        if (RC5_GetStartBits(command) != 3) {
            /* ERROR */
        }
        uint8_t cmdaddress = RC5_GetAddressBits(command);
        uint8_t cmdnum = RC5_GetCommandBits(command);
        uint8_t toggle = RC5_GetToggleBit(command);
		//display_out_buf[5]= cmdaddress;
        if (previews_togle != toggle && cmdaddress == 0 || previews_togle != toggle && cmdaddress == 3) {
            previews_togle = toggle;
            key = cmdnum;
            switch (cmdnum) {
				case  (forward):
					Score_home=0;Score_guest=0;
					display_out_buf[8]=0;display_out_buf[9]=0;display_out_buf[10]=0;
					update_score_display();
					key = -1;
					break;
				case  (countdown_timer1):
				if (user_instruction != 'A' && user_instruction != 'a') {
					user_instruction = 'A'; // This means Counter down 1
					key = -1;
				}
				break;
				case  (countdown_timer2):
				if (user_instruction != 'B' && user_instruction != 'b') {
					user_instruction = 'B'; // This means Counter down 1
					key = -1;
				}
				break;
				case  (up_timer):
					if (user_instruction != 'U' && user_instruction != 'u') {
					    user_instruction = 'U'; // This means Up counter
					    key = -1;
					}
					break;
				case (info): //button [INFO]
					if (user_instruction != 'S' && user_instruction != 's') {
					    user_instruction = 'S'; // This means Set cklock
					    key = -1;
					}
					break;
	            case (nettv):
	                if (user_instruction != 'D' && user_instruction != 'd') {
	                    user_instruction = 'D'; // This means set date
	                    key = -1;
	                }
	                break;
	            case (source):
	                if (user_instruction != 'F' && user_instruction != 'f') {
	                    user_instruction = 'F'; // This means set F menu 
	                    key = -1;
	                }
	                break;
				case (fav):
	                if (user_instruction != 'E' && user_instruction != 'e') {
	                    user_instruction = 'E'; // This means set F menu 
	                    key = -1;
	                }
	                break;
				case (countdown_set):
	                if (user_instruction != 'C' && user_instruction != 'c') {
	                    user_instruction = 'C'; // This means Counter down menu 1
						Set_countdown_bank = 1;
	                    key = -1;
	                }else if (user_instruction == 'C' || user_instruction == 'c')
	                {
						user_instruction = 0;	// This means Counter down menu 2
						Set_countdown_bank = 2; //Set this =2 to recall the function with bank2
	                    key = -1;
	                }
	                break;
				case (countdown_alarm_set):
	                if (user_instruction != 'C' && user_instruction != 'c' && Set_countdown_bank != 3) {
	                    user_instruction = 'C'; // This means Counter down menu 1
						Set_countdown_bank = 3; // Means Set countown alarm 1
	                    key = -1;
	                }else if (user_instruction == 'c' && Set_countdown_bank == 3)
	                {
						user_instruction = 0;	// This means Counter down menu 2
						Set_countdown_bank = 4; //Set this =2 to recall the function with bank2
	                    key = -1;
	                }
	                break;
	            case (exit_button): //check 'EXIT' button	                
	                if (!Change_timer_on){	  
						user_instruction = 0;
						Set_countdown_bank = 0;             
						display_init(1);
					}
	                break;
				case (Shift_source): //unstruction to show brightness
					if (user_instruction != 'O' && user_instruction != 'O') {
						user_instruction = 'O'; // This means show brightness
						key = -1;
					}
					break;
			}
		}else if (Game_on){
        switch (cmdnum) {
			case (V_plus):
				Score_home++;
				if (Score_home>99){Score_home=0;}
				update_score_display();
				timer_display();
				break;
			case (V_minus):
				Score_home--;
				if (Score_home>99){Score_home=99;}
				update_score_display();
				timer_display();
				break;
			case (P_plus):
				Score_guest++;
				if (Score_guest>99){Score_guest=0;}
				update_score_display();
				timer_display();
				break;
			case (P_minus):
				Score_guest--;
				if (Score_guest>99){Score_guest=99;}
				update_score_display();
				timer_display();
				break;
			case (7): //change TF1
				display_out_buf[8]++;
				if (display_out_buf[8]>9){display_out_buf[8]=0;}
				Display_Out();				
				break;
			case (8): //change TF2
				display_out_buf[9]++;
				if (display_out_buf[9]>9){display_out_buf[9]=0;}
				Display_Out();
				break;
			case (9): //change period
				display_out_buf[10]++;
				if (display_out_buf[10]>9){display_out_buf[10]=0;}
				Display_Out();
				break;
			}
		}
		
        if (cmdnum==Speaker){
			Buzzer(1,8600);
			Speaker_delay_open=2;
	        //_delay_ms(300);
	        //Buzzer(0,0);
			}

    }
}

void rf_instruction(uint8_t button){
	if (eeprom_read_byte((uint8_t*)FAV_eep + RF_remote))
	{
	switch(button){
		
		case (RF_BUTTON_1):
		if (Game_on && !Timer_Butt_Minus){
			key=ok;			
		}else if (Timer_Butt_Start){
			uint8_t Start_setting = eeprom_read_byte((uint8_t*)FAV_eep + Start_game_choice);
			switch (Start_setting){
				case (0):
					user_instruction = 'U'; // This means Up counter
					key = -1;
					break;
				case (1):
					user_instruction = 'A'; // This means Counter Down 1
					key = -1;
					break;
				case (2):
					user_instruction = 'B'; // This means Counter Down 2
					key = -1;
				break;
				default:
					user_instruction = 'U'; // This means Up counter
					key = -1;
					break;				
			}
		}else if (Timer_Butt_Minus){	//this is stop game
			user_instruction = 0;
			Set_countdown_bank = 0;
			display_init(1);
			break;
		}else{
			Timer_Butt_Start = RF_REPEAT_TIME;
		}		
		break;	
//--------------------------------------------------------------
		case (RF_BUTTON_3):
			if (!Game_on)
				break;
			if (Timer_Butt_Minus){
				Timer_Butt_Minus = RF_REPEAT_TIME;
				Score_home--;
				if (Score_home>99){Score_home=99;}
				update_score_display();
			}else{
				Score_home++;
				if (Score_home>99){Score_home=0;}
			}
			update_score_display();
			timer_display();
			break;
//--------------------------------------------------------------			
		case (RF_BUTTON_4):
			if (!Game_on)
				break;
			if (Timer_Butt_Minus){
				Timer_Butt_Minus = RF_REPEAT_TIME;
				Score_guest--;
				if (Score_guest>99){Score_guest=99;}
				update_score_display();
				}else{
				Score_guest++;
				if (Score_guest>99){Score_guest=0;}
			}
			update_score_display();
			timer_display();
			break;
//--------------------------------------------------------------
		case (RF_BUTTON_2):
			Timer_Butt_Minus = RF_REPEAT_TIME;
			break;		
		}
	}else if (Game_on){
		switch(button){
			//Here starts is the Box Button check 
			case (BOX_BUTTON_1):
				Button_Key=1;
				break;				
			case (BOX_BUTTON_2):
				Button_Key=2;
				break;
			case (BOX_BUTTON_3):
				Button_Key=3;
				break;				
			case (BOX_BUTTON_4):
				Button_Key=4;
				break;
		}
	}
}

void Buttons_Score(){
	switch(Button_Key){
		case (1):
		Score_home++;
		if (Score_home>99){Score_home=0;}
		break;		
		case (2):
		Score_home--;
		if (Score_home>99){Score_home=99;}
		break;
		case (3):
		Score_guest++;
		if (Score_guest>99){Score_guest=0;}
		break;		
		case (4):
		Score_guest--;
		if (Score_guest>99){Score_guest=99;}
		break;
	}
	update_score_display();
	timer_display();
	uint8_t i;
	uint8_t Input_PINs;
	for (i=0;i<3;i++){
		_delay_ms(40);
		Input_PINs=0xff;
		while(Input_PINs){
			wdt_reset();
			Input_PINs = ~RF_Pins;
			Input_PINs = Input_PINs>>4;
		}
	}
	Button_Key=0;
}
