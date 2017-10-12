// 
// fskmodem_example.c
//
// This example demostrates the M-ary frequency-shift keying
// (MFSK) modem in liquid. A message signal is modulated and the
// resulting signal is recovered using a demodulator object.
//

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


   unsigned char BufferData[255];
  int IndexData=0;
void AddData(unsigned char DataValue)
{
   
 
    if((IndexData==0)&&DataValue==0x54) {/*printf("[");*/return;} //Skip SYNC BYTE
    if((IndexData==0)&&DataValue==0xC3) {/*printf("_");*/return;} //Skip 2SYNC BYTE 
    BufferData[IndexData++]=DataValue^0xFF;
    //printf("%x",DataValue);
}


inline int min ( int a, int b ) { return a < b ? a : b; }

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


unsigned char crc_update(unsigned char crc, const void *data, size_t data_len)
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

void CheckCRC()
{
   
    enum {ACK=0b010,CON=0b100,PDM=0b101,POD=0b111};    
        
     printf("\nMSG : ");
    for(int i=0;i<IndexData;i++) printf("%x",BufferData[i]);
    printf("\n");
    
    if(IndexData<10) 
    {
        printf("\nUnknown packet : ");
        for(int i=0;i<IndexData;i++) printf("%02x",BufferData[i]);
        return;
    }
    printf("ID1:%x%x%x%x",BufferData[0],BufferData[1],BufferData[2],BufferData[3]);
    printf(" PTYPE:");
    int PacketType=BufferData[4]>>5;
    switch(PacketType)
    {
        case PDM:printf("PDM");break;
        case POD:printf("POD");break;
        case ACK:printf("ACK");break;
        case CON:printf("CON");break;
        default:printf("UNKOWN");break;         
    }
    printf(" SEQ:%d",BufferData[4]&0x1F);
    
    if(PacketType!=CON)
    {
        printf(" ID2:%x%x%x%x",BufferData[5],BufferData[6],BufferData[7],BufferData[8]);
    }
    
    if((PacketType!=CON)&&(PacketType!=ACK)&&IndexData>11)
    {
        printf(" B9:%x",BufferData[9]);
        
        int MessageLen=min((BufferData[10]+2),IndexData-12); //+2 Because CRC16 added ? 
        
        printf(" BLEN:%d/%d",(BufferData[10]),IndexData-12);

        printf(" BODY:");
        for(int i=11;i<(11+MessageLen);i++) printf("%02x",BufferData[i]);
        
        printf(" CRC16=%02x%02x/%04x",BufferData[11+MessageLen-2],BufferData[11+MessageLen-1],crc16(&BufferData[5],MessageLen-2+6));
        printf(" CRC:%x/%x",BufferData[11+MessageLen],crc_update(0x00,BufferData, IndexData-1));
//crc_generate_key(LIQUID_CRC_8,BufferData,IndexData-3));
        //if(crc_check_key(LIQUID_CRC_8,BufferData,IndexData-1)) printf("-CRC OK\n"); else printf("-BAD CRC\n");

        printf("\n");
    }
    else
    {
         printf(" CRC:%x/%x\n",BufferData[IndexData-1],crc_update(0x00,BufferData, IndexData-1));
    }
    if(PacketType==CON)
    {
        printf(" BODY:");
        for(int i=5;i<IndexData-1;i++) printf("%x",BufferData[i]);
        printf("CRC:%x/%x",BufferData[IndexData-1],crc_update(0x00,BufferData, IndexData-1));
        printf("\n");
    }
    //printf("Len should be %d : here %d\n",MessageLen,IndexData);
    
    //if(crc_check_key(LIQUID_CRC_8,BufferData,MessageLen-1)) printf("CRC OK\n"); else printf("BAD CRC\n");
    
}

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
    if(Buffer==0x6665) {/*printf("#");*/FSKCurrentStatus=FSK_SYNC_ON;}
    //if((Buffer&0xF)==0xF) FSKCurrentStatus=FSK_SYNC_OFF;        
    
    return(FSKCurrentStatus);
}

