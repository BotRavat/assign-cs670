#include <utility>
#include "../headerFiles/mpcOperations.h"

using namespace std;

vector<int> mpcVectorandMatrixMul(
    vector<int> alphaV,        // 1xn
    vector<vector<int>> V,     // nxk
    vector<vector<int>> betaM, // nxk
    vector<int> AVShare0,
    vector<int> C, // 1xk
    int modValue)
{
    vector<int> z;

    for (size_t col = 0; col < betaM.size(); col++)
    {
        int mul1 = 0, mul2 = 0;
        for (size_t row = 0; row < alphaV.size(); row++)
        {
            mul1 = (mul1 + (int64_t)(alphaV[row] * V[row][col])) % modValue;
            mul2 = (mul2 + (int64_t)(AVShare0[row] * betaM[row][col])) % modValue;
        }
        int res = (mul1 - mul2 + C[col]) % modValue;
        res = res < 0 ? res + modValue : res;
        z.push_back(res);
    }

    return z;
}

int mpcDotProduct( // dot product always result to scalar
    vector<int> alpha,
    vector<int> BShare,
    vector<int> beta,
    vector<int> AShare,
    int CShare, int modValue = 64)
{
    int mul1 = 0, mul2 = 0, ZShare;
    for (size_t i = 0; i < alpha.size(); i++)
    {
        mul1 = (mul1 + ((int64_t)alpha[i] * BShare[i]) % modValue) % modValue;
        mul2 = (mul2 + ((int64_t)beta[i] * AShare[i]) % modValue) % modValue;
    }
    ZShare = (mul1 - mul2 + CShare) % modValue;
    ZShare = ZShare < 0 ? ZShare + modValue : ZShare;
    return ZShare;
}

// Vector AShare (same size as the vector)
// Scalar BShare
// Vector CShare = element-wise AShare * BShare
vector<int> mpcVectorandScalarMul(
    vector<int> alpha,
    int bShare,
    int beta,
    vector<int> AShare,
    vector<int> CShare,
    int modValue = 64)
{

    vector<int> ZShare;
    for (size_t i = 0; i < alpha.size(); i++)
    {
        int intmVal = 0, mul1 = 0, mul2 = 0;
        mul1 = ((int64_t)alpha[i] * bShare) % modValue;
        mul2 = ((int64_t)beta * AShare[i]) % modValue;
        {
            intmVal = (mul1 - mul2 + CShare[i]) % modValue;

            intmVal = intmVal < 0 ? intmVal + modValue : intmVal;
            ZShare.push_back(intmVal);
        }
    }
    return ZShare;
}