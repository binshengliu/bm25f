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

## Parameter Tuning

The sweeping script requires [hydra](https://hydra.cc/) and [ax](https://ax.dev/).

```
pip install hydra hydra-ax-sweeper
```

### Incremental Tuning

Update `index_path`, `query_path`, `qrels_path`, etc. of [basic.yaml](conf/basic.yaml).

The following algorithm is from the original paper.

1. Tune b values independently

```
python sweep.py --config-name basic -m k1=4.5,5,5.5,6.0,6.5 title_wt=1 url_wt=0 text_wt=0 title_b=0.7,0.8,0.9 url_b=1 text_b=1
python sweep.py --config-name basic -m k1=4.5,5,5.5,6.0,6.5 title_wt=0 url_wt=1 text_wt=0 title_b=1 url_b=0.7,0.8,0.9 text_b=1
python sweep.py --config-name basic -m k1=4.5,5,5.5,6.0,6.5 title_wt=0 url_wt=0 text_wt=1 title_b=1 url_b=1 text_b=0.7,0.8,0.9
```

2. Tune k1 (suppose b values are 0.8, 0.8, and 0.9 from last step)

```
python sweep.py --config-name basic -m k1=4.5,5,5.5,6.0,6.5 title_wt=1 url_wt=1 text_wt=1 title_b=0.8 url_b=0.8 text_b=0.9
```

3. Tune weight (suppose k1 is 5)

```
python sweep.py --config-name basic -m k1=5 'title_wt=range(1, 20)' 'url_wt=range(1, 20)' text_wt=1 title_b=0.8 url_b=0.8 text_b=0.9
```

### Bayesian Optimization

Check how it works [here](https://ax.dev/docs/bayesopt.html).

Update `index_path`, `query_path`, `qrels_path`, etc. of [ax.yaml](conf/ax.yaml).

```
python sweep.py --config-name ax -m 'k1=interval(0, 10)' 'title_wt=interval(1, 20)' 'url_wt=interval(1, 20)' text_wt=1 'title_b=interval(0.0, 1.0)' 'url_b=interval(0.0, 1.0)' 'text_b=interval(0.0, 1.0)'
```

## Original Paper

```
Zaragoza, H. and Craswell, N. and Taylor, M. and Saria, S. and Robertson, S. 2004 Microsoft Cambridge at TREC 13: Web and HARD Tracks
```
