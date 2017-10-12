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

// http://www.ece.cmu.edu/~koopman/roses/dsn04/koopman04_crc_poly_embedded.pdf
//
// This implementation is reflected, processing the least-significant bit of the
// input first, has an initial CRC register value of 0xff, and exclusive-or's
// the final register value with 0xff. As a result the CRC of an empty string,
// and therefore the initial CRC value, is zero.
//
// The standard description of this CRC is:
// width=8 poly=0x4d init=0xff refin=true refout=true xorout=0xff check=0xd8
// name="CRC-8/KOOP"

static unsigned char const crc8_table[] = {
    0xea, 0xd4, 0x96, 0xa8, 0x12, 0x2c, 0x6e, 0x50, 0x7f, 0x41, 0x03, 0x3d,
    0x87, 0xb9, 0xfb, 0xc5, 0xa5, 0x9b, 0xd9, 0xe7, 0x5d, 0x63, 0x21, 0x1f,
    0x30, 0x0e, 0x4c, 0x72, 0xc8, 0xf6, 0xb4, 0x8a, 0x74, 0x4a, 0x08, 0x36,
    0x8c, 0xb2, 0xf0, 0xce, 0xe1, 0xdf, 0x9d, 0xa3, 0x19, 0x27, 0x65, 0x5b,
    0x3b, 0x05, 0x47, 0x79, 0xc3, 0xfd, 0xbf, 0x81, 0xae, 0x90, 0xd2, 0xec,
    0x56, 0x68, 0x2a, 0x14, 0xb3, 0x8d, 0xcf, 0xf1, 0x4b, 0x75, 0x37, 0x09,
    0x26, 0x18, 0x5a, 0x64, 0xde, 0xe0, 0xa2, 0x9c, 0xfc, 0xc2, 0x80, 0xbe,
    0x04, 0x3a, 0x78, 0x46, 0x69, 0x57, 0x15, 0x2b, 0x91, 0xaf, 0xed, 0xd3,
    0x2d, 0x13, 0x51, 0x6f, 0xd5, 0xeb, 0xa9, 0x97, 0xb8, 0x86, 0xc4, 0xfa,
    0x40, 0x7e, 0x3c, 0x02, 0x62, 0x5c, 0x1e, 0x20, 0x9a, 0xa4, 0xe6, 0xd8,
    0xf7, 0xc9, 0x8b, 0xb5, 0x0f, 0x31, 0x73, 0x4d, 0x58, 0x66, 0x24, 0x1a,
    0xa0, 0x9e, 0xdc, 0xe2, 0xcd, 0xf3, 0xb1, 0x8f, 0x35, 0x0b, 0x49, 0x77,
    0x17, 0x29, 0x6b, 0x55, 0xef, 0xd1, 0x93, 0xad, 0x82, 0xbc, 0xfe, 0xc0,
    0x7a, 0x44, 0x06, 0x38, 0xc6, 0xf8, 0xba, 0x84, 0x3e, 0x00, 0x42, 0x7c,
    0x53, 0x6d, 0x2f, 0x11, 0xab, 0x95, 0xd7, 0xe9, 0x89, 0xb7, 0xf5, 0xcb,
    0x71, 0x4f, 0x0d, 0x33, 0x1c, 0x22, 0x60, 0x5e, 0xe4, 0xda, 0x98, 0xa6,
    0x01, 0x3f, 0x7d, 0x43, 0xf9, 0xc7, 0x85, 0xbb, 0x94, 0xaa, 0xe8, 0xd6,
    0x6c, 0x52, 0x10, 0x2e, 0x4e, 0x70, 0x32, 0x0c, 0xb6, 0x88, 0xca, 0xf4,
    0xdb, 0xe5, 0xa7, 0x99, 0x23, 0x1d, 0x5f, 0x61, 0x9f, 0xa1, 0xe3, 0xdd,
    0x67, 0x59, 0x1b, 0x25, 0x0a, 0x34, 0x76, 0x48, 0xf2, 0xcc, 0x8e, 0xb0,
    0xd0, 0xee, 0xac, 0x92, 0x28, 0x16, 0x54, 0x6a, 0x45, 0x7b, 0x39, 0x07,
    0xbd, 0x83, 0xc1, 0xff};


// Return the CRC-8 of data[0..len-1] applied to the seed crc. This permits the
// calculation of a CRC a chunk at a time, using the previously returned value
// for the next seed. If data is NULL, then return the initial seed. See the
// test code for an example of the proper usage.
unsigned crc8(unsigned crc, unsigned char const *data, size_t len)
{
    if (data == NULL)
        return 0;
    crc &= 0xff;
    unsigned char const *end = data + len;
    while (data < end)
        crc = crc8_table[crc ^ ((*data++))];
    return crc;
}

void CheckCRC()
{
   
    enum {ACK=0b010,CON=0b100,PDM=0b101,POD=0b111};    
    /*    
     printf("\n Raw : ");
    for(int i=0;i<IndexData;i++) printf("%x",BufferData[i]);
    printf("\n");
    */
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
    
         unsigned int crc = crc8(0, NULL, 0);
        printf(" CRC:%x/%x/%x",BufferData[11+MessageLen-1],crc8(crc, BufferData, 11+MessageLen-1),crc_generate_key(LIQUID_CRC_8,BufferData,11+MessageLen-1));
//crc_generate_key(LIQUID_CRC_8,BufferData,IndexData-3));
        //if(crc_check_key(LIQUID_CRC_8,BufferData,IndexData-1)) printf("-CRC OK\n"); else printf("-BAD CRC\n");

        printf("\n");
    }
    else
    {
         printf(" CRC:%x\n",BufferData[9]);
    }
    if(PacketType==CON)
    {
        printf(" BODY:");
        for(int i=5;i<IndexData-1;i++) printf("%x",BufferData[i]);
        printf("CRC:%x",BufferData[IndexData-2]);
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
