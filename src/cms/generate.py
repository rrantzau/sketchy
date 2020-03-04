#!/usr/bin/env python

from numpy.random import zipf, seed, randint, normal
import sys
import time

magic_zipf = 1.2
MAX = pow(2, 31)

rows = int(sys.argv[1])


def z(): return zipf(magic_zipf)  # Zipf distribution


def u(): return randint(0, int(rows))  # Uniform distribution


def n(): return abs(int(normal(0, int(rows/500) + 50)))  # Normal distribution


seed(int(time.time()))
c = 0  # Row counter
for row in xrange(rows):
    c += 1
    z1 = z()
    u1 = u()
    n1 = n()
    if z1 < MAX and u1 < MAX and n1 < MAX:
        #print "%s" % (z())
        print "%s %s %s %s" % (c, z1, u1, n1)
        # print "%s,%s,%s,%s" % (row, z(),i(),n())
