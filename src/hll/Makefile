CXX = g++
#CXX = clang++
CC_FLAGS = -std=c++2a -O3 -D_XOPEN_SOURCE=700 -Wall

UNAME := $(shell uname)
ifeq ($(UNAME),Darwin)
## MacOS:  No librt needed
LD_FLAGS = -Wl,-u,_munmap -m64 -lcapnp -lkj -lcrypto -L/usr/local/opt/openssl/lib
else
## Linux:  Use real-time library for shm* things
LD_FLAGS = -lrt -Wl,-u,_munmap -m64 -lcapnp -lkj -lcrypto
endif

DEMO = demo
HLL = hll

all: $(HLL)

run:
	#./$(DEMO)
	./$(HLL)

$(DEMO): $(DEMO).o $(HLL).o util.o $(HLL).capnp.h
	$(CXX) -o $(DEMO) $(DEMO).o $(HLL).o util.o $(LD_FLAGS) -D_REENTRANT -lpthread -Llibcount libcount/libcount.a
	
$(HLL): $(HLL).capnp.h $(HLL).o util.o
	$(CXX) -o $(HLL) $(HLL).o util.o $(LD_FLAGS) -D_REENTRANT -lpthread -Llibcount libcount/libcount.a

$(DEMO).o: $(DEMO).cpp $(HLL).h $(HLL).capnp.h
	$(CXX) -c $(DEMO).cpp $(CC_FLAGS) -Ilibcount/include

$(HLL).o: libcount/libcount.a $(HLL).cpp $(HLL).h $(HLL).capnp.h
	$(CXX) -c $(HLL).cpp $(CC_FLAGS) -Ilibcount/include

util.o: util.cpp util.h
	$(CXX) -c util.cpp $(CC_FLAGS)

$(HLL).capnp.h:
	capnp compile -oc++ --src-prefix=. ./$(HLL).capnp

$(HLL).capnp.o:
	$(CXX) -c $(HLL).capnp.c++ $(CC_FLAGS)

generate:
	python generate.py 100000 > data.csv

libcount/libcount.a:
	git clone https://github.com/dialtr/libcount.git
	cd libcount; git submodule init && git submodule update --recursive --remote; make

clean:
	rm -f *.capnp.*
	rm -f *.o
	rm -f out.*
	rm -f a.out
	rm -f $(DEMO)
	rm -f $(HLL)
	rm -rf libcount
