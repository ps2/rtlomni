/* Created by Evariste Courjaud F5OEO. Code is GPL
rtlomni is a software to sniff RF packets using a RTLSDR dongle in order to analysis Omnipod protocol.

Credits :

This work is mainly based on https://github.com/ps2/omnipod_rf

Hope this could help https://github.com/openaps/openomni

SDR demodulation and signal processing is based on excellent https://github.com/jgaeddert/liquid-dsp/

Licence : 
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <math.h>

#include <stdint.h>

#include <liquid/liquid.h>


/*
433.923MHz center signal
2-FSK, with 26.37kHz deviation
40625bps data rate (before manchester)
Manchester coded, non-ieee
SYNCM_CARRIER_16_of_16 (16/16 sync word bits detected)
MFMCFG1_NUM_PREAMBLE_8 (8 bytes of preamble)
Sync word: 0x54c3
8-bit crc with standard/common polynomal
*/

enum {Debug_FSK,Debug_Manchester,Debug_Packet,Debug_Message};
enum {ACK=0b010,CON=0b100,PDM=0b101,POD=0b111};    

int min( int a, int b ) { return a < b ? a : b; }

int DebugLevel=Debug_Message;

// RF Layer Global
#define MAXPACKETLENGTH 4096
unsigned char BufferData[MAXPACKETLENGTH];
int IndexData=0;
FILE* iqfile=NULL;
FILE *DebugIQ=NULL; 
FILE *ManchesterFile=NULL;
#ifdef DEBUG_FM
FILE *DebugFM=NULL; 
#endif


#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"


//***************************************************************************************************
//*********************************** NONCE GENERATION***********************************************
//***************************************************************************************************

#define MAX_NOUNCES_PROCESS 1000
#define MAX_NONCE_RESYNC 256

unsigned long *TabNounce=NULL;
unsigned long mlot=0,mtid=0;
unsigned long a7[18];

int GeneralIndexNounce=-1;

unsigned long GenerateEntryNonce()
{
        a7[0] = ((a7[0] >> 16) + (a7[0] & 0xFFFF) * 0x5D7F) & 0xFFFFFFFF;
        a7[1] = ((a7[1] >> 16) + (a7[1] & 0xFFFF) * 0x8CA0) & 0xFFFFFFFF;
        return ((a7[1] + (a7[0] << 16)) & 0xFFFFFFFF);
}

void InitNounce(unsigned long lot, unsigned long tid,int F7,int F8)
{
         unsigned long Nonce=0;
        unsigned char  byte_F9=0; 
        if(TabNounce==NULL)
        {    
            TabNounce=(unsigned long*)malloc(MAX_NOUNCES_PROCESS*sizeof(unsigned long));
        }
        
        //if((mlot==lot)&&(mtid==tid)) return;
        mlot=lot;
        mtid=tid;
        
        a7[0]=(lot & 0xFFFF) + 0x55543DC3 + (lot >> 16);
        a7[0]&=0xFFFFFFFF;
        a7[1]=(tid & 0xFFFF) + 0xAAAAE44E + (tid >> 16);
        a7[1]&=0xFFFFFFFF;
       
        
       
            a7[0]+=(F7&0xFF);
            a7[1]+=(F8&0xFF);
        //printf("A7_0=%lx A7_1=%lx\n",a7[0],a7[1]);

        for(int i=2;i<18;i++)
        {
           a7[i]=GenerateEntryNonce();      
        } 
        
        if(F7==0)
            byte_F9 = (a7[0] + a7[1]) & 0xF; 
        else
            byte_F9=1;
        for(int i=0;i<=MAX_NOUNCES_PROCESS;i++)
        {

            Nonce=a7[2+byte_F9];
            TabNounce[i]=Nonce;
            a7[2 + byte_F9] = GenerateEntryNonce();
            byte_F9=Nonce&0xF;

        }
        
        
}


unsigned long GetNounce(int IndexNounce)
{
   if(TabNounce==NULL) {/*printf("Nonce not init\n");*/return 0;}
    return TabNounce[IndexNounce];
}

int CheckNonce(unsigned long Nounce)
{
    for(int k=0;k<MAX_NONCE_RESYNC;k++)
    {
        for(int j=0;j<MAX_NONCE_RESYNC;j++)
        {
            for(int i=0;i<MAX_NOUNCES_PROCESS;i++)
            {

               if(GetNounce(i)==Nounce)
               {
                  if(j!=0) printf("F7 %d F8 %d",j,k);   
                  if(GeneralIndexNounce==-1) GeneralIndexNounce=i;
                  
                  if((GeneralIndexNounce==i)||(GeneralIndexNounce+1==(i)))
                    GeneralIndexNounce=i;
                   else
                   {
                      printf(ANSI_COLOR_RED);   
                     printf("--Nonce skipped %d/%d--",GeneralIndexNounce,i);
                     GeneralIndexNounce=i; // We set with the new index found   
                     printf(ANSI_COLOR_GREEN);      
                   } 
                  
                  return i;
                }
            }
            GeneralIndexNounce=-1;
            //The nonce reset simply increments a counter that is added to the lot number. If you use Lot 42540, TID 310475 you get the new nonce 2e76fcee and all is fine.
             // Search if nonce errors
            
            InitNounce(mlot,mtid,j,k);
        }
    }
    return -1;
}



//***************************************************************************************************
//*********************************** SUB-MESSAGE LAYER ******************************************************
//***************************************************************************************************

// From WIKI 
/*
Command 0E requests status from the Pod.

Command format:

byte 0E: mtype
byte 01: length
byte: request type. 0, 1, 2, 3, 5, 6, 0x46, 0x50, or 0x51.
A request of type 0 (the usual PDM status request) yields a Status response 1D. Other requests yield a Response 02, returning different data depending on the request type.
*/

