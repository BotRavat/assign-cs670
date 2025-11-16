#include <utility>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include "../common.hpp"
#include "../headerFiles/gen_queries.h"

using namespace std;

int main(int argc, char *argv[])
{

    int modValue = 64;
    int m = getenv("USERS") ? atoi(getenv("USERS")) : 3;
    int n = getenv("ITEMS") ? atoi(getenv("ITEMS")) : 4;
    int k = getenv("FEATURES") ? atoi(getenv("FEATURES")) : 5;

    if (argc >= 4)
    {
        m = atoi(argv[1]);
        n = atoi(argv[2]);
        k = atoi(argv[3]);
    }

    cout << "Generating matrices with m=" << m << ", k=" << k << ", n=" << n << "\n";

    // Generate full latent vectors for users (U) and items (V)
    LatentVector U = generateLatentVector(k, modValue, m, 0); // 0 -> user
    LatentVector V = generateLatentVector(k, modValue, n, 1); // 1 -> item

    saveMatrix("U_matrixFull.txt", U.lVector);
    saveMatrix("V_matrixFull.txt", V.lVector);

    // LatentVectorShares generateVectorShares(LatentVector lVector, int modValue);
    // struct LatentVectorShares{vector<vector<int>> lvShare0, lvShare1;};
    LatentVectorShares UShares = generateVectorShares(U, modValue);
    LatentVectorShares VShares = generateVectorShares(V, modValue);

    saveMatrix("U_ShareMatrix0.txt", UShares.lvShare0);
    saveMatrix("U_ShareMatrix1.txt", UShares.lvShare1);
    saveMatrix("V_ShareMatrix0.txt", VShares.lvShare0);
    saveMatrix("V_ShareMatrix1.txt", VShares.lvShare1);

    cout << "Matrices and shares saved successfully.\n";

    return 0;
}