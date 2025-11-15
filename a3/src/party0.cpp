#include <utility>
#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/asio.hpp>
#include "../headerFiles/mpcOperations.h"
#include "../common.hpp"
#include "dpf.hpp"

using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
using namespace std;
using u128 = unsigned __int128;

// Coroutine to receive all shares and triplets from dealer
awaitable<bool> receiveSharesFromDealer(
    tcp::socket &socket,
    int &n, int &k, int &modValue,
    vector<int> &AVShare0, vector<vector<int>> &BMShare0, vector<int> &CVShare0,
    vector<int> &vectorA0, vector<int> &vectorB0, int &scalarC0,
    vector<int> &AShare0, int &bShare0, vector<int> &CShare0,
    vector<int> &eShare0,
    int &one0, int &query_i, u128 rootSeed, vector<bool> &T0, vector<u128> &CW, vector<u128> &FCW0)
{

    int first;
    co_await recv_int(socket, first);
    if (first == -1)
        co_return false; // DONE

    n = first;

    // co_await recv_int(socket, n);
    // cout << "Party0: received n = " << n << "\n"
    //      << flush;

    co_await recv_int(socket, k);
    cout << "Party0: received k = " << k << "\n"
         << flush;

    co_await recv_int(socket, modValue);
    cout << "Party0: received modValue = " << modValue << "\n"
         << flush;

    AVShare0.resize(n);
    BMShare0.resize(n, vector<int>(k));

    vectorA0.resize(k);
    vectorB0.resize(k);

    AShare0.resize(k);
    CShare0.resize(k);

    eShare0.resize(n);

    T0.resize(2 * n - 1);
    CW.resize((int)(ceil(log2(n))) + 1);
    FCW0.resize(k);

    // matrix-vector triplet
    co_await recvVector(socket, AVShare0);
    co_await recvMatrix(socket, BMShare0);
    co_await recvVector(socket, CVShare0);
    cout << "Party0: received C0 size = " << CVShare0.size() << "\n"
         << flush;

    // vector dot product triplet
    // vector triplet for U-V computation
    co_await recvVector(socket, vectorA0);
    co_await recvVector(socket, vectorB0);
    co_await recv_int(socket, scalarC0);
    cout << "Party0: received A10 size = " << vectorA0.size() << "\n"
         << flush;
    cout << "Party0: received c0 = " << scalarC0 << "\n"
         << flush;

    // receive scalar-vector triplet
    co_await recvVector(socket, AShare0);
    co_await recv_int(socket, bShare0);
    co_await recvVector(socket, CShare0);
    cout << "Party0: received bShare0 = " << bShare0 << "\n"
         << flush;
    cout << "Party0: received CShare0 size = " << CShare0.size() << "\n"
         << flush;

    // e-share
    co_await recvVector(socket, eShare0);

    co_await recv_int(socket, one0);
    cout << "Party0: received one0 = " << one0 << "\n"
         << flush;
    co_await recv_int(socket, query_i);
    cout << "Party0: received query_i = " << query_i << "\n"
         << flush;

    co_await recv_int(socket, rootSeed);
    cout << "Party0: received rootSeed\n"
         << flush;

    co_await recvVector(socket, T0);
    cout << "Party0: received T0 size = " << T0.size() << "\n"
         << flush;

    co_await recvVector(socket, CW);
    cout << "Party0: received CW size = " << CW.size() << "\n"
         << flush;
    co_await recvVector(socket, FCW0);
    cout << "Party0: received FCW size = " << FCW0.size() << "\n"
         << flush;

    co_return true;
}

awaitable<void> sendFinalSharesToDealer(tcp::socket &socket, const vector<int> &U0i)
{
    co_await sendVector(socket, U0i);

    cout << "Party: sent final user vector share to dealer\n";
    co_return;
}