unsigned char printbit(unsigned char Byte,int bitstart,int bitstop)
{
    if(bitstop>9) printf("biterror");
    for(int i=bitstop;i>=bitstart;i--)
    {
        printf("%d",((Byte>>i)&1));
        
    }
    return(Byte>>bitstart);

}

void InterpretSubMessage(int Source,int Type,unsigned char *SubMessage,int Length,int SeqMessage)
{
    enum {Cmd_GetConfig=3,Cmd_Pairing=7,Cmd_GetStatus=0xe,Cmd_InsulinScheduleExtra=0x17,Cmd_SyncTime=0x19,Cmd_InsulinSchedule=0x1a,Cmd_CancelBolus=0x1F};
    enum {Resp_Status=0x1D,Resp_Tid=0x01,Resp02=0x02,RespError=0x06};
    const char *TypeInsulin[]={"Basal","Temp Basal","Bolus"};

   
    if(Source==POD)
    {    
        printf(ANSI_COLOR_GREEN);
        printf("#%d POD %02x",SeqMessage,Type);
    }
    if(Source==PDM)
    {    
        printf(ANSI_COLOR_BLUE);
        printf("#%d PDM %02x",SeqMessage,Type);
    }
    printf("(%d)->",Length);
    switch(Type)
    {
        case Cmd_GetConfig:printf("GetConfig");
        //https://github.com/openaps/openomni/wiki/Command-03
        {
            printf("POD Add=%02x%02x%02x%02x",SubMessage[0],SubMessage[1],SubMessage[2],SubMessage[3]);printf(" ");
            printf("Unknwown %02x",SubMessage[5]);printf(" ");
            printf("Date %02x%02x%02x%02x%02x (%d/%d/---)",SubMessage[6],SubMessage[7],SubMessage[8],SubMessage[9],SubMessage[10],SubMessage[6],SubMessage[7]);

            if(Length==0x13)
            {
                 unsigned long Lot=(((unsigned long)SubMessage[11])<<24)|(((unsigned long)SubMessage[12])<<16)|(((unsigned long)SubMessage[13])<<8)|(((unsigned long)SubMessage[14]));
                 unsigned long Tid=(((unsigned long)SubMessage[15])<<24)|(((unsigned long)SubMessage[16])<<16)|(((unsigned long)SubMessage[17])<<8)|(((unsigned long)SubMessage[18])); 
                printf("Lot=%lx(L%ld)",Lot,Lot);printf(" ");
                printf("Tid=%lx(T%ld)",Tid,Tid);printf(" ");
                
               
            }
        }    
        break;
        case Cmd_Pairing:
            //https://github.com/openaps/openomni/wiki/Command-07
             printf("Pairing with ID %02x%02x%02x%02x",SubMessage[0],SubMessage[1],SubMessage[2],SubMessage[3]);
        break;
        case Cmd_GetStatus:
            //https://github.com/openaps/openomni/wiki/Command-0E
            printf("Get Status type %02x",SubMessage[0]);break;  
        case Cmd_InsulinSchedule:
            //https://github.com/openaps/openomni/wiki/Insulin-Schedule-Command
        {
            printf("Insulin Schedule:"); 
             unsigned long CurrentNonce=0;
            CurrentNonce=(((unsigned long)SubMessage[0])<<24)|(((unsigned long)SubMessage[1])<<16)|(((unsigned long)SubMessage[2])<<8)|(((unsigned long)SubMessage[3]));
             int IndexNounce=CheckNonce(CurrentNonce);
        
            printf("Nonce:%02x%02x%02x%02x(%d)",SubMessage[0],SubMessage[1],SubMessage[2],SubMessage[3],IndexNounce);printf(" ");
            printf("Type:%02x %s",SubMessage[4],TypeInsulin[SubMessage[4]&0x3]);printf(" ");
            int Type=SubMessage[4];
                    

            switch(Type) // To be completed with other modes : Fixme !
            {
                case 0x2://BOLUS
                {
                    int CheckSum=0;
                   for(int i=7;i<14;i++)
                        CheckSum+=SubMessage[i];
                   CheckSum=CheckSum&0xFFFF;

                    printf("CheckSum:%02x%02x/%02x%02x",SubMessage[5],SubMessage[6],CheckSum>>8,CheckSum&0xFF);printf(" ");
                    printf("Duration:%02x(%d minutes)",SubMessage[7],SubMessage[7]*30);printf(" ");
                    printf("FiledA:%02x%02x",SubMessage[8],SubMessage[9]);printf(" ");
                    float UnitRate=0.05;
                    //if((SubMessage[10]*256+SubMessage[11])==0x0040) UnitRate=0.1;
                    //if((SubMessage[10]*256+SubMessage[11])==0x0060) UnitRate=0.05;
                    //printf("UnitRate:%0.1f",SubMessage[10],SubMessage[11],(SubMessage[10]*256+SubMessage[11])*0.1);printf(" ");
                    printf("UnitRate:%02x%02x(%0.1fU)",SubMessage[10],SubMessage[11],(SubMessage[10]*256+SubMessage[11])*UnitRate);printf(" ");
                    printf("UnitRateSchedule:%02x%02x(%0.1fU)",SubMessage[12],SubMessage[13],(SubMessage[12]*256+SubMessage[13])*UnitRate);printf(" ");
                }    
                break;
            }
        }
        break;
        case Cmd_InsulinScheduleExtra:
        //https://github.com/openaps/openomni/wiki/Command-17---Bolus-extra    
        printf("InsulinExtra");
        if(Length==0x10) printf("(long):");
        if(Length==0xD) printf("(short):");    
        printf("Immediate");
        printf("\n");
        break;
        case Cmd_CancelBolus:
        //https://github.com/openaps/openomni/wiki/Command-1F        
        {
             printf("Cancel :");
            
            unsigned long CurrentNonce=0;
            CurrentNonce=(((unsigned long)SubMessage[0])<<24)|(((unsigned long)SubMessage[1])<<16)|(((unsigned long)SubMessage[2])<<8)|(((unsigned long)SubMessage[3]));
            int IndexNounce=CheckNonce(CurrentNonce);
            printf("Nonce %02x%02x%02x%02x(%d)",SubMessage[0],SubMessage[1],SubMessage[2],SubMessage[3],IndexNounce);printf(" ");
            printf("Type %x(%s)",SubMessage[4]&0x3,TypeInsulin[SubMessage[4]&0x3]);
        }
        break;
        case 0x13 : 
        //https://github.com/NightscoutFoundation/omni-firmware/blob/master/c_code/process_input_message_and_create_output_message.c#L479
         printf(ANSI_COLOR_RED);
        printf("Submessage need to be analyzed ");
        printf("\n");
        break;
        
        case Cmd_SyncTime:
        //https://github.com/openaps/openomni/wiki/Command-19
        {
            printf("CancelTime:");   
            unsigned long CurrentNonce=0;
            CurrentNonce=(((unsigned long)SubMessage[0])<<24)|(((unsigned long)SubMessage[1])<<16)|(((unsigned long)SubMessage[2])<<8)|(((unsigned long)SubMessage[3]));
            int IndexNounce=CheckNonce(CurrentNonce);
            
            printf("Nonce %02x%02x%02x%02x(%d)",SubMessage[0],SubMessage[1],SubMessage[2],SubMessage[3],IndexNounce);printf(" ");     
                   
            
            uint Time=((SubMessage[4]&0x1)<<8)|SubMessage[5];
            printf("for %d minutes",(Time+15));
        }
        break;
        
        case Resp_Status: 
        //https://github.com/openaps/openomni/wiki/Status-response-1D
        /*The 1D response has the following form:

        byte 1D: The message type.
        byte: bits ABCDEEEE. Bits A, B, C, D indicate values of internal table 7. 4-bit value EEEE is an important internal state value.
        dword: 4 zero bits. 13 bits with Table1[2]. 4 bits (maybe message sequence number). 11 bits (sum of various Table entries divided by 10 and rounded up).
        dword: 1 bit (indicates event 0x14 was logged). 8 bits (internal value). 13 bits (Tab1[1]). 10 bits (Tab1[0]).
        */
        printf("Resp Status"); 
            if(Length>=9)
            {
                        printf(":");printf("Table7:");printbit(SubMessage[0],4,7);printf(" EEEE:");printbit(SubMessage[0],0,3);printf(" ");
                        printf("4zero:");printbit(SubMessage[1],4,7);printf(" ");
                        printf("Table1[2]:");printbit(SubMessage[1],0,3);printbit(SubMessage[2],0,7);printbit(SubMessage[3],7,7);printf(" ");
                        printf("seqnumb:");printbit(SubMessage[3],3,6);printf(" ");
                        printf("sum table:");printbit(SubMessage[3],0,2);printbit(SubMessage[4],0,7);printf(" ");    
                       
                        printf("Event14:");printbit(SubMessage[5],7,7);printf(" ");
                        printf("Internal value:");printbit(SubMessage[5],0,6);printbit(SubMessage[6],7,7);printf(" ");
                        printf("Minutes Actives %d",((SubMessage[5]&0x3F)<<6)+(SubMessage[6]>>7));printf(" ");

                        //printf("Tab1[1]:");printbit(SubMessage[6],0,6);printbit(SubMessage[7],2,7);printf(" ");
                        printf("Tab1[1]:%04x",((SubMessage[6]&0x7F)<<6)+((SubMessage[7]>>2)&0x3F));printf(" ");
                        int Reservoir=(((SubMessage[6]&0x03)<<6)+(SubMessage[7]>>2));
                        if((Reservoir&0xFF)!=0xFF)
                            printf("Reservoir Level %0.01fU",(((SubMessage[6]&0x03)<<6)+(SubMessage[7]>>2))*50.0/256.0);  
                        /*else
                            printf("Reservoir Level %0.01fU",200.0-((SubMessage[6]&0x7F)>>2)*1);  // 200U is the max, POD has maybe not sensor over 50 to measure*/

                        printf("Tab1[0]:%04x",((SubMessage[7]&0x3)<<6)+(SubMessage[8]));printf(" ");

            }
        break;
        case Resp_Tid:
        //https://github.com/openaps/openomni/wiki/Response-01
        {
            printf("ResTid:");
            if(Length==0x1b)
            {
                printf("PM %d.%d.%d/PI %d.%d.%d.",SubMessage[7],SubMessage[8],SubMessage[9],SubMessage[10],SubMessage[11],SubMessage[12]);printf(" ");
                printf("State %02x",SubMessage[14]);printf(" ");
                 unsigned long Lot=(((unsigned long)SubMessage[15])<<24)|(((unsigned long)SubMessage[16])<<16)|(((unsigned long)SubMessage[17])<<8)|(((unsigned long)SubMessage[18]));
                 unsigned long Tid=(((unsigned long)SubMessage[19])<<24)|(((unsigned long)SubMessage[20])<<16)|(((unsigned long)SubMessage[21])<<8)|(((unsigned long)SubMessage[22])); 
                 InitNounce(Lot,Tid,0,0);
                printf("Lot=%lx(L%ld)",Lot,Lot);printf(" ");
                printf("Tid=%lx(T%ld",Tid,Tid);printf(" ");
                printf("Pod Add=%02x%02x%02x%02x",SubMessage[23],SubMessage[24],SubMessage[25],SubMessage[26]);printf(" "); 
            }
            if(Length==0x15)    
            {
                printf("PM %d.%d.%d/PI %d.%d.%d.",SubMessage[0],SubMessage[1],SubMessage[2],SubMessage[3],SubMessage[4],SubMessage[5]);printf(" ");
                printf("State %02x",SubMessage[7]);printf(" ");
                unsigned long Lot=(((unsigned long)SubMessage[8])<<24)|(((unsigned long)SubMessage[9])<<16)|(((unsigned long)SubMessage[10])<<8)|(((unsigned long)SubMessage[11]));
                unsigned long Tid=(((unsigned long)SubMessage[12])<<24)|(((unsigned long)SubMessage[13])<<16)|(((unsigned long)SubMessage[14])<<8)|(((unsigned long)SubMessage[15])); 
                 InitNounce(Lot,Tid,0,0);
                
                printf("Lot=%lx(L%ld)",Lot,Lot);printf(" ");
                printf("Tid=%lx(T%ld)",Tid,Tid);printf(" ");
                printf("RSSI=%02x",SubMessage[16]);printf(" ");
                printf("Pod Add=%02x%02x%02x%02x",SubMessage[17],SubMessage[18],SubMessage[19],SubMessage[20]);printf(" "); 
            }
        }   
        break; 
        case Resp02:
        //https://github.com/openaps/openomni/wiki/Response-02
        {
            printf("Resp02:");
            for(int i=0;i<Length;i++) printf("%02x",SubMessage[i]);
        }
        break;
        case RespError:
        //https://github.com/NightscoutFoundation/omni-firmware/blob/e7a217005c565c020a9f3b9f73e06d04a52b2b4c/c_code/process_input_message_and_create_output_message.c#L871
        //https://github.com/NightscoutFoundation/omni-firmware/blob/e7a217005c565c020a9f3b9f73e06d04a52b2b4c/c_code/generate_output.c#L344
        {
             printf("POD Error:");
             int Type=SubMessage[0];
             printf("Type:%02x ",Type);
             switch(Type)
             {
                case 0x14:
                {
                    //(word_F5 + crc_table[byte_FA] + lot_number + serial_number) ^ word_F7;
                    //https://github.com/NightscoutFoundation/omni-firmware/blob/e7a217005c565c020a9f3b9f73e06d04a52b2b4c/c_code/nonce.c#L5
                    printf("Nonce ErrorHx=%02x%02x",SubMessage[1],SubMessage[2]);
                }
                break;
             }   
                
        }
        break;
        default:
        printf(ANSI_COLOR_RED);
        printf("Submessage not parsed :%02x(%d)",Type,Length);
        for(int i=0;i<Length;i++) printf("%02x",SubMessage[i]);
        printf("\n");
        
        break; 

    }
    printf(ANSI_COLOR_RESET);
    
}



