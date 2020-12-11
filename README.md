# BM25 with Fields

This is a bm25f implementation for Indri.

## Installation

1. Clone the code at the root of Indri code.

```
cd /path/to/indri/
git clone https://github.com/binshengliu/bm25f.git
```

2. Build

```
cd bm25f
make
```

## Usage

```
$ ./bm25f -help
Built with Indri release 5.11
bm25f usage: 
  bm25f -index=myindex -qno=1 -query=myquery -count=1000 -k1=10 -fieldB=title:8,body:2 -fieldWt=title:6,body:1
```

## Original Paper

```
Zaragoza, H. and Craswell, N. and Taylor, M. and Saria, S. and Robertson, S. 2004 Microsoft Cambridge at TREC 13: Web and HARD Tracks
```
