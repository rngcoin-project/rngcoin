VERSION='1.0.0'
ARCH='x86_64'

rm -rf release

mkdir release
mkdir release/bin


cp ../../src/rngcoin-tx    		release/bin/
cp ../../src/rngcoin-cli   		release/bin/
cp ../../src/qt/rngcoin-qt 		release/bin/
cp ../../src/rngcoind      		release/bin/
cp ../../src/test/test_rngcoin		release/bin/


strip ./release/bin/rngcoin-tx
strip ./release/bin/rngcoin-cli
strip ./release/bin/rngcoin-qt
strip ./release/bin/rngcoind
strip ./release/bin/test_rngcoin

cd release

# Print output path
tar czf rngcoin-${VERSION}-${ARCH}-linux-gnu.tar.gz bin
output="$(readlink -f rngcoin-${VERSION}-${ARCH}-linux-gnu.tar.gz)"
echo "Output: "$output 
