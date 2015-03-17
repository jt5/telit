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

#include <avr/pgmspace.h>
#include <util/delay.h>
#include "TelitGSM.h"
#include "stuff.h"

const char CTRLZ[2] PROGMEM =  {26, 00};
extern volatile unsigned char NewSMS_index, NewSMS_index_;
extern volatile unsigned char CMTI_wait_state;

signed char TelitClass::CheckSMS(void)
{
	if (NewSMS_index_ != NewSMS_index)
	{
		NewSMS_index_ = NewSMS_index;
		return 1;
	}
	else return 0;
}

/****************************************************
Отправка СМС из FLASH
*****************************************************/
signed char TelitClass::SendSMS_P (const char* Num, const char *Txt)
{	
	_delay_ms(100);
	Telit.WriteStr_P(PSTR("AT+CMGS=\""));
	Telit.WriteStr_P(Num);
	Telit.WriteStr_P(PSTR("\"\r\n"));
	
	if (Telit.WaitResp(MORE, pause256)>0)
	{
		_delay_ms(1000);
		Telit.WriteStr_P(Txt);
		Telit.WriteStr_P(CTRLZ);
		Telit.WriteStr_P(PSTR("\"\r\n"));
		if (Telit.WaitResp(OK, pause256 * 2)>0)
		{
			return 1;
		}
		else return -3;
	}
	else return -2;
}

/****************************************************
Отправка СМС из SRAM
*****************************************************/
signed char TelitClass::SendSMS (const char* Num, char *Txt)
{
	
	_delay_ms(100);
	Telit.WriteStr_P(PSTR("AT+CMGS=\""));
	Telit.WriteStr_P(Num);
	Telit.WriteStr_P(PSTR("\"\r\n"));
	
	if (Telit.WaitResp(MORE, pause256)>0)
	{
		_delay_ms(1000);
		Telit.WriteStr(Txt);
		Telit.WriteStr_P(CTRLZ);
		Telit.WriteStr_P(PSTR("\"\r\n"));
		if (Telit.WaitResp(OK, pause256 * 2)>0)
		{
			return 1;
		}
		else return -3;
	}
	else return -2;				
}

void put_integer( signed int i )
{
	//! Local variables
	int ii;
	unsigned char int_buf[5];

	if (i < 0)                                              //Integer is negative
	{
		i = -i;                                             //Convert to positive Integer
		Telit.Write('-');                                   //Print - sign
	}

	for (ii=0; ii < 5; )                                    //Convert Integer to char array
	{
		int_buf[ii++] = '0'+ i % 10;                        //Find carry using modulo operation
		i = i / 10;                                         //Move towards MSB
	}
	do{ ii--; }while( (int_buf[ii] == '0') && (ii != 0) );  //Remove leading 0's
	do{ Telit.Write( int_buf[ii--] ); }while (ii >= 0);     //Print int->char array convertion
}

signed char TelitClass::ReadSMS(unsigned char index, char* pDst, char* pSenderID)
{
	char* Src;
	_delay_ms(100);
	Telit.WriteStr_P(PSTR("AT+CMGR="));
	put_integer( (signed int) index );
	Telit.WriteStr_P(PSTR("\r\n"));
	// Ждем ответ от команды AT+CMGR
	if (Telit.WaitResp(PSTR("\r\n\OK"), pause256) > 0)
	{
			Src = (char*)&rx_buffer[0];			
			while (*Src++ != ','); // Пропускаем всё до первой запятой			
			ParseQuotes(Src, pSenderID); // Парсим номер отправителя
			while (*Src++ != 0x0A); // Пропускаем всё до 0x0A = <LF>
				// Копируем текст СМС до первого символа 0x0D
				
				while ((*Src != 0x0d) && (*Src))
				{					
					*pDst++ = *Src++;
				}
				*pDst = 0x00;
				return 1;
	}else return -1;
}



