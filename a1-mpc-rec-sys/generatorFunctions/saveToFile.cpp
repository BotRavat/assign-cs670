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

    // if (argc < 4)
    // {
    //     cerr << "Usage: " << argv[0] << " <m: num users> <k: num features> <n: num items>\n";
    //     return -1;
    // }

    // int m = atoi(argv[1]); // number of users (rows in U)
    // int n = atoi(argv[2]); // number of items (rows in V)
    // int k = atoi(argv[3]); // number of features
    int modValue = 64;
    int m; // number of users
    int k; // number of features
    int n; // number of items
    cout << "Enter number of users :";
    cin >> m;
    cout << "\n Enter number of items :";
    cin >> n;
    cout << "\n Enter number of features :";
    cin >> k;

    cout << "Generating matrices with m=" << m << ", k=" << k << ", n=" << n << "\n";

    // Generate full latent vectors for users (U) and items (V)
    LatentVector U = generateLatentVector(k, modValue, m, 0); // 0 -> user
    LatentVector V = generateLatentVector(k, modValue, n, 1); // 1 -> item

    saveMatrix( "U_matrixFull.txt",U.lVector);
    saveMatrix( "V_matrixFull.txt",V.lVector);

    // LatentVectorShares generateVectorShares(LatentVector lVector, int modValue);
    // struct LatentVectorShares{vector<vector<int>> lvShare0, lvShare1;};
    LatentVectorShares UShares = generateVectorShares(U, modValue);
    LatentVectorShares VShares = generateVectorShares(V, modValue);

   
    saveMatrix("U_ShareMatrix0.txt",UShares.lvShare0);
    saveMatrix( "U_ShareMatrix1.txt",UShares.lvShare1);
    saveMatrix( "V_ShareMatrix0.txt",VShares.lvShare0);
    saveMatrix( "V_ShareMatrix1.txt",VShares.lvShare1);

    cout << "Matrices and shares saved successfully.\n";

    return 0;
}