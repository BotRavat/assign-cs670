#include <utility>
#include <iostream>
#include <random>
#include <fstream>
#include <sstream>
#include <vector>
#include <boost/asio.hpp>
#include "../headerFiles/gen_queries.h"
#include "../common.hpp"

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
using namespace std;
using u128 = unsigned __int128;

struct DpfKey
{
    int targetLocation;
    vector<u128> V0, V1; // vector containing nodes of tree for respective tree, size= total number of nodes
    vector<bool> T0, T1; // vector containing flag bits for each node, size= total number of nodes
    vector<u128> CW;     // vector containing correction word per level, size= height of tree+1
    vector<u128> FCW0;   // final correction word at leaf level for party 0
    vector<u128> FCW1;   // final correction word at leaf level for party 1
};




awaitable<void> send_shares_to_party(
    tcp::socket &socket,
    int32_t n, int32_t k, int32_t modValue,
    vector<int> AShare,
    int b,
    vector<int> CShare,
    vector<int> vectorA, vector<int> vectorB,
    int scalarC,
    vector<int> eshare,
    vector<int> AV,
    vector<vector<int>> BM,
    vector<int> CV,
    int oneShare, int query_i)
{
    try
    {
        co_await send_int(socket, n, "n");
        co_await send_int(socket, k, "k");
        co_await send_int(socket, modValue, "modValue");

        // matrix triplet
        co_await sendVector(socket, AV);
        co_await sendMatrix(socket, BM);
        co_await sendVector(socket, CV);

        // vector triplet
        co_await sendVector(socket, vectorA);
        co_await sendVector(socket, vectorB);
        co_await send_int(socket, scalarC, "scalarC");

        // scalar and vector triplet
        co_await sendVector(socket, AShare);
        co_await send_int(socket, b, "b");
        co_await sendVector(socket, CShare);

        // e share triplet
        co_await sendVector(socket, eshare);

        // share of one
        co_await send_int(socket, oneShare, "oneShare");
        co_await send_int(socket, query_i, "query_i");

        cout << "Dealer: sent shares and triplets to a connected party.\n";
    }
    catch (exception &e)
    {
        cerr << "Dealer send error: " << e.what() << "\n";
    }
    co_return;
}

awaitable<vector<int>> receiveFinalShares(tcp::socket &socket, const string &name)
{
    vector<int> finalShare;
    co_await recvVector(socket, finalShare);
    cout << "\n Dealer: received final share from a party: " << name << "\n";
    co_return finalShare;
}

// ------------------ Correctness Check Helper ------------------

