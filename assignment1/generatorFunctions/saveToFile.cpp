#include <utility>
#include "../headerFiles/gen_queries.h"
#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

void saveMatrix(const std::vector<std::vector<int>> &matrix, const std::string &filename)
{
    std::ofstream outFile(filename);
    if (!outFile)
    {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }

    for (const auto &row : matrix)
    {
        for (std::size_t i = 0; i < row.size(); i++)
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
    //     std::cerr << "Usage: " << argv[0] << " <m: num users> <k: num features> <n: num items>\n";
    //     return -1;
    // }

    // int m = std::atoi(argv[1]); // number of users (rows in U)
    // int n = std::atoi(argv[2]); // number of items (rows in V)
    // int k = std::atoi(argv[3]); // number of features
    int modValue = 64;
    int m; // number of users
    int k; // number of features
    int n; // number of items
    std::cout << "Enter number of users :";
    std::cin >> m;
    std::cout << "\n Enter number of items :";
    std::cin >> n;
    std::cout << "\n Enter number of features :";
    std::cin >> k;

    std::cout << "Generating matrices with m=" << m << ", k=" << k << ", n=" << n << "\n";

    // Generate full latent vectors for users (U) and items (V)
    LatentVector U = generateLatentVector(k, modValue, m, 0); // 0 -> user
    LatentVector V = generateLatentVector(k, modValue, n, 1); // 1 -> item

    saveMatrix(U.lVector, "U_matrixFull.txt");
    saveMatrix(V.lVector, "V_matrixFull.txt");

    // LatentVectorShares generateVectorShares(LatentVector lVector, int modValue);
    // struct LatentVectorShares{std::vector<std::vector<int>> lvShare0, lvShare1;};
    LatentVectorShares UShares = generateVectorShares(U, modValue);
    LatentVectorShares VShares = generateVectorShares(V, modValue);

   
    saveMatrix(UShares.lvShare1, "U_ShareMatrix1.txt");
    saveMatrix(VShares.lvShare0, "V_ShareMatrix0.txt");
    saveMatrix(VShares.lvShare1, "V_ShareMatrix1.txt");

    std::cout << "Matrices and shares saved successfully.\n";

    return 0;
}