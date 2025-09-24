#include <iostream>
#include "../headerFiles/mpcOperations.h"

int main()
{

    // get shares of U, and V  from client or trusted dealer
    std::vector<std::vector<int>> U0, V0;

    // assuming we have to update user i on item j, get i and j from dealer
    int i, j, modValue = 64;

    // get beavers triplet for vectors from dealer
    std::vector<int> A0, B0;
    int c0;

    // sent blinded value to party S1
    // UA0=U0[i]+A0, VB0=V0[i]+B0   // vector addition
    std::vector<int> UA0, VB0;
    for (size_t k = 0; k < U0[i].size(); k++)
    {
        UA0.push_back((U0[i][k] + A0[k]) % modValue);
        VB0.push_back((V0[j][k] + B0[k]) % modValue);
    }
    

    // get blinded values from party S1
    std::vector<int> UA1, VB1; // U1[i]+A1,V1[j]+B1

    // calculate alpha, beta
    std::vector<int> alpha, beta;
    for (size_t k = 0; k < U0[i].size(); k++)
    {
        alpha.push_back((UA1[k] + U0[i][k] + A0[k]) % modValue);
        beta.push_back((VB1[k] + V0[j][k] + B0[k]) % modValue);
    }

    // compute share of mpc dot product of <ui,vi>
    int UdotV0 = mpcDotProduct(alpha, B0, beta, A0, c0, modValue);

    // get share of 1
    int one0;

    // compute share of 1-<ui,vj>
    int sub0 = (one0 - UdotV0) % modValue;
    sub0 = sub0 < 0 ? sub0 + modValue : sub0;

    // to compute vj(1- <ui,vj)

    // get beaver triplet of scalar and vector
    std::vector<int> AShare0, CShare0;
    int bShare; // scalar

    // sent blinded value to party S1 ( V0[j],AShare0, UdotV0+bShare0)
    // blinded_vec,blindV0A0= V0[j]+AShare0;  blinded_scalar,UdotV0BS0=UdotV0+bShare0

    // get blinded values from party S1
    std::vector<int> blindV1A1;
    int blindUdotV1BS1;

    std::vector<int> mul0 = mpcVectorandScalarMul(
        blindV1A1,
        bShare,
        blindUdotV1BS1,
        AShare0,
        CShare0,
        modValue);

    // now calculate ui=ui+vj(1-<ui,vj>)
    for (size_t k = 0; k < U0[i].size(); k++)
    {
        U0[i][k] = (U0[i][k] + mul0[k]) % modValue;
    }

    // sent share back to client
}