/*
Name:		library for Cosmo GSM Shield
Version:	0.1
Created:	17.03.2015
Programmer:	Erezeev A.
Production:	JT5.RU
Source:		https://github.com/jt5/telit

This library is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "TelitGSM.h"
#include <string.h>

#define PD7	(7)
#define PD6	(6)
#define PD5	(5)
#define PB0	(0)
#define PB1	(1)

#ifdef  _AVR_IOM328P_H_ || _AVR_IOM328_H_
	#define RXCIEn	RXCIE0
	#define RXCn	RXC0
	#define RXENn	RXEN0
	#define TXENn	TXEN0
	#define UBRRnH	UBRR0H
	#define UBRRnL	UBRR0L
	#define UCSRnA	UCSR0A
	#define UCSRnB	UCSR0B
	#define UCSRnC	UCSR0C
	#define UDRn	UDR0
	#define USARTn_RX_vect USART_RX_vect
#elif  _AVR_IOM2560_H_
	#ifdef Telit_USART0
		#define USARTn_RX_vect USART0_RX_vect	
		#define RXCIEn	RXCIE0
		#define RXCn	RXC0
		#define RXENn	RXEN0
		#define TXENn	TXEN0
		#define UBRRnH	UBRR0H
		#define UBRRnL	UBRR0L
		#define UCSRnA	UCSR0A
		#define UCSRnB	UCSR0B
		#define UCSRnC	UCSR0C
		#define UDRn	UDR0
	#elif Telit_USART1
		#define USARTn_RX_vect USART1_RX_vect
		#define RXCIEn	RXCIE1
		#define RXCn	RXC1
		#define RXENn	RXEN1
		#define TXENn	TXEN1
		#define UBRRnH	UBRR1H
		#define UBRRnL	UBRR1L
		#define UCSRnA	UCSR1A
		#define UCSRnB	UCSR1B
		#define UCSRnC	UCSR1C
		#define UDRn	UDR1
	#elif Telit_USART2
		#define USARTn_RX_vect USART2_RX_vect
		#define RXCIEn	RXCIE2
		#define RXCn	RXC2
		#define RXENn	RXEN2
		#define TXENn	TXEN2
		#define UBRRnH	UBRR2H
		#define UBRRnL	UBRR2L
		#define UCSRnA	UCSR2A
		#define UCSRnB	UCSR2B
		#define UCSRnC	UCSR2C
		#define UDRn	UDR2		
	#elif Telit_USART3
		#define USARTn_RX_vect USART3_RX_vect
		#define RXCIEn	RXCIE3
		#define RXCn	RXC3
		#define RXENn	RXEN3
		#define TXENn	TXEN3
		#define UBRRnH	UBRR3H
		#define UBRRnL	UBRR3L
		#define UCSRnA	UCSR3A
		#define UCSRnB	UCSR3B
		#define UCSRnC	UCSR3C
		#define UDRn	UDR3		
	#else #error Invalid USART port number!
	#endif		
	//USART1_RX_vect
	//USART2_RX_vect
	//USART3_RX_vect
#else #error	Unsupported target MCU!!!
	
		
#endif

unsigned char rx_buffer[RX_BUFFER_SIZE];

//! Индекс поиска
static unsigned char rx_i;
//! Индекс записи в буфер приёма
static unsigned char rx_wr_i;
//! Флаги переполнения и подтверждения
volatile unsigned char rx_overflow, rx_ack;

volatile unsigned char CMTI_wait_state;


const char * searchFor; // Указатель на шаблон поиска

static unsigned char rx_i_CMTI;
volatile unsigned char NewSMS_index, NewSMS_index_;
static unsigned char searchStr;



const char * pCMTI;

char CallerID[16];

static unsigned char putchar(unsigned char send_char)
{
	while (!(UCSRnA & 0x20));
	UDRn = (unsigned char) send_char;
	_delay_us(500); //Пауза для нормальной работы модема
	return(send_char);
}

static unsigned char getchar( void )
{
	/* Wait for data to be received */
	while ( !(UCSRnA & (1<<RXCn)) )
	;
	/* Get and return received data from buffer */
	return UDRn;
}


unsigned char TelitClass::Write(unsigned char Byte)
{
	 return putchar(Byte);
}

