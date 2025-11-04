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

// Run multiple coroutines in parallel
template <typename... Fs>
void run_in_parallel(boost::asio::io_context &io, Fs &&...funcs)
{
    // (co_spawn(io, funcs, detached), ...);
    vector<awaitable<void>> tasks = {funcs()...};
    for (auto &t : tasks)
        co_await t;
    co_return;
}

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

        //matrix triplet
        co_await sendVector(socket, AV);
        co_await sendMatrix(socket, BM);
        co_await sendVector(socket, CV);

        //vector triplet
        co_await sendVector(socket, vectorA);
        co_await sendVector(socket, vectorB);
        co_await send_int(socket, scalarC, "scalarC");
       
        //scalar and vector triplet
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

awaitable<vector<int>> receiveFinalShares(tcp::socket &socket, int k, const string &name)
{
    vector<int> finalShare;
    co_await recvVector(socket, finalShare);
    cout << "\n Dealer: received final share from a party: " << name << "\n";
    co_return finalShare;
}

awaitable<void> sendAndRcvFromParties(
    boost::asio::io_context &io_context,
    int n, int k, int modValue,
    int query_i, int query_j)
{
    // Generate all shares
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

    cout << "mvTripletShare size" << mvTripletShares.BMShare0.size();

    try
    {
        // boost::asio::io_context &io_context = co_await boost::asio::this_coro::executor;
        // boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 9002));

        tcp::socket socket_p0(io_context);
        cout << "Dealer: waiting for P0...\n";

        co_await acceptor.async_accept(socket_p0, use_awaitable);
        // acceptor.accept(socket_p0);
        cout << "Dealer: P0 connected.\n";

        tcp::socket socket_p1(io_context);
        cout << "Dealer: waiting for P1...\n";
        co_await acceptor.async_accept(socket_p1, use_awaitable);
        // acceptor.accept(socket_p1);
        cout << "Dealer: P1 connected.\n";

        // Send sequentially: first P0, then P1
        co_await send_shares_to_party(socket_p0, n, k, modValue,
                                      svTripletShares.AShare0, svTripletShares.b0, svTripletShares.CShare0,
                                      vTripletSharesS.vectorA0, vTripletSharesS.vectorB0, vTripletSharesS.scalarC0,
                                      eShares.eshare0,
                                      mvTripletShares.AVShare0, mvTripletShares.BMShare0, mvTripletShares.CVShare0,
                                      oneShares.first, query_i);

        co_await send_shares_to_party(socket_p1, n, k, modValue,
                                      svTripletShares.AShare1, svTripletShares.b1, svTripletShares.CShare1,
                                      vTripletSharesS.vectorA1, vTripletSharesS.vectorB1, vTripletSharesS.scalarC1,
                                      eShares.eshare1,
                                      mvTripletShares.AVShare1, mvTripletShares.BMShare1, mvTripletShares.CVShare1,
                                      oneShares.second, query_i);
        // Launch coroutines for P0 and P1 in parallel
        // run_in_parallel(io_context, [&]() -> awaitable<void>
        //                 {
        //         co_await send_shares_to_party(socket_p0,
        //                                       n, k, modValue,
        //                                       svTripletShares.AShare0, svTripletShares.b0, svTripletShares.CShare0,
        //                                       vTripletSharesS.vectorA0, vTripletSharesS.vectorB0, vTripletSharesS.scalarC0,
        //                                       eShares.eshare0,
        //                                       mvTripletShares.AShare0, mvTripletShares.BB0, mvTripletShares.C0,
        //                                       oneShares.first);
        //         co_return; }, [&]() -> awaitable<void>
        //                 {
        //         co_await send_shares_to_party(socket_p1,
        //                                       n, k, modValue,
        //                                       svTripletShares.AShare1, svTripletShares.b1, svTripletShares.CShare1,
        //                                       vTripletSharesS.vectorA1, vTripletSharesS.vectorB1, vTripletSharesS.scalarC1,
        //                                       eShares.eshare1,
        //                                       mvTripletShares.AShare1, mvTripletShares.B1, mvTripletShares.C1,
        //                                       oneShares.second);
        //         co_return; });

        vector<int> finalShare0 = co_await receiveFinalShares(socket_p0, k, "party0");
        vector<int> finalShare1 = co_await receiveFinalShares(socket_p1, k, "party1");
        cout << "here";
        // io_context.run();

        // Reconstruct vector
        vector<int> reconstructed(k);
        for (int i = 0; i < k; i++)
            reconstructed[i] = (finalShare0[i] + finalShare1[i]) % modValue;

        ofstream outFile("reconstructed_user_vector.txt");
        for (int val : reconstructed)
            outFile << val << " ";
        outFile << "\n";
        cout << "Dealer: reconstructed user vector saved.\n";

        socket_p0.close();
        socket_p1.close();
    }
    catch (exception &e)
    {
        cerr << "Exception in dealer: " << e.what() << "\n";
    }

    // co_return;
}


int main(int argc, char *argv[])
{
    try
    {

        int m; // number of users
        int k; // number of features
        int n; // number of items
        int modValue = 64;
        // cout << "Enter number of users :" cin >> m;
        // cout << "\n Enter number of items :" cin >> n;
        // cout << "\n Enter number of features :" cin >> k;

        cout << "loading matrix \n";

        vector<vector<int>> U0 = loadMatrix("U_ShareMatrix0.txt");
        vector<vector<int>> U1 = loadMatrix("U_ShareMatrix1.txt");
        vector<vector<int>> V0 = loadMatrix("V_ShareMatrix0.txt");
        vector<vector<int>> V1 = loadMatrix("V_ShareMatrix1.txt");

        if (U0.empty() || U1.empty() || V0.empty() || V1.empty())
        {
            cerr << "Error loading matrices.\n";
            return -1;
        }

        m = U0.size();    // number of users
        k = U0[0].size(); // number of features
        n = V0.size();    // number of items
        // int modValue = 64;

        int numQueries;
        vector<pair<int, int>> queries;
        cout << "Enter number of Queries: ";
        cin >> numQueries;
        cout << "\n Enter Queries: ";
        for (int i = 0; i < numQueries; i++)
        {
            int a, b;
            cin >> a >> b;
            queries.push_back({a, b});
        }

        // int numQueries = atoi(argv[1]);
        // if (argc < 2)
        // {
        //     cerr << "Usage: " << argv[0] << " numQueries i1 j1 i2 j2 ...\n";
        //     return -1;
        // }
        // for (int q = 0; q < numQueries; ++q)
        // {
        //     int i = atoi(argv[2 + 2 * q]);
        //     int j = atoi(argv[2 + 2 * q + 1]);
        //     queries.emplace_back(i, j);
        // }

        boost::asio::io_context io_context;
        for (auto &qpair : queries)
        {
            int query_i = qpair.first;  // user index
            int query_j = qpair.second; // item index

            cout << "Processing query (" << query_i << ", " << query_j << ")\n";

            co_spawn(io_context, [&]() -> awaitable<void>
                     {
                 co_await sendAndRcvFromParties(io_context, n, k, modValue, query_i, query_j);
                 co_return; }, boost::asio::detached);
            io_context.run();
            io_context.restart();
        }
    }
    catch (exception &e)
    {
        cerr << "Coordinator exception: " << e.what() << "\n";
    }

    return 0;
}