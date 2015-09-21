#!/bin/bash

# cd ~/Desktop/in4073TE300/in4073_xufo/x32-tools/lib-x32
cd /home/synchro/Desktop/in4073TE300/in4073_xufo/x32/te32-qr/

if test -f nexys.x32.h; then #change x32.h to the Nexys config
  mv x32.h QR.x32.h
  mv nexys.x32.h x32.h
  echo "switched to the Nexys config"
else #change x32.h to the QR config
  mv x32.h nexys.x32.h
  mv QR.x32.h x32.h
  echo "switched to QR config"
fi