void UART0_rx_reset( void )
{
	UCSRnB &= ~(1<<RXCIEn);
	rx_i = rx_wr_i = 0;
	rx_ack = 0;
	rx_buffer[ rx_wr_i ] = '\0';
}



void UART0_rx_on( void )
{
	UCSRnB |= ( 1 << RXCIEn );
}


void UART0_rx_off( void )
{

	UCSRnB &= ~( 1 << RXCIEn );
}
	
//Установка шаблона для поиска
//pSTR - адрес шаблона в ПЗУ
void UART0_setSearchString( const char * pPattern_P )
{
	UCSRnB &= ~( 1 << RXCIEn );
	searchFor = pPattern_P;	 //Что требуется найти в буфере ответа
	rx_i = 0;
}	



/************************************************************************/
/* Обработчик прерываний по приему нового символа по UART
/* Выполняет сравнение с шаблоном из FLASH "на лету"                                                                 */
/************************************************************************/
ISR(USARTn_RX_vect)
{
	char data;
	char* Src;
	
	unsigned char match_flag;
	static unsigned char CMTI_received; // Признак приёма индикации нового СМС +CMTI
	
	data = UDRn;
	if (!data) return;
	rx_buffer[ rx_wr_i++ ] = data; //Положили новый символ в буфер
	if( rx_wr_i > RX_BUFFER_MASK ) //Если буфер переполнен, продолжим сначала с перезатиранием
	{
		rx_wr_i = 0;
	}
		
	if( data == pgm_read_byte(&searchFor[rx_i]) )  //Если пришедший символ совпал с символом шаблона
	{
		rx_i++; //Следующий символ шаблона
		match_flag = 1;
		
	}
	else if (data == pgm_read_byte(&searchFor[0])) //Если пришедший символ совпал с первым символом шаблона
	{
		rx_i = 1; //Следующий символ шаблона
		match_flag = 1;
	}			
	/*
	else if ( pgm_read_byte(&CMTI[rx_i_CMTI]) == data )
	{
		match_flag = 0; rx_i_CMTI++; rx_i = 0;
		if( !pgm_read_byte(&CMTI[rx_i_CMTI]) )
		{
			//Приняли шаблон: "+CMTI: " целиком
			rx_i_CMTI = 0;
			// Устанавливаем шаблон для поиска CR_LF
			searchFor = CR_LF;			
			rx_wr_i = 0;
			CMTI_received = 1;
			// Ждём конца строки
		}
	}
	*/
	else
	{
		rx_i = 0; rx_i_CMTI = 0;
	}
	
	if (match_flag)
	{		
		if( !pgm_read_byte(&searchFor[rx_i]) ) // Если дошли до конца шаблона - выключим приём
		{
			rx_i = 0;
			rx_buffer[ rx_wr_i] = 0x00;
			if ((CMTI_wait_state) && (!CMTI_received))
			{
				// Если пришёл шаблон +CMTI: 
				CMTI_wait_state = 0; // +CMTI больше не ждём
				searchFor = CR_LF;	//  а ждём перевод строки
				rx_wr_i = 0;
				CMTI_received = 1;
				return; //Приём не заканчиваем, а просто выходим из обработчика
			}			
			else if ((CMTI_received)&&(!CMTI_wait_state))
			{
				CMTI_received = 0;
				CMTI_wait_state = 0;
				//Приняли оповещение:"+CMTI: <mem>,<index>" целиком
				Src = (char*)rx_buffer;
				while (*Src++ != ',');
				// Парсим индекс
				int n;
				n = 0;
				while (*Src)
				{
					if (( *Src >= '0' ) && ( *Src <= '9' ) )
					{
						n = 10*n + ( *Src - '0' );
					}
				Src++;
				}
			NewSMS_index = n;
			}			
			rx_ack = 1; // поднимем флаг - пришел ответ
			UCSRnB &= ~( 1 << RXCIEn ); // Выключаем приём
		}		
		
		
	}
	
}


void TelitClass::begin()
{
	sei();
}

void TelitClass::WriteStr(char* s)
{
	while (*s) putchar(*s++);
}

void TelitClass::WriteStr_P(const char* s)
{
	while (pgm_read_byte(s))
	putchar(pgm_read_byte(s++));
}

