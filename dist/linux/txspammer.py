#!/usr/bin/env python3

import os
import time
import argparse
import subprocess

CLI_PATH = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'src/rngcoin-cli')


def exec_cli(args, datadir):
    datadir = os.path.realpath(datadir)
    subprocess.run([CLI_PATH, '-datadir={}'.format(datadir)] + args)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('address',
                                 type=str,
                                 help='Destination address')
    parser.add_argument('--amount',
                                 type=str,
                                 default=0.1,
                                 help='Destination address')
    parser.add_argument('--datadir',
                                 type=str,
                                 default='~/.rngcoin',
                                 help='Datadir path')
    parser.add_argument('--txfee', type=float,
                                 default=0.00001,
                                 help='Transaction fee')
    args = parser.parse_args()

    while True:
        exec_cli(['settxfee', str(args.txfee)], datadir=args.datadir)
        exec_cli(['sendtoaddress', args.address, str(args.amount)], datadir=args.datadir)
        time.sleep(1)
