
#ifndef GEN_QUERIES_H
#define GEN_QUERIES_H

#include <vector>
#include <cstdint>
using namespace std;

vector<int> mpcVectorandMatrixMul(
    const vector<int>& alphaV,
    const vector<vector<int>>& V0,
    const vector<vector<int>>& betaM,
    const vector<int>& AVShare0,
    const vector<int>& CShare0,
    int modValue);

vector<int> mpcVectorandScalarMul(
    const vector<int>& alpha,
    int bShare,
    int beta,
    const vector<int>& AShare,
    const vector<int>& CShare,
    int modValue);

int mpcDotProduct(
    const vector<int>& alpha,
    const vector<int>& b,
    const vector<int>& beta,
    const vector<int>& a,
    int c,
    int modValue);

#endif













// #ifndef GEN_QUERIES_H
// #define GEN_QUERIES_H

// #include <utility>
// #include <vector>
// #include <cstdint>
// #include "gen_queries.h"


// using namespace std;
// vector<int> mpcVectorandMatrixMul(
//     vector<int> alphaV,
//     vector<vector<int>>V0,
//     vector<vector<int>> betaM,
//     vector<int> AVShare0,
//     vector<int> CShare0,
//     int modValue);


// vector<int> mpcVectorandScalarMul(
//     vector<int> alpha,
//     int bShare,
//     int beta,
//     vector<int> AShare,
//     vector<int> CShare,
//     int modValue);

// int mpcDotProduct(vector<int> alpha,
//                   vector<int> b,
//                   vector<int> beta,
//                   vector<int> a,
//                   int c, int modValue);


// #endif