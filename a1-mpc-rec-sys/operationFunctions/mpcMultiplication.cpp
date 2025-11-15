// mpcOperations.cpp — corrected implementations

#include <utility>
#include "../headerFiles/mpcOperations.h"

using namespace std;

// vector<int> mpcVectorandMatrixMul(
//     const vector<int>& alphaV,        // 1 x n
//     const vector<vector<int>>& V,     // n x k
//     const vector<vector<int>>& betaM, // n x k
//     const vector<int>& AVShare0,      // length n
//     const vector<int>& C,             // length k
//     int modValue)
// {
//     int n = (int)alphaV.size();
//     int k = V.empty() ? 0 : (int)V[0].size();
//     vector<int> z;
//     z.resize(k);

//     for (int col = 0; col < k; ++col)
//     {
//         long long mul1 = 0;
//         long long mul2 = 0;
//         for (int row = 0; row < n; ++row)
//         {
//             mul1 = (mul1 + (long long)alphaV[row] * (long long)V[row][col]) % modValue;
//             mul2 = (mul2 + (long long)AVShare0[row] * (long long)betaM[row][col]) % modValue;
//         }
//         long long res = (mul1 - mul2 + C[col]) % modValue;
//         if (res < 0) res += modValue;
//         z[col] = (int)res;
//     }

//     return z;
// }

vector<int> mpcVectorandMatrixMulBeaver(
    const vector<int>& alphaV,              // d = x - a
    const vector<vector<int>>& VShare,      // b_i
    const vector<vector<int>>& betaM,       // e = y - b
    const vector<int>& AVShare,             // a_i
    const vector<int>& CShare,              // c_i
    bool isParty0,                          // true only on party0
    int modValue)
{
    int n = (int)alphaV.size();
    int k = (int)VShare[0].size();

    vector<int> z(k);
    long long temp = 0;

    for (int col = 0; col < k; col++)
    {
        long long term = CShare[col]; // c_i[col]

        // d * b_i  (1×n times n×k)
        for (int row = 0; row < n; row++)
            term = (term + (long long)alphaV[row] * VShare[row][col]) % modValue;

        // a_i * e  (1×n times n×k)
        for (int row = 0; row < n; row++)
            term = (term + (long long)AVShare[row] * betaM[row][col]) % modValue;

        // only party0 adds d*e
        if (isParty0)
        {
            for (int row = 0; row < n; row++)
                term = (term + (long long)alphaV[row] * betaM[row][col]) % modValue;
        }

        if (term < 0) term += modValue;
        z[col] = (int)term;
    }

    return z;
}



int mpcDotProduct(
    const vector<int>& alpha,
    const vector<int>& BShare,
    const vector<int>& beta,
    const vector<int>& AShare,
    int CShare,
    int modValue)
{
    long long mul1 = 0, mul2 = 0;
    size_t sz = alpha.size();
    for (size_t i = 0; i < sz; ++i)
    {
        mul1 = (mul1 + (long long)alpha[i] * (long long)BShare[i]) % modValue;
        mul2 = (mul2 + (long long)beta[i] * (long long)AShare[i]) % modValue;
    }
    long long ZShare = (mul1 - mul2 + CShare) % modValue;
    if (ZShare < 0) ZShare += modValue;
    return (int)ZShare;
}

vector<int> mpcVectorandScalarMul(
    const vector<int>& alpha,
    int bShare,
    int beta,
    const vector<int>& AShare,
    const vector<int>& CShare,
    int modValue)
{
    vector<int> ZShare;
    ZShare.reserve(alpha.size());
    for (size_t i = 0; i < alpha.size(); ++i)
    {
        long long mul1 = ((long long)alpha[i] * (long long)bShare) % modValue;
        long long mul2 = ((long long)beta * (long long)AShare[i]) % modValue;
        long long intmVal = (mul1 - mul2 + CShare[i]) % modValue;
        if (intmVal < 0) intmVal += modValue;
        ZShare.push_back((int)intmVal);
    }
    return ZShare;
}



// #include <utility>
// #include "../headerFiles/mpcOperations.h"

// using namespace std;

// vector<int> mpcVectorandMatrixMul(
//     vector<int> alphaV,        // 1xn
//     vector<vector<int>> V,     // nxk
//     vector<vector<int>> betaM, // nxk
//     vector<int> AVShare0,
//     vector<int> C, // 1xk
//     int modValue)
// {
//     vector<int> z;

//     for (size_t col = 0; col < betaM.size(); col++)
//     {
//         int mul1 = 0, mul2 = 0;
//         for (size_t row = 0; row < alphaV.size(); row++)
//         {
//             mul1 = (mul1 + (int64_t)(alphaV[row] * V[row][col])) % modValue;
//             mul2 = (mul2 + (int64_t)(AVShare0[row] * betaM[row][col])) % modValue;
//         }
//         int res = (mul1 - mul2 + C[col]) % modValue;
//         res = res < 0 ? res + modValue : res;
//         z.push_back(res);
//     }

//     return z;
// }

// int mpcDotProduct( // dot product always result to scalar
//     vector<int> alpha,
//     vector<int> BShare,
//     vector<int> beta,
//     vector<int> AShare,
//     int CShare, int modValue = 64)
// {
//     int mul1 = 0, mul2 = 0, ZShare;
//     for (size_t i = 0; i < alpha.size(); i++)
//     {
//         mul1 = (mul1 + ((int64_t)alpha[i] * BShare[i]) % modValue) % modValue;
//         mul2 = (mul2 + ((int64_t)beta[i] * AShare[i]) % modValue) % modValue;
//     }
//     ZShare = (mul1 - mul2 + CShare) % modValue;
//     ZShare = ZShare < 0 ? ZShare + modValue : ZShare;
//     return ZShare;
// }

// // Vector AShare (same size as the vector)
// // Scalar BShare
// // Vector CShare = element-wise AShare * BShare
// vector<int> mpcVectorandScalarMul(
//     vector<int> alpha,
//     int bShare,
//     int beta,
//     vector<int> AShare,
//     vector<int> CShare,
//     int modValue = 64)
// {

//     vector<int> ZShare;
//     for (size_t i = 0; i < alpha.size(); i++)
//     {
//         int intmVal = 0, mul1 = 0, mul2 = 0;
//         mul1 = ((int64_t)alpha[i] * bShare) % modValue;
//         mul2 = ((int64_t)beta * AShare[i]) % modValue;
//         {
//             intmVal = (mul1 - mul2 + CShare[i]) % modValue;

//             intmVal = intmVal < 0 ? intmVal + modValue : intmVal;
//             ZShare.push_back(intmVal);
//         }
//     }
//     return ZShare;
// }