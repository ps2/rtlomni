// RF Tx for Omnipod
// F5OEO Evariste (c)2017
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject
// to the following conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
// CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#define TONE_SPACING            8789           // ~8.7890625 Hz

// Global variables

int FileFreqTiming=0;    

float BaudRate=40625;
float Deviation=29296;
void WriteTone(double Frequency,uint32_t Timing)
{
	typedef struct {
		double Frequency;
		uint32_t WaitForThisSample;
	} samplerf_t;
	samplerf_t RfSample;
	
	RfSample.Frequency=Frequency;
	RfSample.WaitForThisSample=Timing*1000L; //en 100 de nanosecond
	//printf("Freq =%f Timing=%d\n",RfSample.Frequency,RfSample.WaitForThisSample);
	if (write(FileFreqTiming, &RfSample,sizeof(samplerf_t)) != sizeof(samplerf_t)) {
		fprintf(stderr, "Unable to write sample\n");
	}

}

void WriteFSK(unsigned char bit)
{
    uint32_t Delay=1e9*1.0/BaudRate;
    if(bit==0) 
        WriteTone(-Deviation,Delay);
    else
        WriteTone(Deviation,Delay);
       
}

void WriteByteManchester(unsigned char Byte)
{
    for(int i=7;i>=0;i--)
    {
        if(((Byte>>i)&0x1)==0) 
        {
            WriteFSK(0);
            WriteFSK(1);    
        }
        else
        {
            WriteFSK(1);
            WriteFSK(0);
        }
    }
}

void WriteSync()
{
    for(int i=0;i<10;i++)
    {
        WriteByteManchester(0x54);
    }
    WriteByteManchester(0xC3);    
}

void WriteEnd()
{
       for(int i=0;i<5;i++)
           WriteFSK(1);
}
 
int main(int argc, char **argv)
{
    
	char *sText;
	if (argc > 2) 
	{
		sText=(char *)argv[1];
		//FileText = open(argv[1], O_RDONLY);

		char *sFileFreqTiming=(char *)argv[2];
		FileFreqTiming = open(argv[2], O_WRONLY|O_CREAT, 0644);
	}
	else
	{
		printf("usage : omnitx StringToTransmit file.ft\n");
		exit(0);
	}
  
    char sHexDigit[2]="A";
    char *End;

    WriteSync();
	for(int i=0;i<strlen(sText);i++)
	{
        sHexDigit[0]=sText[i];
        WriteByteManchester((unsigned char)strtol(sHexDigit,&End,16));

	}
    WriteEnd(); 
	close(FileFreqTiming);
}
 
