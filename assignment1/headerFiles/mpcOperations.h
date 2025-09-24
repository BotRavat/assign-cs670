#ifndef GEN_QUERIES_H
#define GEN_QUERIES_H

#include <vector>
#include <utility>
#include <cstdint>
#include "gen_queries.h"

// mpcDotProduct, mpcScalarMultiplication

// int mpcVectorandScalarMul(int alpha, int b, int beta, int a, int c,int modValue);

std::vector<int> mpcVectorandScalarMul(
    std::vector<int> alpha,
    int BShare,
    int beta,
    std::vector<int> AShare,
    std::vector<int> CShare,
    int modValue );

int mpcDotProduct(std::vector<int> alpha, std::vector<int> b, std::vector<int> beta, std::vector<int> a, int c,int modValue);

#endif