signed char TelitClass::ReadListedSMS(char* pDst, char* pSenderID)
{
		char* Src;
		unsigned char i = 0;
		_delay_ms(100);
		Telit.WriteStr_P(PSTR("AT+CMGL=\"REC UNREAD\"\r\n"));
		// Ждем ответ от команды AT+CMGL=0
			if (Telit.WaitResp(PSTR("\r\n\OK"), pause256) > 0)
			{
				Src = (char*)&rx_buffer[0];
				for (i =0; i<8; i++)
				{
					if (*Src++ == '+') break; 
				}
				if (i==8) return -2; // Видимо непрочитанных СМС нет																		
												
				while (*Src++ != ','); // Пропускаем всё до первой запятой	
				Src++;
				while (*Src++ != ','); // Пропускаем всё до второй запятой	
				ParseQuotes(Src, pSenderID); // Парсим номер отправителя
				while (*Src++ != 0x0A); // Пропускаем всё до 0x0A = <LF>
				// Копируем текст СМС до первого символа 0x0D
							
				while ((*Src != 0x0d) && (*Src))
				{
					*pDst++ = *Src++;
				}
				*pDst = 0x00;
				return 1;
			} else 	return -1;				
}

char Hex2Bin(char Hex )
{
	if ((Hex >= '0') && (Hex <= '9')) return (Hex - (char)'0');
	else if ((Hex >='A') && (Hex <='F')) return (Hex + 10 - (char)'A');
	else return 0;
}

signed char TelitClass::ReadBalance(char* p, const char* Num, signed int* Money/*, const char* StopPattern*/)
{
	char* Src;
	char* pDst = i2a_buf;
	_delay_ms(100);
	unsigned int Sym16;
	char Sym8;
	Telit.WriteStr_P(PSTR("AT+CUSD=1\r\n"));	
	if (Telit.WaitResp(PSTR("\r\n\OK"), pause256) > 0)
	{
		Telit.WriteStr_P(PSTR("ATD"));
		Telit.WriteStr_P(Num);
		Telit.WriteStr_P(PSTR("\r\n"));
		//if (Telit.WaitResp(PSTR("\r\n\OK"), pause256) > 0)
		if (Telit.WaitResp(PSTR("0440")/*StopPattern*//*PSTR("002E")*/, pause256) > 0)
		{
			Src = (char*)&rx_buffer[4];
			if (*Src != '+') return -2; // Некорректный ответ
			while (*Src++ != '"'); // Пропускаем всё до первой кавычки
			//Src++;
			
			unsigned char flag = 0;
			while (*Src/**Src!= '"'*/) // Обработка соообщения UCS2
			{
				Sym16 = 0x1000* (unsigned int) Hex2Bin(Src[0])+ 0x0100* (unsigned int) Hex2Bin(Src[1]) + 0x0010 *  (unsigned int) Hex2Bin(Src[2])+ (unsigned int) Hex2Bin(Src[3]); 
				if (Sym16 == 0x002D) *pDst++ = '-';
				else if ((Sym16 >= 0x0030) && (Sym16 <= 0x0039))
				{
					Sym8 = (char) Sym16;
					*pDst++ = Sym8;	
				}
				else if ((Sym16 == 0x002e) || (Sym16 == 0x002c) || (Sym16 == 0x0440))
				{
					*pDst = 0x00;
					flag++; // подсчёт символов
					break;
				}				
				Src += 4;
			}
			
			signed int Balance = 0;
			if (flag) 
			{
				// Баланс найден
				flag = 0;
				pDst = i2a_buf;				
				if (*pDst == '-') {flag = 1; pDst++;};								
				char c;
				while(c = *pDst++)
				{					
//					if (c==' ') continue;
					c-='0';					
					Balance *= 10;
					Balance += c;
				}
				if (flag) Balance = -Balance;												
			}			
			*Money = Balance;		
			Telit.WriteStr_P(PSTR("\r\nAT+CUSD=0\r\n"));	
			if (Telit.WaitResp(PSTR("\r\n\OK"), pause256) > 0) return 1;
			else return -4;						
		}else return -2;										
	} else return -1;
}


signed char TelitClass::NewSMSindic(char Key)
{	
	_delay_ms(100);
	if (Key)
	{
		if (Telit.SendCmdWaitResp(PSTR("AT+CNMI=2,1,,,1\r\n"),OK,pause256) > 0)
		{
			return 1;
		}
		else return -1;	
	}
	else
	{
		if (Telit.SendCmdWaitResp(PSTR("AT+CNMI=2,0,,,1\r\n"),OK,pause256) > 0)
				{
					return 1;
				}
				else return -1;
		
	}
	
	
}

signed char TelitClass::DeleteAllSMS(void)
{
	_delay_ms(100);
	if (Telit.SendCmdWaitResp(PSTR("AT+CMGD=1,4\r\n"), OK, pause256) > 0)
	{
		return 1;
	}
	else return -1;
}