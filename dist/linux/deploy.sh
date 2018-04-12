#!/bin/sh

rm -f rngcoin.docker.tar.xz
tar cvzf rngcoin.docker.tar.xz docker
scp ./rngcoin.docker.tar.xz root@${REMOTE}:/root
rm -f rngcoin.docker.tar.xz
