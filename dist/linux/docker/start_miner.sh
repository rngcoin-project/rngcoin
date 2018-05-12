#!/bin/sh

/rngcoin/rngcoind -daemon
sleep 10
/rngcoin/minerd -a scrypt -o http://127.0.0.1:20022 -O test:test --no-stratum --coinbase-addr=RLjetEZxPnrm9TeF3rqQbdadFMz9UeCNDF
