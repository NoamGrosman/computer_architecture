# Compiler
CXX = g++

# Compiler Flags:
CXXFLAGS = -g -Wall -std=c++11

# Target executable name
TARGET = cacheSim

# Object files
OBJS = cacheSim.o

# Default rule to build the target
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Rule to compile the source file into an object file
cacheSim.o: cacheSim.cpp
	$(CXX) $(CXXFLAGS) -c cacheSim.cpp

# Clean up build files
.PHONY: clean
clean:
	rm -f *.o $(TARGET)