void ParseSubMessage(int Seq,int Source,unsigned char *Message,int Length,int SeqMessage)
{
    int i=0;
    int nbsub=0;
    //printf("\nSUBMESSAGES:\n");
    //printf("Packet %d Message %d-------------------------------------------\n",Seq,SeqMessage);
    while(i<Length)
    {
        //if(Source==POD) printf("POD:"); else printf("PDM:");
        //printf("(%d:%d)\tCommand %02x->",Seq,nbsub,Message[i]);    
        //printf("%02x->",Message[i]);    
        int Type=Message[i++];
        unsigned char Submessage[255];
        //printf("%02x+",Message[i]);
        int SubLength=Message[i++];
       
        if(Type==0x1D) SubLength=Length; //Unlike other messages, the second byte holds data rather than the message length. The reason for this is unknown.
        for(int j=0;j<SubLength;j++)
        {    
           Submessage[j]=Message[i++];
           //printf("%02x",Submessage[j]);
           
        }
        InterpretSubMessage(Source,Type,Submessage,SubLength,SeqMessage); 
        nbsub++;
 
        printf("\n");        

    }
    
    
}
 

//***************************************************************************************************
//*********************************** MESSAGE LAYER ******************************************************
//***************************************************************************************************
 



unsigned int crc_table16[] = {0x0000,0x8005,0x800f,0x000a,0x801b,0x001e,0x0014,0x8011,0x8033,
               0x0036,0x003c,0x8039,0x0028,0x802d,0x8027,0x0022,0x8063,0x0066,
               0x006c,0x8069,0x0078,0x807d,0x8077,0x0072,0x0050,0x8055,0x805f,
               0x005a,0x804b,0x004e,0x0044,0x8041,0x80c3,0x00c6,0x00cc,0x80c9,
               0x00d8,0x80dd,0x80d7,0x00d2,0x00f0,0x80f5,0x80ff,0x00fa,0x80eb,
               0x00ee,0x00e4,0x80e1,0x00a0,0x80a5,0x80af,0x00aa,0x80bb,0x00be,
               0x00b4,0x80b1,0x8093,0x0096,0x009c,0x8099,0x0088,0x808d,0x8087,
               0x0082,0x8183,0x0186,0x018c,0x8189,0x0198,0x819d,0x8197,0x0192,
               0x01b0,0x81b5,0x81bf,0x01ba,0x81ab,0x01ae,0x01a4,0x81a1,0x01e0,
               0x81e5,0x81ef,0x01ea,0x81fb,0x01fe,0x01f4,0x81f1,0x81d3,0x01d6,
               0x01dc,0x81d9,0x01c8,0x81cd,0x81c7,0x01c2,0x0140,0x8145,0x814f,
               0x014a,0x815b,0x015e,0x0154,0x8151,0x8173,0x0176,0x017c,0x8179,
               0x0168,0x816d,0x8167,0x0162,0x8123,0x0126,0x012c,0x8129,0x0138,
               0x813d,0x8137,0x0132,0x0110,0x8115,0x811f,0x011a,0x810b,0x010e,
               0x0104,0x8101,0x8303,0x0306,0x030c,0x8309,0x0318,0x831d,0x8317,
               0x0312,0x0330,0x8335,0x833f,0x033a,0x832b,0x032e,0x0324,0x8321,
               0x0360,0x8365,0x836f,0x036a,0x837b,0x037e,0x0374,0x8371,0x8353,
               0x0356,0x035c,0x8359,0x0348,0x834d,0x8347,0x0342,0x03c0,0x83c5,
               0x83cf,0x03ca,0x83db,0x03de,0x03d4,0x83d1,0x83f3,0x03f6,0x03fc,
               0x83f9,0x03e8,0x83ed,0x83e7,0x03e2,0x83a3,0x03a6,0x03ac,0x83a9,
               0x03b8,0x83bd,0x83b7,0x03b2,0x0390,0x8395,0x839f,0x039a,0x838b,
               0x038e,0x0384,0x8381,0x0280,0x8285,0x828f,0x028a,0x829b,0x029e,
               0x0294,0x8291,0x82b3,0x02b6,0x02bc,0x82b9,0x02a8,0x82ad,0x82a7,
               0x02a2,0x82e3,0x02e6,0x02ec,0x82e9,0x02f8,0x82fd,0x82f7,0x02f2,
               0x02d0,0x82d5,0x82df,0x02da,0x82cb,0x02ce,0x02c4,0x82c1,0x8243,
               0x0246,0x024c,0x8249,0x0258,0x825d,0x8257,0x0252,0x0270,0x8275,
               0x827f,0x027a,0x826b,0x026e,0x0264,0x8261,0x0220,0x8225,0x822f,
               0x022a,0x823b,0x023e,0x0234,0x8231,0x8213,0x0216,0x021c,0x8219,
               0x0208,0x820d,0x8207,0x0202};

