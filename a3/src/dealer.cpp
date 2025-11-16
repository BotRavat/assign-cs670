#include <utility>
#include <iostream>
#include <random>
#include <fstream>
#include <sstream>
#include <vector>
#include <boost/asio.hpp>
#include "../headerFiles/gen_queries.h"
#include "../common.hpp"
#include "dpf.hpp"

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
using namespace std;
using u128 = unsigned __int128;

// struct DpfKey
// {
//     int targetLocation;
//     vector<u128> V0, V1; // vector containing nodes of tree for respective tree, size= total number of nodes
//     vector<bool> T0, T1; // vector containing flag bits for each node, size= total number of nodes
//     vector<u128> CW;     // vector containing correction word per level, size= height of tree+1
//     vector<u128> FCW0;   // final correction word at leaf level for party 0
//     vector<u128> FCW1;   // final correction word at leaf level for party 1
// };

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
    int oneShare, int query_i,
    u128 rootSeed,
    const vector<bool> &T,
    const vector<u128> &CW,
    const vector<u128> &FCW)
{
    try
    {
        cout<<"Dealer: sending shares and triplets to a connected party.\n";
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

        // Send DPF data
        co_await send_u128(socket, rootSeed);

        co_await sendVectorBool(socket, T);

        co_await sendVector_u128(socket, CW);
        co_await sendVector_u128(socket, FCW);

        cout << "Dealer: sent shares and triplets to a connected party.\n";
    }
    catch (exception &e)
    {
        cerr << "Dealer send error: " << e.what() << "\n";
    }
    co_return;
}

awaitable<vector<vector<int>>> receiveFinalShares(tcp::socket &socket, const string &name)
{
    vector<vector<int>> finalShare;
    co_await recvMatrix(socket, finalShare);
    cout << "\n Dealer: received final share from a party: " << name << "\n";
    co_return finalShare;
}

// ------------------ Correctness Check Helper ------------------

