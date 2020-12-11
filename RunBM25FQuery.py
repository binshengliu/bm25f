#!/usr/bin/env python3
import argparse
from pathlib import Path
import lxml.etree as ET
import sys
import os
from concurrent.futures import ProcessPoolExecutor
import subprocess


def bm25f(args):
    proc = subprocess.run(
        args, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return proc.stdout.decode('utf-8')


def bm25f_mp(bin_args, queries, threads):
    processes = len(os.sched_getaffinity(0)) if threads is None else threads
    bin_args_list = [
        bin_args + ['-qno={}'.format(qno), '-query={}'.format(text)]
        for qno, text in queries
    ]
    with ProcessPoolExecutor(max_workers=processes) as executor:
        output = executor.map(bm25f, bin_args_list)

    output = ''.join(output).strip()
    print(output)


def eprint(*args, **kwargs):
    print(*args, **kwargs, file=sys.stderr)


def fullpath(p):
    return Path(p).resolve()


def parse_args():
    parser = argparse.ArgumentParser(description='Run queries distributedly')

    parser.add_argument('-threads', type=int)

    parser.add_argument(
        'param',
        nargs='+',
        type=Path,
        help='Param file, none for reading from stdin',
    )

    args = parser.parse_args()

    return args


def main():
    args = parse_args()

    binpath = str(Path(__file__).with_name('bm25f'))
    wanted = ['index', 'k1', 'count', 'fieldB', 'fieldWt', 'query', 'stemmer']
    bin_args = [binpath]
    queries = []
    for p in args.param:
        root = ET.parse(str(p)).getroot()
        for child in root:
            if child.tag not in wanted:
                continue
            if child.tag == 'query':
                queries.append((child.find('number').text,
                                child.find('text').text))
            elif child.tag == 'stemmer':
                if child.find('name') is not None:
                    bin_args.append('-{}={}'.format(child.tag,
                                                    child.find('name').text))
                else:
                    bin_args.append('-{}={}'.format(child.tag, child.text))
            else:
                bin_args.append('-{}={}'.format(child.tag, child.text))

    bm25f_mp(bin_args, queries, args.threads)


if __name__ == '__main__':
    main()
