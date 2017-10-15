# rtlomni

_Created by Evariste Courjaud F5OEO. Code is GPL_

**rtlomni** is a software to sniff RF packets using a RTLSDR dongle in order to analysis Omnipod protocol.

This work is mainly based on https://github.com/ps2/omnipod_rf

Hope this could help https://github.com/openaps/openomni

SDR demodulation and signal processing is based on excellent https://github.com/jgaeddert/liquid-dsp/

# Installation under Debian based system
```sh
git clone https://github.com/jgaeddert/liquid-dsp/
cd liquid-dsp
./bootstrap.sh     # <- only if you cloned the Git repo
./configure
make
sudo make install
sudo ldconfig

git clone https://github.com/F5OEO/rtlomni
cd rtlomni
make

#Install rtl-sdr driver and utilities (rtl_test, rtl_sdr ...)
sudo apt-get install rtl-sdr

```

# Launching rtlomni
you can launch :
```sh
./rtlomni
```
It outputs messages from a RF sample file included in the folder.

For live message recording, there is a script 
```sh
./recordiq.sh
```