// Replace existing function with this version
void checkCorrectnessAndUpdate(
    int query_i, int query_j, int modValue,
    const vector<vector<int>> &finalShare0,
    const vector<vector<int>> &finalShare1,
    size_t qi)
{
    auto mod_norm = [&](long long x) -> int
    {
        if (modValue == 0)
            return (int)x; // avoid div zero, though modValue should be > 0
        long long r = x % (long long)modValue;
        if (r < 0)
            r += modValue;
        return (int)r;
    };

    // Load full U and V matrices
    vector<vector<int>> U = loadMatrix("/app/matrices/U_matrixFull.txt");
    vector<vector<int>> V = loadMatrix("/app/matrices/V_matrixFull.txt");
    if (U.empty() || V.empty())
    {
        std::cerr << "[Dealer] Failed to load full U or V matrices.\n";
        return;
    }

    // Basic index checks
    if (query_i < 0 || (size_t)query_i >= U.size())
    {
        std::cerr << "[Dealer] query_i out of range: " << query_i << "\n";
        return;
    }
    if (query_j < 0 || (size_t)query_j >= V.size())
    {
        std::cerr << "[Dealer] query_j out of range: " << query_j << "\n";
        return;
    }

    // print (optional) - keep short in production
    cout << "\nDealer: Checking query #" << (qi + 1) << " (" << query_i << "," << query_j << ")\n";

    vector<int> ui = U[query_i];
    vector<int> vj = V[query_j];
    int k = (int)ui.size();

    if ((int)vj.size() != k)
    {
        std::cerr << "[Dealer] Dimension mismatch: ui.size()=" << k << " but vj.size()=" << vj.size() << "\n";
        return;
    }

    // compute dot product using wider accumulator
    long long dot_acc = 0;
    for (int t = 0; t < k; ++t)
    {
        dot_acc += (long long)ui[t] * (long long)vj[t];
        // if product might become huge you can periodically reduce:
        if (llabs(dot_acc) > (1ll << 60))
            dot_acc = mod_norm(dot_acc); // keep small
    }
    int dot = mod_norm(dot_acc);

    // compute factor = (1 - dot) mod modValue
    int factor = mod_norm(1 - dot);

    // compute u_scaled = ui * factor mod modValue
    vector<int> u_scaled(k);
    for (int t = 0; t < k; ++t)
        u_scaled[t] = mod_norm((long long)ui[t] * (long long)factor);

    // expected updated vj
    vector<int> expected(k);
    for (int t = 0; t < k; ++t)
        expected[t] = mod_norm((long long)vj[t] + (long long)u_scaled[t]);

    // Validate finalShare sizes
    if ((size_t)query_j >= finalShare0.size() || (size_t)query_j >= finalShare1.size())
    {
        std::cerr << "[Dealer] Received final shares do not contain index " << query_j << ".\n";
        return;
    }
    if ((int)finalShare0[query_j].size() != k || (int)finalShare1[query_j].size() != k)
    {
        std::cerr << "[Dealer] Final share row length mismatch: expected " << k
                  << ", got " << finalShare0[query_j].size()
                  << " and " << finalShare1[query_j].size() << "\n";
        return;
    }

    // reconstruct from MPC outputs
    vector<int> reconstructed(k);
    for (int t = 0; t < k; ++t)
        reconstructed[t] = mod_norm((long long)finalShare0[query_j][t] + (long long)finalShare1[query_j][t]);

    // Print comparison and basic check
    cout << "Reconstructed (from MPC): ";
    for (int val : reconstructed)
        cout << val << " ";
    cout << "\nExpected (direct computation): ";
    for (int val : expected)
        cout << val << " ";
    cout << "\n";

    // quick correctness flag
    bool ok = true;
    for (int t = 0; t < k; ++t)
    {
        if (reconstructed[t] != expected[t])
        {
            ok = false;
            break;
        }
    }
    if (ok)
        cout << "Dealer: CORRECT for query (" << query_i << "," << query_j << ").\n";
    else
        cout << "Dealer: MISMATCH for query (" << query_i << "," << query_j << ").\n";

    // (optional) write to file for logging
    // ofstream ofs("correctness_log.txt", ios::app);
    // ofs << "Query (" << query_i << "," << query_j << ") result: " << (ok ? "OK" : "FAIL") << "\n";
    // ofs.close();
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

    // ------------------ PROCESS ALL QUERIES ------------------
    for (size_t qi = 0; qi < queries.size(); qi++)
    {
        auto [query_i, query_j] = queries[qi];
        cout << "\n=== Processing Query " << qi + 1
             << " (" << query_i << "," << query_j << ") ===\n";

        // ----------- GENERATE ALL SHARE MATERIAL FOR THIS QUERY -----------
        auto svTriplet       = generateScalarandVectorTriplet(k, modValue);
        auto svTripletShares = sAndVectorTripletShares(svTriplet, modValue);

        auto vTriplet        = generateVectorTriplet(k, modValue);
        auto vTripletSharesS = vTripletShares(vTriplet, modValue);

        auto oneShares       = sharesOfOne(modValue);

        vector<int> e(n, 0);
        if (query_j >= 0 && query_j < n)
            e[query_j] = 1;

        auto eShares         = sharesOfe(e, modValue);

        auto mvTriplet       = generateMTriplet(n, k, modValue);
        auto mvTripletShares = genMShare(mvTriplet, modValue);

        // ------------------ DPF GENERATION ------------------
        DpfKey dpf;
        dpf.targetLocation = query_j;

        int dpf_size     = n;
        int totalNodes   = 2 * dpf_size - 1;
        int height       = (int)ceil(log2(dpf_size));

        dpf.V0.resize(totalNodes, (u128)0);
        dpf.V1.resize(totalNodes, (u128)0);
        dpf.T0.resize(totalNodes, false);
        dpf.T1.resize(totalNodes, false);
        dpf.CW.resize(height + 1, (u128)0);
        dpf.FCW0.resize(k, (u128)0);
        dpf.FCW1.resize(k, (u128)0);

        generateDPF(dpf, dpf_size);

        // -------- DPF PARTIES' KEYS ----------
        u128 rootSeed = dpf.V0[0];

        vector<bool>  T0   = dpf.T0;
        vector<bool>  T1   = dpf.T1;

        vector<u128> CW    = dpf.CW;
        vector<u128> FCW0  = dpf.FCW0;
        vector<u128> FCW1  = dpf.FCW1;

        // ====================================================================
        // ✔ FIX #1 — CAPTURE DATA BY VALUE INSIDE ASYNC LAMBDAS
        // ====================================================================

        // Copy P0’s objects locally so lambdas get safe copies
        auto AShare0_copy   = svTripletShares.AShare0;
        auto b0_copy        = svTripletShares.b0;
        auto CShare0_copy   = svTripletShares.CShare0;

        auto vectorA0_copy  = vTripletSharesS.vectorA0;
        auto vectorB0_copy  = vTripletSharesS.vectorB0;
        auto scalarC0_copy  = vTripletSharesS.scalarC0;

        auto e0_copy        = eShares.eshare0;

        auto AV0_copy       = mvTripletShares.AVShare0;
        auto BM0_copy       = mvTripletShares.BMShare0;
        auto CV0_copy       = mvTripletShares.CVShare0;

        int one0_copy       = oneShares.first;


        // Copy P1’s objects
        auto AShare1_copy   = svTripletShares.AShare1;
        auto b1_copy        = svTripletShares.b1;
        auto CShare1_copy   = svTripletShares.CShare1;

        auto vectorA1_copy  = vTripletSharesS.vectorA1;
        auto vectorB1_copy  = vTripletSharesS.vectorB1;
        auto scalarC1_copy  = vTripletSharesS.scalarC1;

        auto e1_copy        = eShares.eshare1;

        auto AV1_copy       = mvTripletShares.AVShare1;
        auto BM1_copy       = mvTripletShares.BMShare1;
        auto CV1_copy       = mvTripletShares.CVShare1;

        int one1_copy       = oneShares.second;

        auto T0_copy        = T0;
        auto T1_copy        = T1;
        auto CW_copy        = CW;
        auto FCW0_copy      = FCW0;
        auto FCW1_copy      = FCW1;

        // ====================================================================
        // ✔ FIX #2 — START PARALLEL SENDS WITH SAFE COPIES
        // ====================================================================

        cout << "Dealer: sending shares to P0 & P1...\n";

        co_spawn(io,
                 [=, &socket_p0]() mutable -> awaitable<void>
                 {
                     co_await send_shares_to_party(
                         socket_p0,
                         n, k, modValue,
                         AShare0_copy, b0_copy, CShare0_copy,
                         vectorA0_copy, vectorB0_copy, scalarC0_copy,
                         e0_copy,
                         AV0_copy, BM0_copy, CV0_copy,
                         one0_copy, query_i,
                         rootSeed,
                         T0_copy,
                         CW_copy,
                         FCW0_copy
                     );
                     co_return;
                 },
                 detached);

        co_spawn(io,
                 [=, &socket_p1]() mutable -> awaitable<void>
                 {
                     co_await send_shares_to_party(
                         socket_p1,
                         n, k, modValue,
                         AShare1_copy, b1_copy, CShare1_copy,
                         vectorA1_copy, vectorB1_copy, scalarC1_copy,
                         e1_copy,
                         AV1_copy, BM1_copy, CV1_copy,
                         one1_copy, query_i,
                         rootSeed,
                         T1_copy,
                         CW_copy,
                         FCW1_copy
                     );
                     co_return;
                 },
                 detached);

        // ====================================================================
        // ✔ FIX #3 — WAIT FOR FINAL SHARES FROM BOTH PARTIES
        // ====================================================================

        vector<vector<int>> finalShare0 = co_await receiveFinalShares(socket_p0, "Party0");
        vector<vector<int>> finalShare1 = co_await receiveFinalShares(socket_p1, "Party1");

        checkCorrectnessAndUpdate(query_i, query_j, modValue, finalShare0, finalShare1, qi);
    }

    // ========================================================================
    // SEND DONE FLAG
    // ========================================================================
    int doneFlag = -1;
    co_await send_int(socket_p0, doneFlag, "doneFlag");
    co_await send_int(socket_p1, doneFlag, "doneFlag");

    cout << "\nDealer: Sent DONE message to both parties.\n";

    socket_p0.close();
    socket_p1.close();

    cout << "Dealer: Closed all connections.\n";
    co_return;
}



