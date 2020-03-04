#CXX = g++
CXX = clang++
CC_FLAGS = -std=c++2a -O3 -D_XOPEN_SOURCE=700 -Wall
#CC_FLAGS = -std=c++17 -O3 -D_XOPEN_SOURCE=700 -Wall
#-Wextra -Wreturn-type -Wunused -Wno-unused-parameter

UNAME := $(shell uname)
ifeq ($(UNAME),Darwin)
## MACOS:  No librt needed
LD_FLAGS = -Wl,-u,_munmap -m64 -lcapnp -lkj
else
## LINUX: Use real-time library for shm* things
LD_FLAGS = -lrt -Wl,-u,_munmap -m64 -lcapnp -lkj
endif

CMS = cms

all: $(CMS)

run:
	./$(CMS)

$(CMS).capnp.h:
	capnp compile -oc++ --src-prefix=. ./$(CMS).capnp

$(CMS).capnp.o:
	$(CXX) -c $(CMS).capnp.c++ $(CC_FLAGS)

$(CMS).o: $(CMS).cc $(CMS).h $(CMS).capnp.h
	$(CXX) -c $(CMS).cc $(CC_FLAGS)

$(CMS): $(CMS).o $(CMS).capnp.h
	$(CXX) -o $(CMS) $(CMS).o $(LD_FLAGS) -D_REENTRANT -lpthread 

generate:
	python generate.py 100000 > data.csv

clean:
	rm -f *.capnp.*
	rm -f *.o
	rm -f out.*
	rm -f a.out
	rm -f $(CMS)