unsigned int crc16(unsigned char *data,int len)
{

    unsigned int acc = 0x00;
    for(int i=0;i<len;i++)
    {
    
        acc = (acc >> 8) ^ crc_table16[(acc ^ data[i]) & 0xff];
    }
    return acc;
}



void AddMessage(int Seq,int Source,unsigned char*Packet,int Length,int TargetMessageLength,int SeqMessage)
{
    static unsigned char Message[512];
    static int IndexMessage=0;
    static int MessageLength=0;
    static int LastSource=0;
    static int MemSeqMessage=-1;
    if(SeqMessage!=-1) MemSeqMessage=SeqMessage;
    if(Source==ACK) 
    {
        //printf("ACK %d: ",Seq);
        for(int j=0;j<Length;j++)
        {    
           
           //printf("%02x",Packet[j]);
           
        }
        //printf("\n");
        return;
    }
    if((Source==POD)||(Source==PDM))
    {
        LastSource=Source;
    }
    
    if(TargetMessageLength!=0)
    {
         MessageLength=TargetMessageLength;
         IndexMessage=0; // To avoid repetition packet   
         
    }
    //printf("\nTargetMessage=%d Length=%d\n",MessageLength,IndexMessage+Length);
   
    for(int i=0;i<Length;i++)
    {
        Message[IndexMessage++]=Packet[i];
    }

    if((Length==0)||(IndexMessage==MessageLength)) 
    {
        
        if(Length==0) printf("Incomplete ");
        //printf("Body Message :");
        //for(int i=6;i<IndexMessage;i++) printf("%02x",Message[i]);
        //printf(" CRC16=%02x%02x/%04x",Message[IndexMessage-2],Message[IndexMessage-1],crc16(&Message[0],MessageLength-2));
        
        unsigned int CRCRead=(Message[IndexMessage-2]*256)+Message[IndexMessage-1];
        unsigned int CRCProcess=crc16(&Message[0],MessageLength-2);
        // printf(" CRC16=%04x/%04x",CRCRead,CRCProcess);
        if(CRCRead==CRCProcess) //CRC OK
          ParseSubMessage(Seq,LastSource,&Message[6],IndexMessage-6-2,MemSeqMessage);  



         IndexMessage=0;
    }
        
}

