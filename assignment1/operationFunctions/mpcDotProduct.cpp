#include "../headerFiles/mpcOperations.h"

//  in this create AShare triplet ,  vector AShare, vector BShare, and CShare=<AShare,BShare>

// Vector AShare (same size as the vector)
// Scalar BShare
// Vector CShare = element-wise AShare * BShare
std::vector<int> mpcVectorandScalarMul(
    std::vector<int> alpha,
    int BShare,
    int beta,
    std::vector<int> AShare,
    std::vector<int> CShare,
    int modValue = 64)
{

    std::vector<int> ZShare;
    for (size_t i = 0; i < alpha.size(); i++)
    {
        int intmVal = 0, mul1 = 0, mul2 = 0;
        mul1 = ((int64_t)alpha[i] * BShare);
        mul2 = ((int64_t)beta * AShare[i]);
        intmVal = (mul1 - mul2 + CShare[i]) % modValue;
        intmVal = intmVal < 0 ? intmVal + modValue : intmVal;
        ZShare.push_back(intmVal);
    }
    return ZShare;
}
// int mpcVectorandScalarMul(int alpha, int BShare, int beta, int AShare, int CShare, int modValue = 64)
// {
//     int ZShare = ((int64_t)alpha * BShare) - ((int64_t)beta * AShare) + CShare;
//     ZShare = ZShare < 0 ? ZShare + modValue : ZShare;

//     return ZShare;
// }

int mpcDotProduct( // dot product always result to scalar
    std::vector<int> alpha,
    std::vector<int> BShare,
    std::vector<int> beta,
    std::vector<int> AShare,
    int CShare, int modValue = 64)
{
    int mul1 = 0, mul2 = 0, ZShare;
    for (size_t i = 0; i < alpha.size(); i++)
    {
        mul1 = mul1 + ((int64_t)alpha[i] * BShare[i]);
        mul2 = mul2 + ((int64_t)beta[i] * AShare[i]);
    }
    ZShare = mul1 - mul2 + CShare;
    ZShare = ZShare < 0 ? ZShare + modValue : ZShare;
    return ZShare;
}
