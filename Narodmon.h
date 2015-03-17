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
		unsigned char MAC_ID[16];							// MAC ���������� (� ���������� ���� {'1','3','A',...'4'})
		unsigned char MAC_SENSORS	[MAX_NUM_SENSORS][8];	// ���� MAC �������� � �������� ����
		signed int DATA_SENSORS	[MAX_NUM_SENSORS];			//  ��������� �������� � ������� s16
		unsigned char NUM_SENSORS;							//����� �������� ��� ��������	
} tNarodmonData;
	
class NarodmonClass{
public:	
	static void begin();
	static void end();	
	static void SetDeviceMACbyIMEI (char* pIMEI);			// ������ MAC ����������
	static void SetNumSensors (unsigned char Num);			// ������ ���-�� �������� ��� ��������
	static void SetMACnByIndex ( unsigned char Index, unsigned char* pMACn);	//������� MAC ����� ������� � �������� Index
	static void SetDATAByIndex ( unsigned char Index, signed int Data);			//��������� ������ ������� � �������� Index
	static void TelnetSend ( unsigned char (*PutSocket) (unsigned char));	//�������� ������ � ������� �� ��������� Telnet
};

extern NarodmonClass Narodmon;
