CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -pedantic

.PHONY: all clean test check

all: main

main: src/main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

test: main
	./main < tests/basic.tc > tests/basic.s
	./main < tests/control.tc > tests/control.s
	./main < tests/functions.tc > tests/functions.s

check: test
	python3 tools/check_codegen.py

clean:
	rm -f main compiler tests/*.s