int main(int argc, char *argv[])
{
    DpfKey dpf;
    try
    {
        int modValue = 64;
        int m, n, k;
        vector<vector<int>> U0 = loadMatrix("/app/matrices/U_ShareMatrix0.txt");
        vector<vector<int>> U1 = loadMatrix("/app/matrices/U_ShareMatrix1.txt");
        vector<vector<int>> V0 = loadMatrix("/app/matrices/V_ShareMatrix0.txt");
        vector<vector<int>> V1 = loadMatrix("/app/matrices/V_ShareMatrix1.txt");

        if (U0.empty() || V0.empty())
        {
            cerr << "Error loading matrices.\n";
            return -1;
        }

        m = U0.size();
        k = U0[0].size();
        n = V0.size();
        int numQueries = getenv("NUM_QUERIES") ? atoi(getenv("NUM_QUERIES")) : 2;
        if (argc >= 2)
        {
            numQueries = atoi(argv[1]);
        }

        cout << "Number of queries: " << numQueries << "\n";

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist_i(0, m - 1);
        std::uniform_int_distribution<> dist_j(0, n - 1);

        vector<pair<int, int>> queries;
        for (int q = 0; q < numQueries; ++q)
        {
            int i = dist_i(gen);
            int j = dist_j(gen);
            queries.emplace_back(i, j);
        }
        cout << "Generated Queries:\n";
        for (auto &[i, j] : queries)
            cout << "(" << i << "," << j << ")\n";

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
