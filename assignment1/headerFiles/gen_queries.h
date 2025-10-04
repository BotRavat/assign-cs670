#ifndef GEN_QUERIES_H
#define GEN_QUERIES_H

#include <utility>
#include <vector>
#include <cstdint>

struct LatentVector
{
    std::vector<std::vector<int>> lVector;
};

struct LatentVectorShares
{
    std::vector<std::vector<int>> lvShare0, lvShare1;
};
LatentVector generateLatentVector(int sizeOfVector, int modValue, int numOfVectors, int vectorType);
LatentVectorShares generateVectorShares(LatentVector lVector, int modValue);




// shares of standard vector e
struct StandardVectorShares
{
    std::vector<int> eshare0, eshare1;
};

StandardVectorShares sharesOfe(std::vector<int> e, int modValue);

// generate beaver triplet for vectro and matrix multiplication
struct MatrixVectorTriplet
{
    std::vector<std::vector<int>> AM; // nxk matrix
    std::vector<int> BV, CV;           // 1xn vector
                                     // C :  1xk
};

MatrixVectorTriplet generateMTriplet(int n, int k, int modValue);

struct MatrixVectorTripletShare
{
    std::vector<std::vector<int>> AMShare0, AMShare1;
    std::vector<int> BVShare0, BVShare1;
    std::vector<int> CVShare0, CVShare1;
};

MatrixVectorTripletShare genMShare(MatrixVectorTriplet mTriplet, int modValue);





struct VectorTriplet
{
    std::vector<int> vectorA, vectorB;
    int scalarC;
};
VectorTriplet generateVectorTriplet(int sizeOfVector, int modValue);

struct VectorTripletShares
{
    std::vector<int> vectorA0, vectorA1, vectorB0, vectorB1;
    int scalarC0, scalarC1;
};

VectorTripletShares vTripletShares(VectorTriplet vTriplet, int modValue);






struct ScalarandVectorTriplet
{
    std::vector<int> A;
    int bScalar;
    std::vector<int> C;
};

ScalarandVectorTriplet generateScalarandVectorTriplet(int sizeOfVector, int modValue);

struct ScalarandVectorTripletShares
{
    std::vector<int> AShare0, AShare1;
    int b0, b1;
    std::vector<int> CShare0, CShare1;
};

ScalarandVectorTripletShares sAndVectorTripletShares(ScalarandVectorTriplet sTriplet, int modValue);


// shares of 1
std::pair<int, int> sharesOfOne(int modValue);


#endif