void UART0_init(void)
{
	UCSRnB = 0x00;
	UCSRnA = 0x00;
	UCSRnC = 0x06;
	UBRRnL = 25; //38400 low speed ( @ 16 MHz)
	UBRRnH = 0x00;
	UCSRnB = ( 1 << TXENn )|( 1 << RXENn );
	UART0_rx_reset();
}

// Проверка ответа от модема

signed char UART0_check_acknowledge( unsigned int pause )
{
	//! Local variables
	static unsigned int i, ii;

	for( i = 0; ( rx_ack == 0 ) && ( i < 65535 ); i++ ) //Цикл ожидания
	{
		for( ii = 0; ii < pause; ii++ )
		{
			asm("nop"); // Тратим энергию в пустую
		}
		asm("WDR");
	}

	if( rx_ack > 0 )
	{
		rx_ack = 0;
		return 1;
	}

	else
	{
		UART0_rx_off( );
		UART0_rx_reset( );
		return -1;
	}
}

void TelitClass::end()
{
}

signed char TelitClass::SendCmdWaitResp(const char* pCommand_P, const char* pSearchPattern_P, unsigned int pause)
{
 UART0_rx_reset( );
 UART0_setSearchString(pSearchPattern_P);
 //_delay_ms(500);
 Telit.WriteStr_P(pCommand_P);
 Telit.WriteStr_P(PSTR("\r\n"));
 CMTI_wait_state = 0;
 UART0_rx_on( );
 if( UART0_check_acknowledge( pause ) > 0 )
 {
	 return 1;
 }
 else return -1;
} 
 
 signed char TelitClass::WaitResp(const char* pSearchPattern_P, unsigned int pause)
 {
	 UART0_rx_reset( );
	 UART0_setSearchString(pSearchPattern_P);	 
	 CMTI_wait_state = 0;	 
	 UART0_rx_on( );
	 if( UART0_check_acknowledge( pause ) > 0 )
	 {
		 return 1;
	 }
	 else return -1;
 }
 

void TelitClass::WaitSMS(void)
{
	UART0_rx_reset( );
	CMTI_wait_state = 1;
	searchFor = CMTI;
//	searchCMTI = (const char* const)pgm_read_word(&searchStrings[CMTI_]);
//	UART0_setSearchString( RING_ );
	UART0_rx_on( );
}

 
 signed char TelitClass::Init(const char* PIN)
 {	 	 
	 
	 DDRD |= (1<<PD5);	 	 
	 PORTD |= (1<<PD5);
	 _delay_ms(300);	 
	 PORTD &= ~(1<<PD5);
	 _delay_ms(5000);
	 UART0_init();
	 Telit.WriteStr_P(PSTR("AT\r\n"));
	 _delay_ms(100);
	 Telit.WriteStr_P(PSTR("ATE0\r\n"));
	 _delay_ms(100);
	 Telit.WriteStr_P(PSTR("AT#SELINT=2\r\n"));
	 _delay_ms(100);	 
	 Telit.WriteStr_P(PSTR("AT+IPR=38400\r\n"));
	 _delay_ms(100);
	 Telit.WriteStr_P(PSTR("AT\\Q0\r\n"));
	 _delay_ms(100);
	Telit.WriteStr_P(PSTR("AT&K0\r\n"));
	_delay_ms(100);	
	 Telit.WriteStr_P(PSTR("AT+IFC= 0,0\r\n"));
	 _delay_ms(100);
	 Telit.WriteStr_P(PSTR("AT+ICF= 3\r\n"));
	 _delay_ms(100);
	 Telit.WriteStr_P(PSTR("AT+CMEE=2\r\n"));
	 _delay_ms(100);	 
	 //	searchFor = (const char*)pgm_read_word(&searchStrings[1]);
	 //	 Telit.WriteStr_P(searchFor);
	 sei();
	 	 	 
	 while (SendCmdWaitResp(PSTR("AT"),OK, pause64) < 0);
	 
	 if (SendCmdWaitResp(PSTR("AT+CPIN?"),PSTR("+CPIN: "), pause256)) 
	 {
		 //Пришел ответ "+CPIN:"
		 if (WaitResp(OK, pause64)) 
		 {
			 // Пришел OK
			 if (strstr_P((char*)rx_buffer, PSTR("SIM PIN")))
			 {
				 // Вводим PIN код
				 Telit.WriteStr_P(PSTR("AT+CPIN=\""));
				 Telit.WriteStr_P(PIN);				 
				 if ( SendCmdWaitResp(PSTR("\""),OK, pause256) < 0) return -3; // PIN код не верен			 
			 }
			else if (!strstr_P((char*)rx_buffer, PSTR("READY")))
			{
				return -3;		 				 								
			}
		 }
		 else return -3;		 		 
	 }else return -2;	 
	 //
	/*
		Ждем регистрации в сети
	*/												
	unsigned char Attempts=0;
	while (CheckStatus() < 0) 
	{
		if (Attempts++ > 16) 
		{
			return -1;
		}
	}
	
	/*
	Инициализируем текстовый режим СМС
	*/
	if (SendCmdWaitResp(PSTR("AT+CSCA?"), OK, pause256) < 0) return -2;
	if (SendCmdWaitResp(PSTR("AT+CMGF=1"), OK, pause64) < 0) return -2;
					
	 return 1;
 }

