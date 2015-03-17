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
#define F_CPU     16000000UL/**< Системная частота, МГц */
#define Telit_USART0
#define pause64 64
#define pause256 256
#define RX_BUFFER_SIZE	192
#define RX_BUFFER_MASK	(RX_BUFFER_SIZE - 2)

extern unsigned char rx_buffer[RX_BUFFER_SIZE];

//! Всевозможные низкоуровневые ответы, запросы
const char OK[] PROGMEM			  = "OK\r\n";
const char RING[] PROGMEM         = "RING\r";
const char CLIP[] PROGMEM         = "+CLIP: ";
const char CR_LF[] PROGMEM        = "\r";
const char MORE[] PROGMEM         = ">";
const char CPIN[] PROGMEM         = "+CPIN: ";
const char CGATT[] PROGMEM        = "+CGATT: 1";
const char CMTI[] PROGMEM         = "+CMTI: ";

class TelitClass{
	public:
	static void begin();
	static void end();
	static unsigned char Write(unsigned char Byte);			//Отправка байта
	static void WriteStr(char *String);				//Отправка строки модему из ОЗУ
	static void WriteStr_P(const char *PString);	//Отправка строки модему из ПЗУ
	static signed char Init(const char *PIN);				//Инициализация с вводом ПИН-кода		
	static signed char CheckStatus(void);					//Проверка статуса соединения (регистрации в сети)
	static signed char GetIMEI (char* IMEIbuf);				//Получить IMEI модема
	static signed char PowerOFF();							//Выключить модем
	static signed char SendCmdWaitResp(const char* pCommand_P, const char* pSearchPattern_P, unsigned int pause);
	static signed char WaitResp(const char* pSearchPattern_P, unsigned int pause);
	static void WaitSMS(void);
	static signed char getTime(int* hr,int* min,int* sec,int* day, int* month, int* yr);
	
	// В модуле SMS.cpp
	static signed char CheckSMS (void);
	static signed char SendSMS_P (const char* Num, const char *Txt);
	static signed char SendSMS (const char* Num, char *Txt);
	static signed char ReadSMS(unsigned char index, char* Dst, char* pSenderID);
	static signed char ReadListedSMS(char* Dst, char* pSenderID);
	static signed char NewSMSindic(char Key);
	static signed char DeleteAllSMS(void);
	static signed char ReadBalance(char* p, const char* Num, signed int* Money/*, const char* StopPattern*/);
	
	// В модуле GPRS.cpp
	static signed char ContextInit(const char* ISP_IP);
	static signed char ContextActivation(const char* LOGIN, const char* PWD);
	static signed char ContextDeactivation(void);
	static signed char SocketOpen(unsigned char connId, unsigned char txProt, const char* SERVER_IP, unsigned int SERVER_PORT, unsigned int lPort);
	static signed char ActiveSocketWriteCb (void (*CALLBACK_FUNC)(unsigned char(*SocketWr)(unsigned char)));
	static signed char ActiveSocketWriteStr (char* Str);
	static signed char ActiveSocketWriteStr_P (const char* Str);
	static signed char ActiveSocketSuspend ();
	static signed char SocketClose (unsigned char connId);
	static signed char SocketResume (unsigned char connId);
	static signed char NTPSync (const char* pIP, char* pDst);
};

extern volatile unsigned char IncomingCall;
extern volatile unsigned char NewSMS_index;
extern char CallerID[16];
extern TelitClass Telit;