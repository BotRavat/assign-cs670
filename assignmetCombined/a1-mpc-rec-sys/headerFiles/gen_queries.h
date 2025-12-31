#ifndef GEN_QUERIES_H
#define GEN_QUERIES_H

#include <utility>
#include <vector>
#include <cstdint>

using namespace std;

struct LatentVector
{
    vector<vector<int>> lVector;
};

struct LatentVectorShares
{
    vector<vector<int>> lvShare0, lvShare1;
};
LatentVector generateLatentVector(int sizeOfVector, int modValue, int numOfVectors, int vectorType);
LatentVectorShares generateVectorShares(LatentVector lVector, int modValue);

// shares of standard vector e
struct StandardVectorShares
{
    vector<int> eshare0, eshare1;
};

StandardVectorShares sharesOfe(vector<int> e, int modValue);

// generate beaver triplet for vectro and matrix multiplication
struct MatrixVectorTriplet
{
    vector<vector<int>> BM; // nxk matrix
    vector<int> AV, CV;     // 1xn vector
                            // C :  1xk
};

MatrixVectorTriplet generateMTriplet(int n, int k, int modValue);

struct MatrixVectorTripletShare
{
    vector<vector<int>> BMShare0, BMShare1;
    vector<int> AVShare0, AVShare1;
    vector<int> CVShare0, CVShare1;
};

MatrixVectorTripletShare genMShare(MatrixVectorTriplet mTriplet, int modValue);

struct VectorTriplet
{
    vector<int> vectorA, vectorB;
    int scalarC;
};
VectorTriplet generateVectorTriplet(int sizeOfVector, int modValue);

struct VectorTripletShares
{
    vector<int> vectorA0, vectorA1, vectorB0, vectorB1;
    int scalarC0, scalarC1;
};

VectorTripletShares vTripletShares(VectorTriplet vTriplet, int modValue);

struct ScalarandVectorTriplet
{
    vector<int> A;
    int bScalar;
    vector<int> C;
};

ScalarandVectorTriplet generateScalarandVectorTriplet(int sizeOfVector, int modValue);

struct ScalarandVectorTripletShares
{
    vector<int> AShare0, AShare1;
    int b0, b1;
    vector<int> CShare0, CShare1;
};

ScalarandVectorTripletShares sAndVectorTripletShares(ScalarandVectorTriplet sTriplet, int modValue);

// shares of 1
pair<int, int> sharesOfOne(int modValue);

#endif