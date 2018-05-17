#!/bin/sh
cd ../../depends
make HOST=x86_64-w64-mingw32
cd ..
./autogen.sh
cd ./dist/windows
CONFIG_SITE=$PWD/../../depends/x86_64-w64-mingw32/share/config.site ../../configure --prefix=/

