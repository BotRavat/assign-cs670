#include <utility> 
#include "../headerFiles/gen_queries.h"
#include <random>
#include <stdexcept>
#include <limits>

using namespace std;

namespace
{
  mt19937_64 &rngDataVector() // on every call of distributor it will generate next set of values thats why for generate and and generateShare will generate different numbers
  {
    static mt19937_64 genDataVector(12345); // seed 12345 for reproducable result
    return genDataVector;
  }
  mt19937_64 &rngTripletVector()
  {
    static mt19937_64 genTripletVector(54320);
    return genTripletVector;
  }
  mt19937_64 &rngItemVector()
  {
    static mt19937_64 genItemVector(14370);
    return genItemVector;
  }
  mt19937_64 &rngTripletScalar()
  {
    static mt19937_64 genTripletScalar(1470);
    return genTripletScalar;
  }
  mt19937_64 &rngTripletMatrix()
  {
    static mt19937_64 genTripletMatrix(13205);
    return genTripletMatrix;
  }
}

LatentVector generateLatentVector(int sizeOfVector, int modValue, int numOfVectors, int vectorType)
{
  if (sizeOfVector < 0)
    throw invalid_argument("n < 0");
  if (modValue <= 0)
    throw invalid_argument("mod <= 0");

  // uniform_int_distribution<uint64_t> dist;
  uniform_int_distribution<uint64_t> dist(0, modValue - 1); // above statement will also generate but we provided range
  LatentVector lVector;
  int randomNum;

  mt19937_64 *rngPtr = nullptr;
  if (vectorType == 0)
  {
    rngPtr = &rngDataVector();
  }
  else if (vectorType == 1)
  {
    rngPtr = &rngItemVector();
  }
  else
  {
    rngPtr = &rngDataVector();
  }
  // auto &genDataVector = rngDataVector();
  // mt19937_64 genDataVector(12345); // seed 12345 for reproducable result
  for (int j = 0; j < numOfVectors; j++)
  {
    vector<int> generatedVector;
    for (int i = 0; i < sizeOfVector; i++)
    {
      randomNum = dist(*rngPtr); // for now we using <random> library of c++ later move to  CSPRNG or other
      generatedVector.push_back(randomNum);
    }
    lVector.lVector.push_back(generatedVector);
  }
  return lVector;
}

LatentVectorShares generateVectorShares(LatentVector lVector, int modValue)
{
  uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  auto &genDataVector = rngDataVector();
  // mt19937_64 genDataVector(1234567);

  LatentVectorShares lvShares;

  for (const auto &vec : lVector.lVector)
  {
    vector<int> share0, share1;
    for (size_t i = 0; i < vec.size(); i++) // do not use int i=0, will give error/warning
    {
      int s0, s1;
      s0 = dist(genDataVector);
      s1 = (vec[i] - s0) % modValue;
      s1 = s1 < 0 ? s1 + modValue : s1;
      share0.push_back(s0);
      share1.push_back(s1);
    }
    lvShares.lvShare0.push_back(share0);
    lvShares.lvShare1.push_back(share1);
  }

  return lvShares;
}

//   for generating and secret share of Beaver triplet
ScalarandVectorTriplet generateScalarandVectorTriplet(int sizeOfVecor, int modValue)
{
  uniform_int_distribution<uint64_t> dist(0, modValue - 1);

  vector<int> A, C;
  int b;

  auto &genScalar = rngTripletScalar();

  b = dist(genScalar);

  for (int i = 0; i < sizeOfVecor; i++)
  {
    int a = dist(genScalar);
    A.push_back(a);
    int c = ((int64_t)a * b) % modValue;
    C.push_back(c);
  }

  return ScalarandVectorTriplet{A, b, C};
}

ScalarandVectorTripletShares sAndVectorTripletShares(ScalarandVectorTriplet sTriplet, int modValue)
{

  uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  auto &genScalar = rngTripletScalar();

  ScalarandVectorTriplet t = sTriplet;
  vector<int> A0, A1, C0, C1;
  int b0, b1;
  b0 = dist(genScalar);
  b1 = (t.bScalar - b0) % modValue;
  b1 = b1 < 0 ? b1 + modValue : b1;

  for (size_t i = 0; i < t.A.size(); i++)
  {
    int a0 = dist(genScalar);
    A0.push_back(a0);
    int a1 = (t.A[i] - a0) % modValue;
    a1 = a1 < 0 ? a1 + modValue : a1;
    A1.push_back(a1);
    int c0 = dist(genScalar);
    C0.push_back(c0);
    int c1 = (t.C[i] - c0) % modValue;
    c1 = c1 < 0 ? c1 + modValue : c1;
    C1.push_back(c1);
  }

  return ScalarandVectorTripletShares{A0, A1, b0, b1, C0, C1};
}

VectorTriplet generateVectorTriplet(int sizeOfVector, int modValue)
{

  if (sizeOfVector < 0)
    throw invalid_argument("n < 0");
  if (modValue <= 0)
    throw invalid_argument("mod <= 0");

  uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  vector<int> vectorA, vectorB;
  int random1, random2, scalarC = 0;

  auto &genTripletVector = rngTripletVector();
  // mt19937_64 genDataVector(12345); // seed 12345 for reproducable result

  for (int i = 0; i < sizeOfVector; i++)
  {
    random1 = dist(genTripletVector);
    vectorA.push_back(random1);
    random2 = dist(genTripletVector);
    vectorB.push_back(random2);

    int64_t temp = (int64_t)random1 * random2; /// int64_t --> avoid integer overflow
    scalarC = (scalarC + temp) % modValue;
  }
  return VectorTriplet{vectorA, vectorB, scalarC};
}

