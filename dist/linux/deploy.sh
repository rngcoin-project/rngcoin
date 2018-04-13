#!/bin/sh

rm -f rngcoin.docker.tar.xz
cp ./src/rngcoind ./docker
cp ./src/rngcoin-cli ./docker

tar cvzf rngcoin.docker.tar.xz docker
scp ./rngcoin.docker.tar.xz root@${REMOTE}:/root
rm -f rngcoin.docker.tar.xz
