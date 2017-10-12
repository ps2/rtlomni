all: rtlomni 


CFLAGS	= -Wall -g -O2 -Wno-unused-variable 
LDFLAGS	= -lm -lliquid 


rtlomni: rtlomni.c
		$(CC) $(CFLAGS) -o rtlomni  rtlomni.c $(LDFLAGS) 


