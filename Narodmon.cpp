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
#include "Narodmon.h"
tNarodmonData NarodmonData;
/*
����������� ����� � 16-������
*/		
static void i2hex(unsigned int val, char* dest, int len)
{
        register char* cp;
        register unsigned int n;
        char x;
        dest[len]=0x00;
        n = val;
        cp = &dest[len];
        while (cp > dest)
        {
                x = n & 0xF;
                n >>= 4;
                *--cp = x + ((x > 9) ? 'A' - 10 : '0');
        }
        return;
}

/*
����������� ����� � ���������� ����������
*/
static void i2a( unsigned int i, char* pOut_buf )
{

    //! Local variables
    int ii;
    char int_buf[5];
    
    for (ii=0; ii < 5; )                                    
    {
        int_buf[ii++] = '0'+ i % 10;                        
        i = i / 10;                                         
    }
    do{ ii--; }while( (int_buf[ii] == '0') && (ii != 0) );  
    do
	{ 
		*pOut_buf++= int_buf[ii--];
	} while (ii >= 0);
	*pOut_buf = 0x00;
}



/*
�������� ������ �� ������ narodmon �� ��������� Telnet
(�������������� ���������� ������� �����)

*PutSocket - ��������� �� �������, ������� ���������� ���� � �����

*/
void NarodmonClass::TelnetSend ( unsigned char (*PutSocket) (unsigned char))
{
  unsigned char* pSrc;  // ��������� ���������
  char i2a_buf[6];      //��������� �����
  char* pi2a;           // � ��� ���������  
  if (NarodmonData.NUM_SENSORS == 0) return;  // ���� ��� ��������, �� ������ �� ����������

  // ���������� MAC ����� ����������
  (PutSocket)('#');
  pSrc = NarodmonData.MAC_ID;
  for (unsigned char n = 0; n < 15; n++)
  {
    (PutSocket)(NarodmonData.MAC_ID[n]);		
  }
  (PutSocket)('\n');	  
  // ���������� ������ � ��������
  for (unsigned char i = 0; i < NarodmonData.NUM_SENSORS; i++)
  {
    (PutSocket)('#');	
    // ��� ������� ������� �������� ��� MACn    
    pSrc = &NarodmonData.MAC_SENSORS[i][0];
    for (unsigned char n = 0; n < 8; n++)
    {
	//���������� MACn ������� �� ������
  	i2hex(*pSrc++, i2a_buf, 2);
	pi2a = i2a_buf;
	while (*pi2a) (PutSocket)(*pi2a++); 					
    }	
   (PutSocket)('#');
   // ������ ���������� � ���������� ������
    signed int Datax10 = NarodmonData.DATA_SENSORS[i]; //C������ ������
    if (Datax10 < 0)
    {
	(PutSocket)('-');   // ���� �������� �������������, �� ���������� '-'
	Datax10 = -Datax10; //������������� � �������������
    }
    // ����������� �������� � ����������
    i2a((unsigned int) (Datax10 / 10), i2a_buf);
    pi2a = i2a_buf;
    while (*pi2a) (PutSocket)(*pi2a++);
    (PutSocket)('.');
    i2a((unsigned int) (Datax10 % 10), i2a_buf);
    pi2a = i2a_buf;
    while (*pi2a) (PutSocket)(*pi2a++);
    (PutSocket)('\n');
}
(PutSocket)('#');
(PutSocket)('#');    
}


// ��������� ���������� �������� Num
void  NarodmonClass::SetNumSensors (unsigned char Num)
{
	NarodmonData.NUM_SENSORS = Num;
}

//������ 8 �������� MAC-������ ��� ������� � �������� Index
void NarodmonClass::SetMACnByIndex (unsigned char Index, unsigned char* pMACn)
{
	for (unsigned char i = 0; i < 8; i++)
	{
		NarodmonData.MAC_SENSORS[Index][i] = *pMACn++;
	}
}

//������ ������ ��� ������� � �������� Index
void NarodmonClass::SetDATAByIndex (unsigned char Index, signed int Data)
{
	NarodmonData.DATA_SENSORS[Index] = Data;
}


//������ 15-�������� ID ����������
void NarodmonClass::SetDeviceMACbyIMEI (char* pIMEI)
{	
	for (unsigned char i = 0; i < 15; i++)
	{
		NarodmonData.MAC_ID[i] = *pIMEI++;								
	}
					
}