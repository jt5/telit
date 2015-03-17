#include "stuff.h"
char i2a_buf[32];

void i2a( unsigned int i, char* pOut_buf )
{

	//! Local variables
	int ii;
	char int_buf[5];
	
	for (ii=0; ii < 5; )                                    //Convert Integer to char array
	{
		int_buf[ii++] = '0'+ i % 10;                        //Find carry using modulo operation
		i = i / 10;                                         //Move towards MSB
	}
	do{ ii--; }while( (int_buf[ii] == '0') && (ii != 0) );  //Remove leading 0's
	do
	{
		*pOut_buf++= int_buf[ii--];
	} while (ii >= 0);
	*pOut_buf = 0x00;
}

void ParseQuotes (char* Src, char* Dst)
{
	volatile unsigned char f = 0;
	char c;
	while (c = *Src++)
	{
		if (f == 0)
		{
			if (c == 34)
			{
				f = 1;
				continue;
			}
			else continue;
		}
		else
		{
			if (c == 34)
			{
				break;
			}
			else
			{
				*Dst++ = c;
				//putchar(c);
			}
		}
	}
	*Dst = 0x00;
}
