#include <utility> 
#include "../headerFiles/gen_queries.h"
#include <random>
#include <stdexcept>
#include <limits>

namespace
{
  std::mt19937_64 &rngDataVector() // on every call of distributor it will generate next set of values thats why for generate and and generateShare will generate different numbers
  {
    static std::mt19937_64 genDataVector(12345); // seed 12345 for reproducable result
    return genDataVector;
  }
  std::mt19937_64 &rngTripletVector()
  {
    static std::mt19937_64 genTripletVector(54320);
    return genTripletVector;
  }
  std::mt19937_64 &rngItemVector()
  {
    static std::mt19937_64 genItemVector(14370);
    return genItemVector;
  }
  std::mt19937_64 &rngTripletScalar()
  {
    static std::mt19937_64 genTripletScalar(1470);
    return genTripletScalar;
  }
  std::mt19937_64 &rngTripletMatrix()
  {
    static std::mt19937_64 genTripletMatrix(13205);
    return genTripletMatrix;
  }
}

LatentVector generateLatentVector(int sizeOfVector, int modValue, int numOfVectors, int vectorType)
{
  if (sizeOfVector < 0)
    throw std::invalid_argument("n < 0");
  if (modValue <= 0)
    throw std::invalid_argument("mod <= 0");

  // std::uniform_int_distribution<uint64_t> dist;
  std::uniform_int_distribution<uint64_t> dist(0, modValue - 1); // above statement will also generate but we provided range
  LatentVector lVector;
  int randomNum;

  std::mt19937_64 *rngPtr = nullptr;
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
  // std::mt19937_64 genDataVector(12345); // seed 12345 for reproducable result
  for (int j = 0; j < numOfVectors; j++)
  {
    std::vector<int> generatedVector;
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
  std::uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  auto &genDataVector = rngDataVector();
  // std::mt19937_64 genDataVector(1234567);

  LatentVectorShares lvShares;

  for (const auto &vec : lVector.lVector)
  {
    std::vector<int> share0, share1;
    for (std::size_t i = 0; i < vec.size(); i++) // do not use int i=0, will give error/warning
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
  std::uniform_int_distribution<uint64_t> dist(0, modValue - 1);

  std::vector<int> A, C;
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

  std::uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  auto &genScalar = rngTripletScalar();

  ScalarandVectorTriplet t = sTriplet;
  std::vector<int> A0, A1, C0, C1;
  int b0, b1;
  b0 = dist(genScalar);
  b1 = (t.bScalar - b0) % modValue;
  b1 = b1 < 0 ? b1 + modValue : b1;

  for (std::size_t i = 0; i < t.A.size(); i++)
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
    throw std::invalid_argument("n < 0");
  if (modValue <= 0)
    throw std::invalid_argument("mod <= 0");

  std::uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  std::vector<int> vectorA, vectorB;
  int random1, random2, scalarC = 0;

  auto &genTripletVector = rngTripletVector();
  // std::mt19937_64 genDataVector(12345); // seed 12345 for reproducable result

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

  std::uniform_int_distribution<uint64_t> dist(0, modValue - 1);

  VectorTriplet t = vTriplet;
  std::vector<int> vectorA0, vectorA1, vectorB0, vectorB1;
  int scalarC0, scalarC1;
  int shareA0, shareA1, shareB0, shareB1;

  auto &genTripletVector = rngTripletVector();
  // std::mt19937_64 genDataVector(1234567);

  // do not use int i=0, will give error/warning
  for (std::size_t i = 0; i < t.vectorA.size(); i++)
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

std::pair<int, int> sharesOfOne(int modValue)
{
  std::uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  auto &genTripletVector = rngTripletVector();

  int one0 = dist(genTripletVector);
  int one1 = (1 - one0) % modValue;
  one1 = one1 < 0 ? one1 + modValue : one1;

  return {one0, one1};
}

// standard vector shares
StandardVectorShares sharesOfe(std::vector<int> e, int modValue)
{

  std::uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  auto &genTripletMatrix = rngTripletMatrix();
  StandardVectorShares eShares;

  for (std::size_t i = 0; i < e.size(); i++) // size of e is n ( number of items or row in item matrix)
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

// beaver triplet for vector and matrxi multiplication

// struct MatrixVectorTriplet{
//     std::vector<std::vector<int>>A; //  nxk
//     std::vector<int>B;  // 1xn
//     std::vector<int>C;   //1xk
// };

MatrixVectorTriplet generateMTriplet(int n, int k, int modValue)
{
  std::uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  auto &genTripletMatrix = rngTripletMatrix();

  std::vector<std::vector<int>> A; // nxk matrix
  std::vector<int> B, C;           // B :[]1xn,  C: []1xk

  for (std::size_t i = 0; i < n; i++)
  {
    std::vector<int> tempVec;
    for (std::size_t j = 0; j < k; j++)
    {
      int randomnum = dist(genTripletMatrix);
      tempVec.push_back(randomnum);
    }
    A.push_back(tempVec);
    int rand2 = dist(genTripletMatrix);
    B.push_back(rand2);
  }

  for (std::size_t j = 0; j < k; j++)
  {
    long long sum = 0;
    for (std::size_t i = 0; i < n; i++)
    {
      sum = (sum + ((int64_t)B[i] * A[i][j]) % modValue) % modValue;
    }
    C.push_back(sum);
  }

  return MatrixVectorTriplet{A, B, C};
}

// struct MatrixVectorTripletShare{
//     std::vector<std::vector<int>>AShare0,AShare1;
//     std::vector<int>B0,B1;
//     std::vector<int>C0,C1;
// };

MatrixVectorTripletShare genMShare(MatrixVectorTriplet mTriplet, int modValue)
{
  std::uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  auto &genTripletMatrix = rngTripletMatrix();

  MatrixVectorTripletShare shares;
  std::size_t n = mTriplet.B.size();
  std::size_t k = mTriplet.C.size();

  for (std::size_t i = 0; i < n; i++)
  {
    std::vector<int> rowShare0, rowShare1;
    for (std::size_t j = 0; j < k; j++)
    {
      int rand0 = dist(genTripletMatrix);
      int rand1 = (mTriplet.A[i][j] - rand0) % modValue;
      rand1 = rand1 < 0 ? rand1 + modValue : rand1;
      rowShare0.push_back(rand0);
      rowShare1.push_back(rand1);
    }
    shares.AMShare0.push_back(rowShare0);
    shares.AMShare1.push_back(rowShare1);
  }

  // shares of B []1xn

  for (std::size_t i = 0; i < n; i++)
  {
    int rand0 = dist(genTripletMatrix);
    int rand1 = (mTriplet.B[i] - rand0) % modValue;
    rand1 = rand1 < 0 ? rand1 + modValue : rand1;
    shares.BB0.push_back(rand0);
    shares.B1.push_back(rand1);
  }

  // shares of C []1xk
  for (std::size_t i = 0; i < k; i++)
  {
    int rand0 = dist(genTripletMatrix);
    int rand1 = (mTriplet.C[i] - rand0) % modValue;
    rand1 = rand1 < 0 ? rand1 + modValue : rand1;
    shares.C0.push_back(rand0);
    shares.C1.push_back(rand1);
  }

  return shares;
}