//***************************************************************************************************
//*********************************** PACKET LAYER ******************************************************
//***************************************************************************************************


/**
 * \file
 * Functions and types for CRC checks.
 *
 * Generated on Thu Oct 12 16:51:00 2017
 * by pycrc v0.9.1, https://pycrc.org
 * using the configuration:
 *  - Width         = 8
 *  - Poly          = 0x07
 *  - XorIn         = 0x00
 *  - ReflectIn     = False
 *  - XorOut        = 0x00
 *  - ReflectOut    = False
 *  - Algorithm     = table-driven
 */

static const unsigned char crc_table[256] = {
    0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d,
    0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65, 0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
    0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5, 0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
    0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85, 0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
    0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2, 0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
    0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2, 0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
    0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32, 0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
    0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42, 0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
    0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c, 0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
    0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec, 0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
    0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c, 0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
    0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c, 0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
    0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b, 0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 0x63,
    0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b, 0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13,
    0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb, 0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83,
    0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb, 0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3
};


unsigned char crc_8(unsigned char crc, const void *data, size_t data_len)
{
    const unsigned char *d = (const unsigned char *)data;
    unsigned int tbl_idx;

    while (data_len--) {
        tbl_idx = crc ^ *d;
        crc = crc_table[tbl_idx] & 0xff;
        d++;
    }
    return crc & 0xff;
};

