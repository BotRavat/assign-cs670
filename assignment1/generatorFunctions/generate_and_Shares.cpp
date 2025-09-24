#include "gen_queries.h"
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
ScalarTriplet generateScalarTriplet(int modValue)
{
  std::uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  int a, b, c;

  auto &genScalar = rngTripletScalar();
  a = dist(genScalar);
  b = dist(genScalar);
  c = ((int64_t)a * b) % modValue;

  return ScalarTriplet{a, b, c};
}

ScalarTripletShares sTripletShares(ScalarTriplet sTriplet, int modValue)
{

  std::uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  auto &genScalar = rngTripletScalar();

  ScalarTriplet t = sTriplet;
  int a0, a1, b0, b1, c0, c1;

  a0 = dist(genScalar);
  a1 = (t.a - a0) % modValue;
  a1 = a1 < 0 ? a1 + modValue : a1;

  b0 = dist(genScalar);
  b1 = (t.b - b0) % modValue;
  b1 = b1 < 0 ? b1 + modValue : b1;

  c0 = dist(genScalar);
  c1 = (t.c - c0) % modValue;
  c1 = c1 < 0 ? c1 + modValue : c1;

  return ScalarTripletShares{a0, a1, b0, b1, c0, c1};
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

std::pair<int, int> sharesOfOne(int modValue)
{
  std::uniform_int_distribution<uint64_t> dist(0, modValue - 1);
  auto &genTripletVector = rngTripletVector();

  int one0 = dist(genTripletVector);
  int one1 = (1 - one0) % modValue;
  one1 = one1 < 0 ? one1 + modValue : one1;

  return {one0, one1};
}


/*
int main()
{

  vector<int> dataVector;
  vector<pair<int, int>> sharedVector;
  int sizeOfVector = 5, modValue = 64;

  dataVector = generateLatentVector(sizeOfVector, modValue);

  for (int i = 0; i < 5; i++)
    cout << dataVector[i] << " ";
  cout << "\n";

  sharedVector = generateVectorShares(dataVector, modValue);
  for (auto &p : sharedVector)
  {
    cout << "(" << p.first << ", " << p.second << ")\n";
  }

  return 0;
}*/