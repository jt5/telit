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



#define MAX_NUM_SENSORS (4)

typedef struct
{
		unsigned char MAC_ID[16];							// MAC устройства (в символьном виде {'1','3','A',...'4'})
		unsigned char MAC_SENSORS	[MAX_NUM_SENSORS][8];	// коды MAC датчиков в бинарном виде
		signed int DATA_SENSORS	[MAX_NUM_SENSORS];			//  Показания датчиков в формате s16
		unsigned char NUM_SENSORS;							//Число датчиков для отправки	
} tNarodmonData;
	
class NarodmonClass{
public:	
	static void begin();
	static void end();	
	static void SetDeviceMACbyIMEI (char* pIMEI);			// Задать MAC устройства
	static void SetNumSensors (unsigned char Num);			// Задать кол-во датчиков для отправки
	static void SetMACnByIndex ( unsigned char Index, unsigned char* pMACn);	//Указать MAC адрес датчика с индексом Index
	static void SetDATAByIndex ( unsigned char Index, signed int Data);			//Запомнить данные датчика с индексом Index
	static void TelnetSend ( unsigned char (*PutSocket) (unsigned char));	//Отправка пакета с данными по протоколу Telnet
};

extern NarodmonClass Narodmon;