int main(int argc, char*argv[])
{
    // options
    //float FSKDeviationHz=26370.0;
    float FSKDeviationHz=26370.0; //Inspectrum show +/-20KHZ ?    
    float IQSR = 1300000.0;
    float FreqUp= 325000.0+5000;
    unsigned int m           =   1;     // number of bits/symbol
    unsigned int k           =   IQSR/40625;     // filter samples/symbol -> Baudrate
  
    float        bandwidth   = FSKDeviationHz*2/IQSR;    // frequency spacing : RTLSDR SR shoulde be 256K. Spacing is 26.37KHZ 
    unsigned int nfft        = 1200;    // FFT size for compute spectrum
    uint8_t iq_buffer[k*2]; // 1Byte I, 1Byte Q
   
    unsigned int i=0;
    unsigned int j;

    unsigned int M    = 1 << m;

     FILE* iqfile=NULL;
    if(argc==2)
            iqfile = fopen (argv[1], "r");    
    else    
            iqfile = fopen ("omniup325.cu8", "r");
    
    
    //iqfile = fopen ("fifo.cu8", "r");
    if(iqfile==NULL) {printf("Missing input file\n");exit(0);}

    FILE *DebugIQ=NULL;
    DebugIQ = fopen ("debug.cf32", "wb");
    if(DebugIQ==NULL) {printf("Error opeing output file\n");exit(0);}

   
 
      float complex buf_rx[k];

    // DECIMATOR AFTER NCO 
 int          type       = LIQUID_RESAMP_DECIM;
    unsigned int num_stages = 2;        // decimate by 2^2=4
    float        fc         =  0.2f;    // signal cut-off frequency
    float        f0         =  0.0f;    // (ignored)
    float        As         = 60.0f;    // stop-band attenuation
    
    // create multi-stage arbitrary resampler object
    msresamp2_crcf MyDecim = msresamp2_crcf_create(type, num_stages, fc, f0, As);
    

    fskdem dem = fskdem_create(m,k/4,bandwidth*4.0/2.0); // k/4,bandwidth*4.0/2.0 semble correct % Demod FM
    //fskdem_print(dem);
    nco_crcf MyNCO = nco_crcf_create(LIQUID_NCO);
     nco_crcf_set_phase(MyNCO, 0.0f);
    nco_crcf_set_frequency(MyNCO, 2.0*M_PI*FreqUp/IQSR); // Tuning frequency is SR/4 away : here 256/4=64KHZ : 433923+64=433987
    // modulate, demodulate, count errors

    
    unsigned int num_symbol_errors = 0;
    int bytes_read=0;
    int Lock=-1;
    unsigned int SampleTime=0;    
   
          
    while(1)
    {
           buf_rx[i] = 0; // Read fom I/Q file here
           bytes_read = fread(iq_buffer, 1, k*2, iqfile);
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
                 
                for(i=0;i<k/4;i++) //Decimation by 4
                {
                    msresamp2_crcf_execute(MyDecim, &buf_rx[i*4], &AfterDecim[i]);  
                        //buf_rx[i]=rdown;
                    SampleTime++;
                    buf_rx2[i]=AfterDecim[i];//afteragc;
                  
                    float re=crealf(buf_rx2[i]);//printf("%f+i)",re);
                    float im=cimagf(buf_rx2[i]);//printf("%f\n",im);
                    //  if(agc_crcf_get_rssi(MyAGC)>-20)
                    
                    {
                        //fwrite(&re,sizeof(float),1,DebugIQ);
                        //fwrite(&im,sizeof(float),1,DebugIQ);
                    }

                }
                unsigned char sym_out = fskdem_demodulate(dem, buf_rx2);
                
                static int FSKSyncStatus=0;
                
                

                if((FSKSyncStatus==1))
                {
                    
                    int Manchester=ManchesterAdd(sym_out);
                    //
                    if(Manchester>=0)
                    {
                            AddData(Manchester);
                    }
                    else
                    {
                        
                        if(Manchester==-2)
                        {
                            //printf("\n Unlock \n");
                            CheckCRC();
                            FSKSyncStatus=0; // Error in Manchester 
                            IndexData=0;
                            ManchesterAdd(-1);
                        }
                        
                                          
                    }                        
          
                }
                else
                    ManchesterAdd(-1);

                int InTimeSync=GetFSKSync(sym_out);
                switch(InTimeSync)
                {
                    case FSK_SYNC_ON: FSKSyncStatus=1;break;
                    case FSK_SYNC_OFF:FSKSyncStatus=0;break;
                }
                
              
                

                
                
                    
          } 
          else
               break;               
/*
                
                            printf("Len should be %d : here %d\n",Data[5]>>4,IndexData);
                            if(crc_check_key(LIQUID_CRC_8,Data,Data[5]>>4)) printf("CRC OK\n"); else printf("BAD CRC\n");
*/ 
    }
    fclose(DebugIQ);
    fclose(iqfile);
   
    return 0;
}