void checkCorrectnessAndUpdate(
    int query_i, int query_j, int modValue,
    vector<int> finalShare0, vector<int> finalShare1, size_t qi)
{

    // Load full U and V matrices
    vector<vector<int>> U = loadMatrix("U_matrixFull.txt");
    vector<vector<int>> V = loadMatrix("V_matrixFull.txt");

    cout << "Full User matrix is :\n";
    for (size_t i = 0; i < U.size(); i++)
    {
        for (size_t j = 0; j < U[0].size(); j++)
        {
            cout << U[i][j] << " ";
        }
        cout << "\n";
    }

    cout << "checking User matrix shares :\n";
    vector<vector<int>> U0 = loadMatrix("U_ShareMatrix0.txt");
    vector<vector<int>> U1 = loadMatrix("U_ShareMatrix1.txt");
    for (size_t i = 0; i < U0.size(); i++)
    {
        for (size_t j = 0; j < U0[0].size(); j++)
        {
            cout << (U0[i][j] + U1[i][j]) % modValue << " ";
        }
        cout << "\n";
    }
    cout << "Full Item matrix is :\n";
    for (size_t i = 0; i < V.size(); i++)
    {
        for (size_t j = 0; j < V[0].size(); j++)
        {
            cout << V[i][j] << " ";
        }
        cout << "\n";
    }

    cout << "checking Item matrix shares :\n";
    vector<vector<int>> V0 = loadMatrix("V_ShareMatrix0.txt");
    vector<vector<int>> V1 = loadMatrix("V_ShareMatrix1.txt");
    for (size_t i = 0; i < V0.size(); i++)
    {
        for (size_t j = 0; j < V0[0].size(); j++)
        {
            cout << (V0[i][j] + V1[i][j]) % modValue << " ";
        }
        cout << "\n";
    }

    if (U.empty() || V.empty())
    {
        std::cerr << "[Dealer] Failed to load matrices.\n";
        return;
    }

    vector<int> ui = U[query_i];
    vector<int> vj = V[query_j];
    int k = ui.size();

    // Step 1: compute dot product <ui, vj>
    int dot = 0;
    for (int t = 0; t < k; ++t)
        dot = (dot + ui[t] * vj[t]) % modValue;

    // Step 2: compute (1 - <ui, vj>) mod modValue
    int factor = (1 - dot) % modValue;
    if (factor < 0)
        factor += modValue;

    // Step 3: compute vj * factor
    vector<int> v_scaled(k);
    for (int t = 0; t < k; ++t)
        v_scaled[t] = (vj[t] * factor) % modValue;

    // Step 4: ui = ui + v_scaled
    for (int t = 0; t < k; ++t)
        ui[t] = (ui[t] + v_scaled[t]) % modValue;

    // reconstruct and save
    vector<int> reconstructed(k);
    for (int i = 0; i < k; i++)
        reconstructed[i] = (finalShare0[i] + finalShare1[i]) % modValue;

    // --- Compare and display ---
    cout << "\nDealer: Checking correctness for query (" << query_i << "," << query_j << ")\n";
    cout << "Reconstructed (from MPC): ";
    for (int val : reconstructed)
        cout << val << " ";
    cout << "\nExpected (direct computation): ";
    for (int val : ui)
        cout << val << " ";
    cout << "\n";

    //  write both to file
    // ofstream checkFile("correctness_check.txt", ios::app);
    // checkFile << "Query (" << query_i << "," << query_j << ")\n";
    // checkFile << "MPC Result: ";
    // for (int val : reconstructed) checkFile << val << " ";
    // checkFile << "\nExpected:   ";
    // for (int val : expected) checkFile << val << " ";
    // checkFile << "\n\n";
    // checkFile.close();
}

