# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -O2 -I./src/headerFiles -I./src/generatorFunctions -I./src
LDFLAGS = -lboost_system -lpthread -lssl -lcrypto

# Source files
GENERATE_SRC = src/generatorFunctions/saveToFile.cpp \
               src/generatorFunctions/gen_queries.cpp \
               src/mpcMultiplication.cpp \
#                src/gen_queries_forMatrix.cpp

DEALER_SRC = src/dealer.cpp \
             src/mpcMultiplication.cpp \
             src/generatorFunctions/gen_queries.cpp

PARTY0_SRC = src/party0.cpp \
             src/mpcMultiplication.cpp \
             src/generatorFunctions/gen_queries.cpp

PARTY1_SRC = src/party1.cpp \
             src/mpcMultiplication.cpp \
             src/generatorFunctions/gen_queries.cpp

# Default values (can be overridden)
USERS ?= 3
ITEMS ?= 4
FEATURES ?= 5
NUM_QUERIES ?= 2
QUERIES ?= 0 2 1 3 

# Build all targets
all: generate dealer party0 party1

# Build generate
generate: $(GENERATE_SRC)
	$(CXX) $(CXXFLAGS) -o generate.out $(GENERATE_SRC) $(LDFLAGS)

# Build dealer
dealer: $(DEALER_SRC)
	$(CXX) $(CXXFLAGS) -o dealer.out $(DEALER_SRC) $(LDFLAGS)

# Build party0
party0: $(PARTY0_SRC)
	$(CXX) $(CXXFLAGS) -o party0.out $(PARTY0_SRC) $(LDFLAGS)

# Build party1
party1: $(PARTY1_SRC)
	$(CXX) $(CXXFLAGS) -o party1.out $(PARTY1_SRC) $(LDFLAGS)

# Run generate with m users, n items, k features
run_generate: generate
	./generate.out $(USERS) $(ITEMS) $(FEATURES)

# Run dealer with queries
run_dealer: dealer
	./dealer.out $(NUM_QUERIES) $(QUERIES)

# Clean
clean:
	rm -f *.out *.o src/*.o *.txt

.PHONY: all generate dealer party0 party1 run_generate run_dealer clean
