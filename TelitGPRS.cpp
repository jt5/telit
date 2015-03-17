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
#include "TelitGPRS.h"
#include "stuff.h"

//activation of context
signed char TelitClass::ContextInit(const char* ISP_IP)
{
	_delay_ms(100);
	Telit.WriteStr_P(PSTR("\r\nAT+CGDCONT=1,\"IP\",\""));
	Telit.WriteStr_P(ISP_IP);
	if (SendCmdWaitResp(PSTR("\""), OK, pause256 * 10) > 0 )	return 1;
	else return -1;
}


signed char TelitClass::ContextActivation(const char* LOGIN, const char* PWD)
{
	_delay_ms(1000);
	Telit.WriteStr_P(PSTR("\r\nAT#SGACT=1,1,'"));
	Telit.WriteStr_P(LOGIN);
	Telit.WriteStr_P(PSTR("','"));
	Telit.WriteStr_P(PWD);
	if (SendCmdWaitResp(PSTR("'\r\n"), OK, pause256 * 2) < 0) return -1;
	else return 1;
	
}	
signed char TelitClass::ContextDeactivation(void)
{
		_delay_ms(100);
		Telit.WriteStr_P(PSTR("\r\nAT#SGACT=1,0\r\n"));
		if (SendCmdWaitResp(PSTR(""), OK, pause64) < 0) return -1;
		else return 1;
}


//open socket
signed char TelitClass::SocketOpen(unsigned char connId, unsigned char txProt, const char* SERVER_IP, unsigned int SERVER_PORT, unsigned int lPort)
{
	char* pi2a;
	_delay_ms(100);
	Telit.WriteStr_P(PSTR("\r\nAT#SD="));
	// печатаем номер соединения 1...6
	i2a(connId, i2a_buf);
	pi2a = i2a_buf;
	while (*pi2a) Telit.Write(*pi2a++);
	Telit.WriteStr_P(PSTR(","));
	// печатаем тип сокета 1 - UDP/ 0 - TCP
	i2a(txProt, i2a_buf);
	pi2a = i2a_buf;
	while (*pi2a) Telit.Write(*pi2a++);
	Telit.WriteStr_P(PSTR(","));
	// печатаем порт
	i2a(SERVER_PORT, i2a_buf);
	pi2a = i2a_buf;
	while (*pi2a) Telit.Write(*pi2a++);
	Telit.WriteStr_P(PSTR(",\""));
	// IP или Domain Name
	Telit.WriteStr_P(SERVER_IP);
	Telit.WriteStr_P(PSTR("\",255,")); // Сокет закроется автоматически после +++
	// печатаем порт входящих соединений UDP
	i2a(lPort, i2a_buf);
	pi2a = i2a_buf;
	while (*pi2a) Telit.Write(*pi2a++);
	Telit.WriteStr_P(PSTR(",0"));
	if (SendCmdWaitResp(PSTR(""), PSTR("CONNECT"), pause256 *  10/*Здесь требуется большая пауза тайм-аута*/) < 0) return -1; // Сокет не открылся! 
	return 1;	// Сокет открыт
}

//WriteSocket
signed char TelitClass::ActiveSocketWriteCb (void (*CALLBACK_FUNC)(unsigned char(*SocketWr)(unsigned char)))
{
	(CALLBACK_FUNC)(&Telit.Write);
};


signed char TelitClass::ActiveSocketWriteStr (char* Str)
{
	Telit.WriteStr(Str);
};


signed char TelitClass::ActiveSocketWriteStr_P (const char* Str)
{
	Telit.WriteStr_P(Str);
};

signed char TelitClass::ActiveSocketSuspend ()
{
	_delay_ms(1000);
	Telit.WriteStr_P(PSTR("+++"));//Разрываем соединение
	if (WaitResp(OK, pause256) < 0) return -1;
	//_delay_ms(1000);
	return 1;
};

signed char TelitClass::SocketClose (unsigned char connId)
{
	_delay_ms(100);
	ActiveSocketSuspend ();
	Telit.WriteStr_P(PSTR("AT#SH="));//Разрываем соединение
	// печатаем номер соединения 1...6
	i2a(connId, i2a_buf);
	char* pi2a = i2a_buf;
	while (*pi2a) Telit.Write(*pi2a++);
	if (SendCmdWaitResp(PSTR(""), OK, pause256 *  2) < 0) return -1; // Сокет не закрылся!
	return 1;
};

signed char TelitClass::SocketResume (unsigned char connId)
{
	_delay_ms(100);
	Telit.WriteStr_P(PSTR("AT#SO="));//Возобновляем соединение
	// печатаем номер соединения 1...6
	i2a(connId, i2a_buf);
	char* pi2a = i2a_buf;
	while (*pi2a) Telit.Write(*pi2a++);
	if (SendCmdWaitResp(PSTR(""), PSTR("CONNECT"), pause256 *  10) < 0) return -1; // Сокет не открылся!
};



signed char ActiveSocketReadStr (char* Str)
{
	//*Str++ = getchar ();
};


signed char TelitClass::NTPSync (const char* pIP, char* pDst)
{
	char* Src;
	_delay_ms(100);
	Telit.WriteStr_P(PSTR("AT#SELINT=2\r\n"));	
	_delay_ms(1000);
	Telit.WriteStr_P(PSTR("\r\nAT#NTP=\""));
	Telit.WriteStr_P(pIP);
	if (SendCmdWaitResp(PSTR("\",123,1,2"), OK, pause256 * 10) > 0 )	
	{
		/*Src = (char*) &rx_buffer[0];
		//Пришел ответ от NTP сервера
		while ((*Src != 0x0d) && (*Src))
		{
			*pDst++ = *Src++;
		}
		*pDst = 0x00;*/
		return 1;
	}
	else return -1;
};