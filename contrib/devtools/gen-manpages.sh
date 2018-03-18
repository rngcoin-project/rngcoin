#!/bin/bash

TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
SRCDIR=${SRCDIR:-$TOPDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

RNGCOIND=${RNGCOIND:-$SRCDIR/rngcoind}
RNGCOINCLI=${RNGCOINCLI:-$SRCDIR/rngcoin-cli}
RNGCOINTX=${RNGCOINTX:-$SRCDIR/rngcoin-tx}
RNGCOINQT=${RNGCOINQT:-$SRCDIR/qt/rngcoin-qt}

[ ! -x $RNGCOIND ] && echo "$RNGCOIND not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
RNGVER=($($RNGCOINCLI --version | head -n1 | awk -F'[ -]' '{ print $6, $7 }'))

# Create a footer file with copyright content.
# This gets autodetected fine for bitcoind if --version-string is not set,
# but has different outcomes for bitcoin-qt and bitcoin-cli.
echo "[COPYRIGHT]" > footer.h2m
$RNGCOIND --version | sed -n '1!p' >> footer.h2m

for cmd in $RNGCOIND $RNGCOINCLI $RNGCOINTX $RNGCOINQT; do
  cmdname="${cmd##*/}"
  help2man -N --version-string=${RNGVER[0]} --include=footer.h2m -o ${MANDIR}/${cmdname}.1 ${cmd}
  sed -i "s/\\\-${RNGVER[1]}//g" ${MANDIR}/${cmdname}.1
done

rm -f footer.h2m