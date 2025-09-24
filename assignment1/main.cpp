#include <iostream>
#include "../headerFiles/gen_queries.h"

int main()
{

    int sizeOfVector = 5, modValue = 64, numberOfUsers = 2, numberOfItems = 3;
    LatentVector userVectors, itemVectors;
    LatentVectorShares uVShares, iVShares;

    userVectors = generateLatentVector(sizeOfVector, modValue, numberOfUsers, 0);
    itemVectors = generateLatentVector(sizeOfVector, modValue, numberOfItems, 1);
    uVShares = generateVectorShares(userVectors, modValue);
    iVShares = generateVectorShares(itemVectors, modValue);

    std::cout << "Printing user vectors \n";
    for (size_t i = 0; i < userVectors.lVector.size(); i++)
    {
        std::cout << "Vector " << i << ": ";
        for (size_t j = 0; j < userVectors.lVector[i].size(); j++)
            std::cout << userVectors.lVector[i][j] << " ";
        std::cout << "\n";
    }

    std::cout << "User Vector shares \n";
    for (size_t i = 0; i < uVShares.lvShare0.size(); i++)
    {
        std::cout << "Share 0: ";
        for (size_t j = 0; j < uVShares.lvShare0[i].size(); j++)
            std::cout << uVShares.lvShare0[i][j] << " ";
        std::cout << "\n";
        std::cout << "Share 1: ";

        for (size_t j = 0; j < uVShares.lvShare1[i].size(); j++)
            std::cout << uVShares.lvShare1[i][j] << " ";
        std::cout << "\n";
    }

    std::cout << "Printing item vectors \n";

    for (size_t i = 0; i < itemVectors.lVector.size(); i++)
    {
        std::cout << "Vector " << i << ": ";

        for (size_t j = 0; j < itemVectors.lVector[i].size(); j++)
            std::cout << itemVectors.lVector[i][j] << " ";
        std::cout << "\n";
    }

    std::cout << "Item Vector shares \n";
    for (size_t i = 0; i < iVShares.lvShare0.size(); i++)
    {
        std::cout << "Share 0: ";
        for (size_t j = 0; j < iVShares.lvShare0[i].size(); j++)
            std::cout << iVShares.lvShare0[i][j] << " ";
        std::cout << "\n";
        std::cout << "Share 1: ";

        for (size_t j = 0; j < iVShares.lvShare1[i].size(); j++)
            std::cout << iVShares.lvShare1[i][j] << " ";
        std::cout << "\n";
    }

    ScalarTriplet sTriplet = generateScalarTriplet(modValue);
    std::cout << "a: " << sTriplet.a << " a: " << sTriplet.b << " c: " << sTriplet.c << "\n";

    ScalarTripletShares scalarShares = sTripletShares(sTriplet, modValue);
    std::cout << "a0: " << scalarShares.a0 << " a1: " << scalarShares.a1 << " b0: " << scalarShares.b0 << " b1: " << scalarShares.b1 << " c0:" << scalarShares.c0 << " c1: " << scalarShares.c1 << "\n";

    VectorTriplet vTriplet = generateVectorTriplet(sizeOfVector, modValue);
    std::cout << "Vector A: ";
    for (int i = 0; i < sizeOfVector; i++)
        std::cout << vTriplet.vectorA[i] << " ";
    std::cout << "\n";
    std::cout << "Vector B: ";
    for (int i = 0; i < sizeOfVector; i++)
        std::cout << vTriplet.vectorB[i] << " ";
    std::cout << "\n";
    std::cout << "C: " << vTriplet.scalarC << "\n";

    return 0;
}