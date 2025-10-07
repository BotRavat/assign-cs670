#include <utility>
#include "../headerFiles/gen_queries.h"
#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

using namespace std;

void saveMatrix(const vector<vector<int>> &matrix, const string &filename)
{
    ofstream outFile(filename);
    if (!outFile)
    {
        cerr << "Error opening file for writing: " << filename << endl;
        return;
    }

    for (const auto &row : matrix)
    {
        for (size_t i = 0; i < row.size(); i++)
        {
            outFile << row[i];
            if (i != row.size() - 1)
                outFile << " ";
        }
        outFile << "\n";
    }

    outFile.close();
}

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

    saveMatrix(U.lVector, "U_matrixFull.txt");
    saveMatrix(V.lVector, "V_matrixFull.txt");

    // LatentVectorShares generateVectorShares(LatentVector lVector, int modValue);
    // struct LatentVectorShares{vector<vector<int>> lvShare0, lvShare1;};
    LatentVectorShares UShares = generateVectorShares(U, modValue);
    LatentVectorShares VShares = generateVectorShares(V, modValue);

   
    saveMatrix(UShares.lvShare1, "U_ShareMatrix1.txt");
    saveMatrix(VShares.lvShare0, "V_ShareMatrix0.txt");
    saveMatrix(VShares.lvShare1, "V_ShareMatrix1.txt");

    cout << "Matrices and shares saved successfully.\n";

    return 0;
}