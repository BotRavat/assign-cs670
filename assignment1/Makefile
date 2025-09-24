CXX = g++
CXXFLAGS = -std=c++17 -Wall -IheaderFiles


# SRC = $(wildcard generatorFunctions/*.cpp operationFunctions/*.cpp)

SRC = main.cpp \
      generatorFunctions/generate_and_Shares.cpp

OBJ = $(SRC:.cpp=.o)

TARGET = main

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
