FROM ubuntu:16.04

# install whatever is required to run rngcoin
RUN apt-get update && \
    apt-get install -y software-properties-common && \
	apt-add-repository ppa:bitcoin/bitcoin && \
	apt-get update && \
	apt-get install -y bitcoind

RUN mkdir /rngcoin
COPY ./rngcoind ./minerd ./rngcoin-cli ./start_miner.sh ./start_node.sh /rngcoin/
COPY ./rngcoin.conf /root/.rngcoin/

ENTRYPOINT ["sh", "/rngcoin/start_miner.sh"]
