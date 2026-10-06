CXX = mpicxx
CXXFLAGS = -O2 -std=c++17 -fopenmp -Wall -Wextra -Wpedantic

all: greenhouse

greenhouse: src/greenhouse.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

local: src/greenhouse.cpp
	g++ $(CXXFLAGS) -DGREENHOUSE_LOCAL $< -o greenhouse_local

clean:
	rm -f greenhouse greenhouse_local

.PHONY: all local clean
