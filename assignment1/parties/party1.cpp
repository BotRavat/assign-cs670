#include <iostream>
#include "../headerFiles/mpcOperations.h"

int main()
{

    // get shares of U, and V  from client or trusted dealer
    std::vector<std::vector<int>> U1, V1;

    // assuming we have to update user i on item j, get i and j from dealer
    int i, j, modValue = 64;

    // get beavers triplet for vectors from dealer
    std::vector<int> A1, B1;
    int c1;

    // sent blinded value to party S0
    // UA1=U1[i]+A1, VB1=V1[i]+B1   // vector addition
    std::vector<int> UA1, VB1;
    for (size_t k = 0; k < U1[i].size(); k++)
    {
        UA1.push_back((U1[i][k] + A1[k]) % modValue);
        VB1.push_back((V1[j][k] + B1[k]) % modValue);
    }


    // get blinded values from party S0
    std::vector<int> UA0, VB0; // U0[i]+A0,V0[j]+B0

    // calculate alpha, beta
    std::vector<int> alpha, beta;
    for (size_t k = 0; k < U1[i].size(); k++)
    {
        alpha.push_back((UA0[k] + U1[i][k] + A1[k]) % modValue);
        beta.push_back((VB0[k] + V1[j][k] + B1[k]) % modValue);
    }

    // compute share of mpc dot product
    int UdotV1 = mpcDotProduct(alpha, B1, beta, A1, c1, modValue);

    // get share of 1
    int one1;

    // compute share of 1-<ui,vj>
    int sub1 = (one1 - UdotV1) % modValue;
    sub1 = sub1 < 0 ? sub1 + modValue : sub1;

    // to compute vj(1- <ui,vj)

    // get beaver triplet of scalar and vector
    std::vector<int> AShare1, CShare1;
    int bShare; // scalar

    // sent blinded value to party S0 ( V1[j],AShare1, UdotV1+bShare1)
    // blinded_vec,blindV1A1= V1[j]+AShare1;  blinded_scalar,UdotV1BS1=UdotV1+bShare1

    // get blinded values from party S0
    std::vector<int> blindV0A0;
    int blindUdotV0BS0;

    std::vector<int> mul1 = mpcVectorandScalarMul(
        blindV0A0,
        bShare,
        blindUdotV0BS0,
        AShare1,
        CShare1,
        modValue);

    // now calculate ui=ui+vj(1-<ui,vj>)
    for (size_t k = 0; k < U1[i].size(); k++)
    {
        U1[i][k] = (U1[i][k] + mul1[k]) % modValue;
    }

    // sent share back to client
}