signed char TelitClass::CheckStatus(void)
{
	//	Telit.WriteStr_P(PSTR("AT\r\n"));
	signed char status = -1;
	_delay_ms(500);		
	for (unsigned char i =0; i<4; i++)
	{		
		if (SendCmdWaitResp(PSTR("AT+CGATT?"), PSTR("+CGATT: 1"), pause64) > 0 )
		{
			//Пришел ответ "+CGATT: 1", ждем теперь "OK"
			if (WaitResp(OK, pause64) > 0) {status = 1;break;};
		}
		_delay_ms(1000);		
	}			
	return status;		
}

signed char TelitClass::GetIMEI (char* IMEIbuf)
{	
	if (SendCmdWaitResp(PSTR("AT+CGSN"), OK, pause64) > 0 )
	{
		char* pSrc = (char*)rx_buffer;
		unsigned char n = 0; unsigned char k = 0;
		while ((n < 15) && (k < 64))
		{
			if ((*pSrc >= '0') && (*pSrc <= '9')) {*IMEIbuf++ = *pSrc; n++;}
			else if (*pSrc == 'O') break;
			*pSrc++; k++;
		}
		if (n == 15) {*pSrc =0x00; return 1;}
		else return -2;			
	} else return -1;	
}

signed char TelitClass::PowerOFF()
{	
	if (SendCmdWaitResp(PSTR("AT#SHDN"), OK, pause256) > 0 )
	{
		return 1;
	}
	else return -1;
		
}

/*

		
		*/

signed char TelitClass::getTime(int* hr,int* min,int* sec,int* day, int* month, int* yr)
{
	char* Src;
	int Temp;
	_delay_ms(500);	
	if (SendCmdWaitResp(PSTR("AT+CCLK?"), OK, pause64) > 0 )
	{
		Src = (char*)&rx_buffer[2];
		if (*Src != '+') return -1;
		
		// Взяли год
		Src = (char*)&rx_buffer[10];
		Temp = (*Src++ - '0') * 10;
		Temp += (*Src - '0');
		*yr = Temp;
		
		// Взяли месяц
		Src = (char*)&rx_buffer[13];
		Temp = (*Src++ - '0') * 10;
		Temp += (*Src - '0');
		*month = Temp;
		
		// Взяли день
		Src = (char*)&rx_buffer[16];
		Temp = (*Src++ - '0') * 10;
		Temp += (*Src - '0');
		*day = Temp;
		
		
		// Взяли часы
		Src = (char*)&rx_buffer[19];
		Temp = (*Src++ - '0') * 10;
		Temp += (*Src - '0');
		*hr = Temp;
		
		// Взяли минуты
		Src = (char*)&rx_buffer[22];
		Temp = (*Src++ - '0') * 10;
		Temp += (*Src - '0');
		*min = Temp;
		

		// Взяли секунды
		Src = (char*)&rx_buffer[25];
		Temp = (*Src++ - '0') * 10;
		Temp += (*Src - '0');
		*sec = Temp;				
		
		return 1;	
	}
	
	
}