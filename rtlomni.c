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
    if(prev==BitValue) {printf("!");prev=-1;return -2;}
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
            printf(".");    
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
   
 
    if((IndexData==0)&&DataValue==0x54) {printf("[");return;} //Skip SYNC BYTE
    if((IndexData==0)&&DataValue==0xC3) {printf("_");return;} //Skip 2SYNC BYTE 
    BufferData[IndexData++]=DataValue;
    //printf("%x",DataValue);
}

void CheckCRC()
{
    printf("\n Raw : ");
    for(int i=0;i<IndexData;i++) printf("%x",BufferData[i]);
    printf("\n");
    
    if(IndexData<12) return;
    printf("ID1:%x%x%x%x",BufferData[0],BufferData[1],BufferData[2],BufferData[3]);
    printf(" PTYPE:");
    switch(BufferData[4]>>5)
    {
        case 5:printf("PDM");break;
        case 7:printf("POD");break;
        case 2:printf("ACK");break;
        case 4:printf("CON");break;
        default:printf("UNKOWN");break;         
    }
    printf(" SEQ:%d",BufferData[4]&0x1F);
    printf(" ID2:%x%x%x%x",BufferData[5],BufferData[6],BufferData[7],BufferData[8]);
    printf(" B9:%x",BufferData[9]);
    int MessageLen=(BufferData[10]>>4)+2;
    printf(" BLEN:%d",MessageLen);

    printf(" BODY:");
    for(int i=11;i<MessageLen-1;i++)
    printf("%x",BufferData[i]);
    
    printf("CRC:%x",BufferData[MessageLen-1]);
    printf("\n");
    printf("Len should be %d : here %d\n",MessageLen,IndexData);
    //crc_append_key(check, data, n);    
    //BufferData[0]=0;
    if(crc_check_key(LIQUID_CRC_8,BufferData,MessageLen-1)) printf("CRC OK\n"); else printf("BAD CRC\n");
    
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
    if(Buffer==0x6665) {printf("#");FSKCurrentStatus=FSK_SYNC_ON;}
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
    printf("k= %d\n",k);  
    float        bandwidth   = FSKDeviationHz*2/IQSR;    // frequency spacing : RTLSDR SR shoulde be 256K. Spacing is 26.37KHZ 
    unsigned int nfft        = 1200;    // FFT size for compute spectrum
    uint8_t iq_buffer[k*2]; // 1Byte I, 1Byte Q
   
    unsigned int i=0;
    unsigned int j;

    unsigned int M    = 1 << m;

     FILE* iqfile=NULL;
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
    fskdem_print(dem);
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