void ParsePacket(void)
{
   
    
     int static ActualSEQ=-1;   
     /*printf("\nPACKET : ");
    for(int i=0;i<IndexData;i++) printf("%02x",BufferData[i]);
    printf("\n");
    */
    /* ************* WORKAROUND FOR CC TRANSMITION ****************/
    #ifdef RILEY_WORKAROUND
    if(BufferData[IndexData-1]==0xFF) IndexData--;
    if(BufferData[IndexData-2]==0xFF) IndexData-=2;
    #endif
    /* ************* END OF WORKAROUND FOR CC TRANSMITION ****************/
    
    if(IndexData<4) return; 
    if((IndexData<8)&&(IndexData>=4)) 
    {
        printf("\nUnknown packet : ");
        for(int i=0;i<IndexData;i++) printf("%02x",BufferData[i]);
        printf("\n");
        return;
    }

   
    
    //printf("ID1:%x%x%x%x",BufferData[0],BufferData[1],BufferData[2],BufferData[3]);
    //printf(" PTYPE:");
    int PacketType=BufferData[4]>>5;
/*
    switch(PacketType)
    {
        case PDM:printf("PDM");break;
        case POD:printf("POD");break;
        case ACK:printf("ACK");break;
        case CON:printf("CON");break;
        default:printf("UNKOWN");break;         
    }
*/
    int Source=PacketType;
    //printf(" SEQ:%d",BufferData[4]&0x1F);
    int Seq=BufferData[4]&0x1F;

    //printf("New packet type %x with %d length : ",PacketType,IndexData);   for(int i=0;i<IndexData;i++) printf("%02x",BufferData[i]); printf("\n");
    int CRCOK=0;
    int ProcessedPacket=0;
    if(PacketType!=CON)
    {
        //printf(" ID2:%02x%02x%02x%02x",BufferData[5],BufferData[6],BufferData[7],BufferData[8]);
    }
    
    if(((PacketType==PDM)||(PacketType==POD))&&(IndexData>11))     
    {
        //printf(" B9:%02x",BufferData[9]);
        int MessageSeq=(BufferData[9]&0x3C)>>2;
        
        int MessageLen=min((BufferData[10]+2),IndexData-12); //+2 Because CRC16 added ? 
        int ExtraMessageLen=BufferData[10]+((BufferData[9] & 3) << 8); // TO add for long message : FixMe !!!
        
        //printf(" BLEN:%d/%d",(BufferData[10]+2),IndexData-12);

        //printf(" BODY:");
        //for(int i=11;i<(11+MessageLen);i++) printf("%02x",BufferData[i]);
          
        
        //printf(" CRC:%02x/%02x",BufferData[11+MessageLen],crc_8(0x00,BufferData, MessageLen+12-1/*IndexData-1*/));
        CRCOK=(BufferData[11+MessageLen]==crc_8(0x00,BufferData, MessageLen+12-1/*IndexData-1*/));
        if(CRCOK)
        {
            if(ActualSEQ!=Seq)
            {
                if((BufferData[10]+2)==IndexData-12)
                    AddMessage(Seq,Source,&BufferData[5],IndexData-1-5,ExtraMessageLen+6+2,MessageSeq); // To CHECK here !!!!!!!!!!!!!!
                else
                    AddMessage(Seq,Source,&BufferData[5],IndexData-5-1,ExtraMessageLen+6+2,MessageSeq); // To CHECK here !!!!!!!!!!!!!!
            }
        }
        else
        {
            
            printf("BAD CRC\n");
            Seq=ActualSEQ;
        }
        ProcessedPacket=1;
    }
    

    if(PacketType==CON)
    {
        //printf(" BODY:");
        //for(int i=5;i<IndexData-1;i++) printf("%02x",BufferData[i]);
        //printf(" CRC:%02x/%02x",BufferData[IndexData-1],crc_8(0x00,BufferData, IndexData-1));
        //printf("\n");
        int CRCOK=(BufferData[IndexData-1]==crc_8(0x00,BufferData, IndexData-1));        
        if(CRCOK)
        {    
            if(ActualSEQ!=Seq)
            {
                AddMessage(Seq,Source,&BufferData[5],IndexData-5-1,0,-1);
            }
            
        }
        else
        {
            Seq=ActualSEQ;
            //printf("BAD CRC - CON\n");
        }
       ProcessedPacket=1;
    }

    if(PacketType==ACK)
    {
        int CRCOK=(BufferData[IndexData-1]==crc_8(0x00,BufferData, IndexData-1));        
        if(CRCOK)
        {    
            //printf("ACK %d-----------------------------\n",Seq);
            if(ActualSEQ!=Seq)
            {
                AddMessage(Seq,Source,&BufferData[5],IndexData-5-1,0,-1);
            }
            
        }
        else
        {
            Seq=ActualSEQ;
            //printf("BAD CRC - ACK\n");
        }
       ProcessedPacket=1;
        
    }    
    
    if(ProcessedPacket==0) printf("Packet not parsed\n");    
    if(ActualSEQ==-1) 
    {
        ActualSEQ=Seq;
    }
    else
    {
        
        if ((Seq==((ActualSEQ+2)%32))||(Seq==((ActualSEQ+1)%32))||(Seq==ActualSEQ)) // Normallu always +1 / equal id repetition / Could be +2 if Resync after a lost message
        {
               
        }
        else
        {   
            //printf("---------------- MISS ONE PACKET (%d/%d)------------------\n",Seq,ActualSEQ);
        }
         ActualSEQ=Seq;
    }    

       
}

//***************************************************************************************************
//*********************************** BIT LAYER ******************************************************
//***************************************************************************************************

  
void AddData(unsigned char DataValue)
{
   
 
    if((IndexData==0)&&DataValue==0x54) {/*printf("[");*/return;} //Skip SYNC BYTE : 0x54 is No t inverted
    if((IndexData==0)&&DataValue==0xC3) {/*printf("_");*/return;} //Skip 2SYNC BYTE : C3 is not inverted
    if(IndexData<MAXPACKETLENGTH)
        BufferData[IndexData++]=DataValue^0xFF;
    else
        printf("Packet too long !!!\n");
    //printf("\n%x:%x\n",DataValue^0xFF,DataValue);
}

