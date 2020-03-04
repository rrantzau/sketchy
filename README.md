# sketchy

We show two examples of continuous approximate comtop-k and unique computation over data streams based on sketches

The sketches are implemented using [Cap'n Proto](https://capnproto.org).

The code was tested on MacOS and Ubuntu.

## Top-k items - [The Count-Min Sketch (CMS)](https://en.wikipedia.org/wiki/Count%E2%80%93min_sketch)

*What are the top-k most frequent itmes right now, and how do they rankings change over time?*

    cd src/cms
    make
    make run

## Unique items - [The HyperLogLog Sketch (HLL)](https://en.wikipedia.org/wiki/HyperLogLog)

*How many unique items have I seen in my data stream so far?*

We are using the [HLL implementation of Tom Dial](https://github.com/dialtr/libcount.git).  The sketch itself, however, is represented using Cap'n Proto just for fun.

On MacOS, install the SHA library first:

    brew install openssl

Then do:

    cd src/hll
    make
    make run

