# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -O2 -I./headerFiles -I./generatorFunctions -I./operationFunctions -I./parties -I./dealer
LDFLAGS = -lboost_system -lpthread

# Source files
GENERATE_SRC = generatorFunctions/saveToFile.cpp generatorFunctions/gen_queries.cpp operationFunctions/mpcDotProduct.cpp
DEALER_SRC = dealer/dealer.cpp operationFunctions/mpcDotProduct.cpp generatorFunctions/gen_queries.cpp
PARTY0_SRC = parties/party0.cpp operationFunctions/mpcDotProduct.cpp generatorFunctions/gen_queries.cpp
PARTY1_SRC = parties/party1.cpp operationFunctions/mpcDotProduct.cpp generatorFunctions/gen_queries.cpp

# Default values (can be overridden)
USERS ?= 3
ITEMS ?= 4
FEATURES ?= 5
NUM_QUERIES ?= 2
QUERIES ?= 0 2 1 3 

# Build all
all: generate dealer party0 party1

# Compile generate
generate: $(GENERATE_SRC)
	$(CXX) $(CXXFLAGS) -o generate.out $(GENERATE_SRC) $(LDFLAGS)

# Compile dealer
dealer: $(DEALER_SRC)
	$(CXX) $(CXXFLAGS) -o dealer.out $(DEALER_SRC) $(LDFLAGS)

# Compile party0
party0: $(PARTY0_SRC)
	$(CXX) $(CXXFLAGS) -o party0.out $(PARTY0_SRC) $(LDFLAGS)

# Compile party1
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
	rm -f *.out *.o *.txt

.PHONY: all generate dealer party0 party1 run_generate run_dealer clean