int ManchesterAdd(int BitValue)	
{
    static int prev=-1;
    
    char bitDecoded=-1;
    static int IndexInByte=0;
    static int DecodedByte=0;
  


    if(BitValue==-1) {prev=-1;IndexInByte=0;DecodedByte=0;return -1;} //Reset Manchester  
        //printf("%d",BitValue);
    int FinalByteValue=-1;
    if(prev==-1) { prev=BitValue; return -1;};
    if((prev==0) && (BitValue==1)) {bitDecoded=0;prev=-1;}
    if((prev==1) && (BitValue==0)) {bitDecoded=1;prev=-1;}
    if(prev==BitValue) {/*printf("\n!%d\n",IndexInByte);*/prev=-1;return -2;}
    if(bitDecoded>=0)
    {
        //printf(" M%d ",bitDecoded);
        
        DecodedByte=(DecodedByte<<1)|bitDecoded;
        //DecodedByte=(DecodedByte)bitDecoded<<(7-IndexInByte);
        if(IndexInByte<7)
        {
            IndexInByte++;
            FinalByteValue=-1; //In decoding state 
        }
        else
        {
            IndexInByte=0;
            FinalByteValue=(DecodedByte)&0xFF;
            //printf(".");    
           //printf("->%x\n",FinalByteValue);
            DecodedByte=0;
        }
    }
    else
    {
        IndexInByte=0;
        DecodedByte=0;
        FinalByteValue=-1;
    }
    
    return FinalByteValue;//

}

//***************************************************************************************************
//*********************************** FSK LAYER *****************************************************
//***************************************************************************************************

  

#define FSK_SYNC_RUNNING 0
#define FSK_SYNC_ON 1
#define FSK_SYNC_OFF 2



int GetFSKSync(unsigned char Sym)
{
    static int Index=0;
    static unsigned int Buffer=0;

    int FSKCurrentStatus=FSK_SYNC_RUNNING;
   
    Buffer=((Buffer<<1)&0xFFFE)|Sym;
    //printf("%x\n",Buffer);
    //if(Buffer==0x6665) {/*printf("#");*/FSKCurrentStatus=FSK_SYNC_ON;}
    if(Buffer==0x6665) {/*printf("#");*/Buffer=0;FSKCurrentStatus=FSK_SYNC_ON;}
    //if(Buffer==0xAAAA) {printf("$");FSKCurrentStatus=FSK_SYNC_ON;}
    if((Buffer&0xF)==0xF) FSKCurrentStatus=FSK_SYNC_OFF;        
    
    return(FSKCurrentStatus);
}

//***************************************************************************************************
//*********************************** RF LAYER *****************************************************
//***************************************************************************************************
//RF Process is 
// Get U8 IQ sample at 1.3Msymbols (32*Omnipod symbol rate)
// In order to remove DC spike, tuning of receiver is 325KHz above   
// 1 : 1.3M U8 IQ -> 1.3M Complex float
// 2 : NCO +325Khz
// 3 : Decimation by 4
// 4 : FM Demodulation 
// 5 : Decision with number of transition inside a window of 8 sample
// Todo : Between 2 and 3, make a low pass filtering to avoid replicant signals
// After 3: Make a matched filter (correlator) with conjugate signal of SYNC signal 

    

    
    uint8_t* iq_buffer; // 1Byte I, 1Byte Q
     #define  IQSR 1300000.0
      unsigned int k           =   IQSR/40625;     // filter samples/symbol -> Baudrate
    float complex *buf_rx;
    nco_crcf MyNCO;
    // DECIMATOR AFTER NCO 
    int          type       = LIQUID_RESAMP_DECIM;
    unsigned int num_stages = 2;        // decimate by 2^2=4
    float        fc         =  0.2f;    // signal cut-off frequency
    float        f0         =  0.0f;    // (ignored)
    float        As         = 60.0f;    // stop-band attenuation

    msresamp2_crcf MyDecim;
    fskdem dem;
    freqdem fdem;