awaitable<void> party0(boost::asio::io_context &io_context)
{
    try
    {
        // boost::asio::io_context io_context;
        cout << "hello from inside fun party0";
        // connect to dealer
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", "9002");
        co_await boost::asio::async_connect(socket, endpoints, use_awaitable);
        // boost::asio::connect(socket, endpoints);

        cout << "Party0: connected to dealer\n";

        tcp::socket peer_socket(io_context);
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 9003));

        cout << "Party0: waiting for Party1 to connect...\n";
        co_await acceptor.async_accept(peer_socket, use_awaitable);
        // acceptor.accept(peer_socket);
        cout << "Party0: Party1 connected\n";

        // placeholders for received data
        int n, k, modValue;

        // for vector x matrix
        vector<vector<int>> BMShare0;
        vector<int> AVShare0, CVShare0;

        // for vector dot vector
        vector<int> vectorA0, vectorB0;
        int scalarC0;

        // for scalar x vector
        vector<int> AShare0, CShare0;
        int bShare0;

        vector<int> eShare0;

        int one0, query_i;

        u128 rootSeed;
        vector<bool> T0;
        vector<u128> CW;
        vector<u128> FCW0;

        while (true)
        {
            // receive all shares of 1 query from dealer

            bool has_query = co_await receiveSharesFromDealer(
                socket, n, k, modValue,
                AVShare0, BMShare0, CVShare0,
                vectorA0, vectorB0, scalarC0,
                AShare0, bShare0, CShare0,
                eShare0, one0, query_i, rootSeed, T0, CW, FCW0);

            if (!has_query)
            {
                std::cout << "[Party0] Dealer indicated DONE. Closing.\n";
                break;
            }

            std::cout << "[Party0] Processing query for i=" << query_i << "\n";

            int i = query_i;

            // Load party0 shares
            vector<vector<int>> U0 = loadMatrix("U_ShareMatrix0.txt");
            vector<vector<int>> V0 = loadMatrix("V_ShareMatrix0.txt");

            if (U0.empty() || V0.empty())
            {
                std::cerr << "[Party0] Failed to load matrices.\n";
                co_return;
            }

            size_t m = U0.size();

            // send and reciever blinds

            vector<int> blindV0(n), blindV1(n);
            vector<vector<int>> blindM0(n, vector<int>(k)), blindM1(n, vector<int>(k)); // get blindM1 from other party

            for (size_t it = 0; it < n; it++)
            {
                blindV0[it] = (eShare0[it] - AVShare0[it]) % modValue;
                if (blindV0[it] < 0)
                    blindV0[it] += modValue;
            }

            for (size_t it = 0; it < n; it++)
                for (size_t j = 0; j < k; j++)
                {

                    blindM0[it][j] = (V0[it][j] - BMShare0[it][j]) % modValue;
                    if (blindM0[it][j] < 0)
                        blindM0[it][j] += modValue;
                }

            co_await sendVector(peer_socket, blindV0);
            co_await recvVector(peer_socket, blindV1);
            co_await sendMatrix(peer_socket, blindM0);
            co_await recvMatrix(peer_socket, blindM1);

            cout << "Party0: received blinds from Party1\n";
            // compute alphaM, betaM
            vector<int> alphaV(n);
            vector<vector<int>> betaM(n, vector<int>(k));
            for (size_t it = 0; it < n; it++)
            {
                alphaV[it] = (blindV0[it] + blindV1[it]) % modValue;
                for (size_t j = 0; j < k; j++)
                    betaM[it][j] = (blindM0[it][j] + blindM1[it][j]) % modValue;
            }

            // get the share of row vi
            // vector<int> V0j = mpcVectorandMatrixMulBeaver(
            //     alphaV,
            //     V0,
            //     betaM,
            //     AVShare0,
            //     CShare0,
            //     modValue);

            vector<int> V0j = mpcVectorandMatrixMulBeaver(
                alphaV,
                BMShare0,
                betaM,
                AVShare0,
                CVShare0,
                true,
                modValue);

            cout << "Party0: computed share of Vj\n";
            for (int it = 0; it < V0j.size(); it++)
            {
                cout << V0j[it] << " ";
            }
            cout << endl;
            // take ith row of matrix user
            vector<int> U0i = U0[i];
            /*

                // finally we have both shares of both ith row of user- U0i and jth row of item V0j
              U0i and V0j

            */

            // get beavers triplet for vectors from dealer
            // vectorA0,vectorB0,scalarC0

            // blinded value by party S1
            // UA0=U0[i]+A0, VB0=V0[i]+B0   // vector addition
            vector<int> UA0(k), VB0(k);
            for (size_t it = 0; it < k; it++)
            {
                UA0[it] = (U0i[it] + vectorA0[it]) % modValue;
                VB0[it] = (V0j[it] + vectorB0[it]) % modValue;
            }

            // get blinded values from party S1
            vector<int> UA1, VB1; // U1[i]+A1,V1[j]+B1

            co_await sendVector(peer_socket, UA0);
            co_await recvVector(peer_socket, UA1);
            co_await sendVector(peer_socket, VB0);
            co_await recvVector(peer_socket, VB1);

            cout << "Party0: received blinded vectors from Party1\n";
            // calculate alpha, beta
            vector<int> alpha(k), beta(k);
            // apha=x0+x1+a0+a1
            for (size_t it = 0; it < k; it++)
            {
                alpha[it] = (UA0[it] + UA1[it]) % modValue;
                beta[it] = (VB0[it] + VB1[it]) % modValue;
            }

            // compute share of mpc dot product of <ui,vi>
            int UdotV0 = mpcDotProduct(
                alpha,
                V0j, // vectorB0,
                beta,
                vectorA0,
                scalarC0,
                modValue);

            cout << "Party0: computed share of dot product <ui,vj>\n";
            cout << "Dot product is : " << UdotV0 << endl;
            // compute share of 1-<ui,vj>
            int sub0 = (one0 - UdotV0) % modValue;
            sub0 = sub0 < 0 ? sub0 + modValue : sub0;

            cout << "1- dot is : " << sub0 << endl;

            // to compute vj(1- <ui,vj)

            // get beaver triplet of scalar and vector

            // sent blinded value to party S1 ( V0[j]+AShare0, UdotV0+bShare0)
            // blinded_vec,blindV0A0= V0[j]+AShare0;  blinded_scalar,UdotV0BS0=UdotV0+bShare0

            vector<int> blindSV0(k);
            for (size_t it = 0; it < k; it++)
            {
                blindSV0[it] = (V0j[it] + AShare0[it]) % modValue;
            }
            int blind_scalar0 = (sub0 + bShare0) % modValue;

            // get blinded values from party S1
            vector<int> blindSV1;
            int blind_scalar1;
            co_await sendVector(peer_socket, blindSV0);
            co_await recvVector(peer_socket, blindSV1);
            co_await send_int(peer_socket, blind_scalar0, "blind_scalar0");
            co_await recv_int(peer_socket, blind_scalar1);
            // now calculate alpha and beta for vector mul scalar

            cout << "Party0: received blinded scalar and vector from Party1\n";
            vector<int> alphaSV(k);
            int betaS;
            for (size_t it = 0; it < k; it++)
            {
                alphaSV[it] = (blindSV0[it] + blindSV1[it]) % modValue;
            }
            betaS = (blind_scalar0 + blind_scalar1) % modValue;
            vector<int> mul0 = mpcVectorandScalarMul(
                alphaSV,
                sub0,
                betaS,
                AShare0,
                CShare0,
                modValue);
            cout << "mul is : ";
            for (int it = 0; it < mul0.size(); it++)
            {
                cout << mul0[it] << " ";
            }
            cout << endl;

            // now we will use dpf
            vector<u128> M0;
            M0.reserve(mul0.size());
            for (int v : mul0)
            {
                M0.push_back(static_cast<u128>(v));
            }

            int dpf_size = pow(2, k);
            vector<vector<u128>> result = evalDPF(peer_socket, rootSeed, T0, CW, dpf_size, FCW0, M0);

            //         vector<vector<u128>> evalDPF(u128 rootSeed, vector<bool> &T, vector<u128> &CW, int dpf_size, vector<u128> &FCW, vector<u128> &M)
            // {
            // int height = (int)(ceil(log2(dpf_size)));
            // int leaves = 1 << (height);
            // int totalNodes = 2 * leaves - 1;
            // int lastParentIdx = (totalNodes - 2) / 2;
            // vector<u128> VShare(totalNodes);
            // VShare[0] = rootSeed;
            // vector<vector<u128>> result(leaves);

            // // generate tree using provided key {V,T,CW,dpf_size}
            // for (int l = 0; l < height; l++)
            // {

            //     int lvlStrt = (1 << l) - 1;
            //     int nodesAtLevel = 1 << l;
            //     int childLevel = l + 1;

            //     // generate childrens
            //     for (int i = 0; i < nodesAtLevel; i++)
            //     {
            //         int parentIdx = lvlStrt + i;
            //         int leftChildIdx = 2 * parentIdx + 1;
            //         int rightChildIdx = 2 * parentIdx + 2;

            //         pair<u128, u128> childSeed = prg2(VShare[parentIdx]);
            //         VShare[leftChildIdx] = childSeed.first;   // left child
            //         VShare[rightChildIdx] = childSeed.second; // right child

            //         // apply correction word to nodes
            //         if (T0[parentIdx])
            //         {
            //             VShare[leftChildIdx] ^= CW[childLevel];
            //             VShare[rightChildIdx] ^= CW[childLevel];
            //         }
            //     }
            // }

            // // modify fcw vector and echange with other party
            // int rowLength = M0.size();
            // vector<u128> P0(rowLength), P1(rowLength);
            // for (int i = 0; i < rowLength; i++)
            // {
            //     P0[i] = FCW0[i] - M0[i];
            // }

            // co_await sendVector(peer_socket, P0);
            // co_await recvVector(peer_socket, P1);
            // vector<u128> fcwm(rowLength);

            // for (int i = 0; i < rowLength; i++)
            // {
            //     fcwm[i] = P0[i] + P1[i];
            // }

            // // Apply final correction word to target leaf
            // int leafStart = (1 << (height)) - 1;
            // for (int i = 0; i < dpf_size; i++)
            // {

            //     vector<u128> currLeaf(rowLength);
            //     // result[i] = VShare[leafStart + i];
            //     currLeaf = prgVector(VShare[i + leafStart], rowLength);
            //     for (int j = 0; j < rowLength; j++)
            //         if (T0[leafStart + i])
            //         {
            //             currLeaf[j] += fcwm[j];
            //         }

            //     result.push_back(currLeaf);
            // }

            //     return result;
            // }

            cout << "Party0: updated user vector share U0i\n";
            // saveMatrix("U_ShareMatrix0.txt", U0);
            // sent share back to client

            co_await sendFinalSharesToDealer(socket, U0i);
        }
        socket.close();
        peer_socket.close();
        cout << "Party0: Closed all connections.\n";
    }
    catch (exception &e)
    {
        cerr << "Party0 exception: " << e.what() << "\n";
    }

    co_return;
}

int main()
{
    cout << "hello from  party0 from main";

    try
    {
        boost::asio::io_context io_context;
        co_spawn(io_context, party0(io_context), boost::asio::detached);
        io_context.run();
    }
    catch (exception &e)
    {
        cerr << "Party0 exception: " << e.what() << "\n";
    }
    return 0;
}