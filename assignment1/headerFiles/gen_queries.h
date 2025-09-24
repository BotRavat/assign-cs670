#ifndef GEN_QUERIES_H
#define GEN_QUERIES_H

#include <vector>
#include <utility>
#include <cstdint>



// write code for generating beaver triplet of form vector aShare, scalar bShare, vector cShare= element-wise ashare*bShare

struct LatentVector
{
    std::vector<std::vector<int>>lVector;
};

struct LatentVectorShares{
   std::vector<std::vector<int>>lvShare0,lvShare1;
   
};
LatentVector generateLatentVector(int sizeOfVector, int modValue,int numOfVectors,int vectorType);
LatentVectorShares generateVectorShares(LatentVector lVector, int modValue);


struct ScalarTriplet
{
    int a, b, c;
};

ScalarTriplet generateScalarTriplet(int modValue);

struct ScalarTripletShares
{
    int a0, a1, b0, b1, c0, c1;
};

ScalarTripletShares sTripletShares(ScalarTriplet sTriplet, int modValue);




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


// shares of 1
std::pair<int,int>sharesOfOne(int modValue);

#endif