void InitRF(void)
{    
    float FSKDeviationHz=26296.0;//26370.0; //Inspectrum show +/-20KHZ ?    
   
    float FreqUp= 325000.0+5000;
    unsigned int m           =   1;     // number of bits/symbol
   
    buf_rx=(float complex*)malloc(k*sizeof(float complex));
   iq_buffer=(uint8_t *)malloc(k*2*sizeof(uint8_t)); // 1Byte I, 1Byte Q
    float        bandwidth   = FSKDeviationHz*2/IQSR;    // frequency spacing : RTLSDR SR shoulde be 256K. Spacing is 26.37KHZ 
    unsigned int nfft        = 1200;    // FFT size for compute spectrum
   
    unsigned int M    = 1 << m;
    // create multi-stage arbitrary resampler object
   MyDecim = msresamp2_crcf_create(type, num_stages, fc, f0, As);
    

    dem = fskdem_create(m,k/4,bandwidth*4.0/2.0); // k/4,bandwidth*4.0/2.0 semble correct % Demod FM
    //fskdem_print(dem);
    MyNCO = nco_crcf_create(LIQUID_NCO);
     nco_crcf_set_phase(MyNCO, 0.0f);
    nco_crcf_set_frequency(MyNCO, 2.0*M_PI*FreqUp/IQSR); // Tuning frequency is SR/4 away : here 256/4=64KHZ : 433923+64=433987
    // modulate, demodulate, count errors
    fdem=freqdem_create(27000.0*2*4/IQSR);
    
      
}

   
int ProcessRF()
{
            
              unsigned int i=0;
              unsigned int j;
            static unsigned int SampleTime=0; 
             int bytes_read=0;
              static  int Lock=-1;
             bytes_read = fread(iq_buffer, 1, k*2, iqfile);
            if((bytes_read>0)&&(DebugIQ!=NULL)) fwrite(iq_buffer,1,bytes_read,DebugIQ);
           if (bytes_read > 0)
           {
                
                //printf("Byte read=%d\n",bytes_read);
                // convert i16 to f32
                for (j=0, i=0; j<bytes_read; j+=2, i++) 
                {
                    float complex r= 
                     (((uint8_t*)iq_buffer)[j] -127.5)/128.0+
                     (((uint8_t*)iq_buffer)[j+1] -127.5)/128.0 * I;
                    float complex rdown;
                    nco_crcf_step(MyNCO);
                    nco_crcf_mix_up(MyNCO, r, &rdown);
                    buf_rx[i]=rdown;
                }
                float complex AfterDecim[k/4];
                float complex buf_rx2[k/4];

                
                unsigned char sym_out ;
                float FmAmplitude[k/4];    
                for(i=0;i<k/4;i++) //Decimation by 4
                {
                    msresamp2_crcf_execute(MyDecim, &buf_rx[i*4], &AfterDecim[i]);  
                        //buf_rx[i]=rdown;
                    SampleTime++;
                    buf_rx2[i]=AfterDecim[i];//afteragc;

                    freqdem_demodulate(fdem, buf_rx2[i], &FmAmplitude[i]);
                    #ifdef DEBUG_FM
                    fwrite(&FmAmplitude[i],sizeof(float),1,DebugFM);
                    #endif
                    //printf("%f \n",FmAmplitude[i]);
                    
                    float re=crealf(buf_rx2[i]);//printf("%f+i)",re);
                    float im=cimagf(buf_rx2[i]);//printf("%f\n",im);
                    //  if(agc_crcf_get_rssi(MyAGC)>-20)
                    
                    {
                        //fwrite(&re,sizeof(float),1,DebugIQ);
                        //fwrite(&im,sizeof(float),1,DebugIQ);
                    }

                }
                static int FMState=0;
                static int SampleFromLastTransition=0;    
                int NbSymbol=0;
                unsigned char Sym[k/4];
                for(i=0;i<k/4;i++) 
                {
                    SampleFromLastTransition++;
                    if((FMState==0)&&(FmAmplitude[i]>=0.4))
                         {Sym[NbSymbol++]=1;FMState=1;SampleFromLastTransition=0;}
                    else
                        if((FMState==1)&&(FmAmplitude[i]<-0.4)) {Sym[NbSymbol++]=0;FMState=0;SampleFromLastTransition=0;}   
                    
                    if(SampleFromLastTransition>(k/4+2)) {Sym[NbSymbol++]=FMState;SampleFromLastTransition=0;} 
                }
                 
                if((NbSymbol>2)||(NbSymbol==0)) return 1;/* else printf("%d",NbSymbol);*/// More than 2 transition is surely noise    
                
                 //sym_out = fskdem_demodulate(dem, buf_rx2);
                //NbSymbol=1;Sym[0]=sym_out;
                for(int i=0;i<NbSymbol;i++)
                {
                    static int FSKSyncStatus=0;
                    
                     //sym_out=(sym_out==0)?1:0;

                    if((FSKSyncStatus==1))
                    {
                        
                        int Manchester=ManchesterAdd(Sym[i]);
                        //
                        if(Manchester>=0)
                        {
                                unsigned char ManchesterByte=Manchester; 
                                fwrite(&ManchesterByte,1,1,ManchesterFile);
                                AddData(Manchester);
                                
                        }
                        else
                        {
                            
                            if(Manchester==-2)
                            {
                                //printf("\n Unlock \n");
                                ParsePacket();
                                
                                FSKSyncStatus=0; // Error in Manchester 
                                IndexData=0;
                                ManchesterAdd(-1);
                                
                                
                            }
                            
                                              
                        }                        
              
                    }
                    else
                        ManchesterAdd(-1);
                                
                    //if(FSKSyncStatus!=1)
                    {
                        int InTimeSync=GetFSKSync(Sym[i]);
                        switch(InTimeSync)
                        {
                            case FSK_SYNC_ON: FSKSyncStatus=1;break;
                            case FSK_SYNC_OFF:FSKSyncStatus=0;break;
                        }
                    }
                }    
              
                

                
                return 1;
                    
          } 
          else
              return 0;               
/*
                
                            printf("Len should be %d : here %d\n",Data[5]>>4,IndexData);
                            if(crc_check_key(LIQUID_CRC_8,Data,Data[5]>>4)) printf("CRC OK\n"); else printf("BAD CRC\n");
*/ 
    
}


//***************************************************************************************************
//*********************************** MAIN PROGRAM **************************************************
//***************************************************************************************************

  


int main(int argc, char*argv[])
{
    // options
    //float FSKDeviationHz=26370.0;
    
   
   

  

    
    if(argc>=2)
    {
            iqfile = fopen (argv[1], "r");
            char ManchesterFileName[255];
            strcpy(ManchesterFileName,argv[1]);
            strcat(ManchesterFileName,".man");
            ManchesterFile=fopen(ManchesterFileName, "wb");
    }    
    else    
            iqfile = fopen ("omniup325.cu8", "r");
    if(argc>=3)
    {
         if(atoi(argv[2])==1)
        {
         DebugIQ = fopen ("debug.cu8", "wb");
         if(DebugIQ==NULL) {printf("Error opeing output file\n");exit(0);}
        }
     
    }
      #ifdef DEBUG_FM
      DebugFM = fopen ("debugfm.cf32", "wb");
      #endif  
    
    //iqfile = fopen ("fifo.cu8", "r");
    if(iqfile==NULL) {printf("Missing input file\n");exit(0);}

   
   
  InitNounce(43080,480653,0,0);
    
      
   InitRF();
          
    while(ProcessRF())
    {
           
    } 
    #ifdef DEBUG_FM
    if(DebugFM!=NULL) fclose(DebugFM);      
    #endif    
    if(DebugIQ!=NULL) fclose(DebugIQ);
    if(iqfile!=NULL) fclose(iqfile);
   
    return 0;
}
