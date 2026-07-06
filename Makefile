CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -pedantic

.PHONY: all clean test

all: compiler

compiler: src/main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

test: compiler
	./compiler < tests/basic.tc > tests/basic.s
	./compiler < tests/control.tc > tests/control.s
	./compiler < tests/functions.tc > tests/functions.s

clean:
	rm -f compiler tests/*.s
