#!/bin/sh
perf record -g ./bm25f -index=../../../indri-indexes/ClueWeb09B/Clueweb09B_FIELDS/ \
     -stemmer=krovetz -count=10000 -k1=1.2 -fieldB=title:0.4,heading:0.4,body:0.2 \
     -fieldWt=title:8,heading:2,body:1 -qno=188 -query="internet phone service"
perf script | ~/src/FlameGraph/stackcollapse-perf.pl | ~/src/FlameGraph/flamegraph.pl > perf.svg

