CXX ?= c++
CXXFLAGS ?= -O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic

.PHONY: all clean

all: run

run: main.cpp graph.cpp graph.h graph_io.cpp graph_io.h cl_options.cpp cl_options.h string_converter.h
	$(CXX) $(CXXFLAGS) main.cpp graph.cpp graph_io.cpp cl_options.cpp -o run

clean:
	rm -f run