awaitable<void> dealer_main(boost::asio::io_context &io,
                            const vector<pair<int, int>> &queries,
                            int n, int k, int modValue)
{
    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 9002));
    tcp::socket socket_p0(io), socket_p1(io);

    cout << "Dealer: waiting for P0...\n";
    co_await acceptor.async_accept(socket_p0, use_awaitable);
    cout << "Dealer: P0 connected.\n";

    cout << "Dealer: waiting for P1...\n";
    co_await acceptor.async_accept(socket_p1, use_awaitable);
    cout << "Dealer: P1 connected.\n";

    // Process all queries
    for (size_t qi = 0; qi < queries.size(); qi++)
    {
        auto [query_i, query_j] = queries[qi];
        cout << "\n=== Processing Query " << qi + 1
             << " (" << query_i << "," << query_j << ") ===\n";

        // Generate data for this query (your function)
        auto svTriplet = generateScalarandVectorTriplet(k, modValue);
        auto svTripletShares = sAndVectorTripletShares(svTriplet, modValue);
        auto vTriplet = generateVectorTriplet(k, modValue);
        auto vTripletSharesS = vTripletShares(vTriplet, modValue);
        auto oneShares = sharesOfOne(modValue);

        vector<int> e(n, 0);
        if (query_j >= 0 && query_j < n)
            e[query_j] = 1;
        auto eShares = sharesOfe(e, modValue);
        auto mvTriplet = generateMTriplet(n, k, modValue);
        auto mvTripletShares = genMShare(mvTriplet, modValue);

        // dpf generation
        DpfKey dpf;
        dpf.targetLocation = query_j;
        int dpf_size = pow(2, k);
        int totalNodes = 2 * dpf_size - 1;
        dpf.V0.resize(totalNodes, (u128)0);
        dpf.V1.resize(totalNodes, (u128)0);
        dpf.T0.resize(totalNodes, false);
        dpf.T1.resize(totalNodes, false);
        int height = (int)(ceil(log2(dpf_size)));
        dpf.CW.resize(height + 1, (u128)0);
        dpf.FCW0.resize(k, (u128)0);
        dpf.FCW1.resize(k, (u128)0);
        generateDPF(dpf, dpf_size);
        u128 rootSeed= dpf.V0[0];
        vector<bool> T0= dpf.T0;
        vector<bool>T1= dpf.T1;
        vector<u128> CW= dpf.CW;
        vector<u128> FCW0= dpf.FCW0;
        vector<u128> FCW1= dpf.FCW1;


        // Send to both parties in parallel
        co_spawn(io, [&]() -> awaitable<void>
                 {
            co_await send_shares_to_party(socket_p0, n, k, modValue,
                                          svTripletShares.AShare0, svTripletShares.b0, svTripletShares.CShare0,
                                          vTripletSharesS.vectorA0, vTripletSharesS.vectorB0, vTripletSharesS.scalarC0,
                                          eShares.eshare0,
                                          mvTripletShares.AVShare0, mvTripletShares.BMShare0, mvTripletShares.CVShare0,
                                          oneShares.first, query_i,rootSeed,T0,CW,FCW0);
            co_return; }(), detached);

        co_spawn(io, [&]() -> awaitable<void>
                 {
            co_await send_shares_to_party(socket_p1, n, k, modValue,
                                          svTripletShares.AShare1, svTripletShares.b1, svTripletShares.CShare1,
                                          vTripletSharesS.vectorA1, vTripletSharesS.vectorB1, vTripletSharesS.scalarC1,
                                          eShares.eshare1,
                                          mvTripletShares.AVShare1, mvTripletShares.BMShare1, mvTripletShares.CVShare1,
                                          oneShares.second, query_i,rootSeed,T1,CW,FCW1);
            co_return; }(), detached);

        // Wait for both parties to return final shares
        vector<int> finalShare0 = co_await receiveFinalShares(socket_p0, "Party0");
        vector<int> finalShare1 = co_await receiveFinalShares(socket_p1, "Party1");
        checkCorrectnessAndUpdate(query_i, query_j, modValue, finalShare0, finalShare1, qi);
    }

    // === Send "DONE" message to both parties ===
    int doneFlag = -1;
    co_await send_int(socket_p0, doneFlag, "doneFlag");
    co_await send_int(socket_p1, doneFlag, "doneFlag");

    cout << "\nDealer: Sent DONE message to both parties.\n";

    socket_p0.close();
    socket_p1.close();
    cout << "Dealer: Closed all connections.\n";
    co_return;
}

int main()
{
    DpfKey dpf;
    try
    {
        int modValue = 64;
        int m, n, k;
        vector<vector<int>> U0 = loadMatrix("U_ShareMatrix0.txt");
        vector<vector<int>> U1 = loadMatrix("U_ShareMatrix1.txt");
        vector<vector<int>> V0 = loadMatrix("V_ShareMatrix0.txt");
        vector<vector<int>> V1 = loadMatrix("V_ShareMatrix1.txt");

        if (U0.empty() || V0.empty())
        {
            cerr << "Error loading matrices.\n";
            return -1;
        }

        m = U0.size();
        k = U0[0].size();
        n = V0.size();

        int numQueries;
        vector<pair<int, int>> queries;
        cout << "Enter number of queries: ";
        cin >> numQueries;
        cout << "Enter each query (i j): \n";
        for (int q = 0; q < numQueries; ++q)
        {
            int i, j;
            cin >> i >> j;
            queries.emplace_back(i, j);
        }

        // generateDPF()
        boost::asio::io_context io;
        co_spawn(io, dealer_main(io, queries, n, k, modValue), detached);
        io.run();
    }
    catch (exception &e)
    {
        cerr << "Dealer exception: " << e.what() << "\n";
    }
}
