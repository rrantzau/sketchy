# sketchy

Here are two examples of continuous approximate computations over data streams based on sketches:

* top-k items
* unique items

The sketches are implemented in C++ using [Cap'n Proto](https://capnproto.org).

The code was tested on MacOS and Ubuntu.

## Prerequisites

Install Cap'n Proto on your platform according to [these instructions](https://capnproto.org/install.html).  I usually build it from source.

## Top-k items - [The Count-min sketch (CMS)](https://en.wikipedia.org/wiki/Count%E2%80%93min_sketch)

*What are the top-k most frequent itmes right now, and how do they rankings change over time?*

    cd src/cms
    make
    make run

## Unique items - [The HyperLogLog sketch (HLL)](https://en.wikipedia.org/wiki/HyperLogLog)

*How many unique items have I seen in my data stream so far?*

We are using the [HLL implementation of Tom Dial](https://github.com/dialtr/libcount.git).  The sketch itself, however, is represented using Cap'n Proto just for fun.

On MacOS, install the SLL library to be able to use SHA hash functions:

    brew install openssl

Then do:

    cd src/hll
    make
    make run