VectorTripletShares vTripletShares(VectorTriplet vTriplet, int modValue)
{

  uniform_int_distribution<uint64_t> dist(0, modValue - 1);

  VectorTriplet t = vTriplet;
  vector<int> vectorA0, vectorA1, vectorB0, vectorB1;
  int scalarC0, scalarC1;
  int shareA0, shareA1, shareB0, shareB1;

  auto &genTripletVector = rngTripletVector();
  // mt19937_64 genDataVector(1234567);

  // do not use int i=0, will give error/warning
  for (size_t i = 0; i < t.vectorA.size(); i++)
  {
    shareA0 = dist(genTripletVector);
    shareA1 = (t.vectorA[i] - shareA0) % modValue;
    shareA1 = shareA1 < 0 ? shareA1 + modValue : shareA1;
    vectorA0.push_back(shareA0);
    vectorA1.push_back(shareA1);

    shareB0 = dist(genTripletVector);
    shareB1 = (t.vectorB[i] - shareB0) % modValue;
    shareB1 = shareB1 < 0 ? shareB1 + modValue : shareB1;

    vectorB0.push_back(shareB0);
    vectorB1.push_back(shareB1);
  }
  scalarC0 = dist(genTripletVector);
  scalarC1 = (t.scalarC - scalarC0) % modValue;
  scalarC1 = scalarC1 < 0 ? scalarC1 + modValue : scalarC1;

  return VectorTripletShares{
      vectorA0, vectorA1, vectorB0, vectorB1, scalarC0, scalarC1};
}

pair<int, int> sharesOfOne(int modValue)
{
  uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  auto &genTripletVector = rngTripletVector();

  int one0 = dist(genTripletVector);
  int one1 = (1 - one0) % modValue;
  one1 = one1 < 0 ? one1 + modValue : one1;

  return {one0, one1};
}

// standard vector shares
StandardVectorShares sharesOfe(vector<int> e, int modValue)
{

  uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  auto &genTripletMatrix = rngTripletMatrix();
  StandardVectorShares eShares;

  for (size_t i = 0; i < e.size(); i++) // size of e is n ( number of items or row in item matrix)
  {
    int num0, num1;
    num0 = dist(genTripletMatrix);
    eShares.eshare0.push_back(num0);
    num1 = (e[i] - num0) % modValue;
    num1 = num1 < 0 ? num1 + modValue : num1;
    eShares.eshare1.push_back(num1);
  }
  return eShares;
}

// beaver triplet for vector and matrix multiplication

// struct MatrixVectorTriplet
// {
//     vector<vector<int>> BM; // nxk matrix
//     vector<int> AV, CV;     // 1xn vector
//                             // C :  1xk
// };


MatrixVectorTriplet generateMTriplet(int n, int k, int modValue)
{
  uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  auto &genTripletMatrix = rngTripletMatrix();

  vector<vector<int>> BM; // nxk matrix
  vector<int> AV, CV;           // B :[]1xn,  C: []1xk

  for (size_t i = 0; i < n; i++)
  {
    vector<int> tempVec;
    for (size_t j = 0; j < k; j++)
    {
      int randomnum = dist(genTripletMatrix);
      tempVec.push_back(randomnum);
    }
    BM.push_back(tempVec);
    int rand2 = dist(genTripletMatrix);
    AV.push_back(rand2);
  }

  for (size_t j = 0; j < k; j++)
  {
    long long sum = 0;
    for (size_t i = 0; i < n; i++)
    {
      sum = (sum + ((int64_t)AV[i] * BM[i][j]) % modValue) % modValue;
    }
    CV.push_back(sum);
  }

  return MatrixVectorTriplet{BM, AV, CV};
}

// struct MatrixVectorTripletShare
// {
//     vector<vector<int>> BMShare0, BMShare1;
//     vector<int> AVShare0, AVShare1;
//     vector<int> CVShare0, CVShare1;
// };

MatrixVectorTripletShare genMShare(MatrixVectorTriplet mTriplet, int modValue)
{
  uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  auto &genTripletMatrix = rngTripletMatrix();

  MatrixVectorTripletShare shares;
  size_t n = mTriplet.BM.size();
  size_t k = mTriplet.CV.size();

  //shares of BM []nxk
  for (size_t i = 0; i < n; i++)
  {
    vector<int> rowShare0, rowShare1;
    for (size_t j = 0; j < k; j++)
    {
      int rand0 = dist(genTripletMatrix);
      int rand1 = (mTriplet.BM[i][j] - rand0) % modValue;
      rand1 = rand1 < 0 ? rand1 + modValue : rand1;
      rowShare0.push_back(rand0);
      rowShare1.push_back(rand1);
    }
    shares.BMShare0.push_back(rowShare0);
    shares.BMShare1.push_back(rowShare1);
  }

  // shares of AV []1xn
  for (size_t i = 0; i < n; i++)
  {
    int rand0 = dist(genTripletMatrix);
    int rand1 = (mTriplet.AV[i] - rand0) % modValue;
    rand1 = rand1 < 0 ? rand1 + modValue : rand1;
    shares.AVShare0.push_back(rand0);
    shares.AVShare1.push_back(rand1);
  }

  // shares of CV []1xk
  for (size_t i = 0; i < k; i++)
  {
    int rand0 = dist(genTripletMatrix);
    int rand1 = (mTriplet.CV[i] - rand0) % modValue;
    rand1 = rand1 < 0 ? rand1 + modValue : rand1;
    shares.CVShare0.push_back(rand0);
    shares.CVShare1.push_back(rand1);
  }

  return shares;
}