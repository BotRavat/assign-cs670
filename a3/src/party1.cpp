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
    vector<int> &AVShare1, vector<vector<int>> &BMShare1, vector<int> &CVShare1,
    vector<int> &vectorA1, vector<int> &vectorB1, int &scalarC1,
    vector<int> &AShare1, int &bShare1, vector<int> &CShare1,
    vector<int> &eShare1,
    int &one0, int &query_i, u128 rootSeed, vector<bool> &T1, vector<u128> &CW, vector<u128> &FCW1)

{

    int first;
    co_await recv_int(socket, first);
    if (first == -1)
        co_return false; // DONE

    n = first;

    // co_await recv_int(socket, n);
    // cout << "Party1: received n = " << n << "\n"
    //      << flush;

    co_await recv_int(socket, k);
    cout << "Party0: received k = " << k << "\n"
         << flush;

    co_await recv_int(socket, modValue);
    cout << "Party0: received modValue = " << modValue << "\n"
         << flush;

    AVShare1.resize(n);
    BMShare1.resize(n, vector<int>(k));

    vectorA1.resize(k);
    vectorB1.resize(k);

    AShare1.resize(k);
    CShare1.resize(k);

    eShare1.resize(n);

    T1.resize(2 * n - 1);
    CW.resize((int)(ceil(log2(n))) + 1);
    FCW1.resize(k);

    // matrix-vector triplet
    co_await recvVector(socket, AVShare1);
    co_await recvMatrix(socket, BMShare1);
    co_await recvVector(socket, CVShare1);
    cout << "Party0: received C0 size = " << CVShare1.size() << "\n"
         << flush;

    // vector dot product triplet
    // vector triplet for U-V computation
    co_await recvVector(socket, vectorA1);
    co_await recvVector(socket, vectorB1);
    co_await recv_int(socket, scalarC1);
    cout << "Party0: received A10 size = " << vectorA1.size() << "\n"
         << flush;
    cout << "Party0: received c0 = " << scalarC1 << "\n"
         << flush;

    // receive scalar-vector triplet
    co_await recvVector(socket, AShare1);
    co_await recv_int(socket, bShare1);
    co_await recvVector(socket, CShare1);
    cout << "Party0: received bShare1 = " << bShare1 << "\n"
         << flush;
    cout << "Party0: received CShare1 size = " << CShare1.size() << "\n"
         << flush;

    // e-share
    co_await recvVector(socket, eShare1);

    co_await recv_int(socket, one1);
    cout << "Party0: received one1 = " << one1 << "\n"
         << flush;
    co_await recv_int(socket, query_i);
    cout << "Party0: received query_i = " << query_i << "\n"
         << flush;

    co_await recvVector(socket, T1);
    cout << "Party1: received T0 size = " << T1.size() << "\n"
         << flush;

    co_await recvVector(socket, CW);
    cout << "Party1: received CW size = " << CW.size() << "\n"
         << flush;
    co_await recvVector(socket, FCW1);
    cout << "Party1: received FCW size = " << FCW1.size() << "\n"
         << flush;

    co_return true;
}

awaitable<void> sendFinalSharesToDealer(tcp::socket &socket, const vector<int> &U0i)
{
    co_await sendVector(socket, U0i);

    cout << "Party: sent final user vector share to dealer\n";
    co_return;
}

awaitable<void> party1(boost::asio::io_context &io_context)
{
    try
    {

        // connect to dealer
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", "9002");
        co_await boost::asio::async_connect(socket, endpoints, use_awaitable);
        cout << "Party1: connected to dealer\n";

        tcp::socket peer_socket(io_context);
        auto peer_endpoints = resolver.resolve("127.0.0.1", "9003"); // listening
        co_await async_connect(peer_socket, peer_endpoints, use_awaitable);
        // boost::asio::connect(peer_socket, peer_endpoints);
        cout << "Party1: connected to Party0\n";

        // placeholders for received data
        int n, k, modValue;

        // for vector x matrix
        vector<vector<int>> BMShare1;
        vector<int> AVShare1, CVShare1;

        // for vector dot vector
        vector<int> vectorA1, vectorB1;
        int scalarC1;

        // for scalar x vector
        vector<int> AShare1, CShare1;
        int bShare1;

        vector<int> eShare1;

        int one1, query_i;

        u128 rootSeed;
        vector<bool> T1;
        vector<u128> CW;
        vector<u128> FCW1;

        while (true)
        {

            // receive all shares of 1 query from dealer
            bool has_query = co_await receiveSharesFromDealer(
                socket, n, k, modValue,
                AVShare1, BMShare1, CVShare1,
                vectorA1, vectorB1, scalarC1,
                AShare1, bShare1, CShare1,
                eShare1, one1, query_i, rootSeed, T1, CW, FCW1);

            if (!has_query)
            {
                std::cout << "[Party1] Dealer indicated DONE. Closing.\n";
                break;
            }

            std::cout << "[Party1] Processing query for i=" << query_i << "\n";

            int i = query_i;

            // Load party1 shares
            vector<vector<int>> U1 = loadMatrix("U_ShareMatrix1.txt");
            vector<vector<int>> V1 = loadMatrix("V_ShareMatrix1.txt");

            if (U1.empty() || V1.empty())
            {
                std::cerr << "[Party1] Failed to load matrices.\n";
                co_return;
            }

            size_t m = U1.size();

            // send and reciever blinds

            vector<int> blindV0(n), blindV1(n);
            vector<vector<int>> blindM0(n, vector<int>(k)), blindM1(n, vector<int>(k)); // get blindM1 from other party

            for (size_t it = 0; it < n; it++)
            {
                blindV1[it] = (eShare1[it] - AVShare1[it]) % modValue;
                if (blindV1[it] < 0)
                    blindV1[it] += modValue;
            }

            for (size_t it = 0; it < n; it++)
                for (size_t j = 0; j < k; j++)
                {
                    blindM1[it][j] = (V1[it][j] - BMShare1[it][j]) % modValue;
                    if (blindM1[it][j] < 0)
                        blindM1[it][j] += modValue;
                }

            co_await sendVector(peer_socket, blindV1);
            co_await recvVector(peer_socket, blindV0);
            co_await sendMatrix(peer_socket, blindM1);
            co_await recvMatrix(peer_socket, blindM0);

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
            // vector<int> V1j = mpcVectorandMatrixMul(
            //     alphaV,
            //     V1,
            //     betaM,
            //     AVShare1,
            //     CShare1,
            //     modValue);

            vector<int> V1j = mpcVectorandMatrixMulBeaver(
                alphaV,
                BMShare1,
                betaM,
                AVShare1,
                CVShare1,
                false,
                modValue);

            cout << "Party1: computed share of Vj\n";
            for (int it = 0; it < V1j.size(); it++)
            {
                cout << V1j[it] << " ";
            }
            cout << endl;
            // take ith row of matrix user
            vector<int> U1i = U1[i];
            /*

                // finally we have both shares of both ith row of user- U0i and jth row of item V0j
              U0i and V0j

            */

            // get beavers triplet for vectors from dealer
            // vectorA1,vectorB1,scalarC1

            // blinded value by party S1
            // UA0=U1[i]+A0, VB0=V1[i]+B0   // vector addition
            vector<int> UA1(k), VB1(k);
            for (size_t it = 0; it < k; it++)
            {
                UA1[it] = (U1i[it] + vectorA1[it]) % modValue;
                VB1[it] = (V1j[it] + vectorB1[it]) % modValue;
            }

            // get blinded values from party S1
            vector<int> UA0, VB0; // U1[i]+A1,V1[j]+B1

            co_await sendVector(peer_socket, UA1);
            co_await recvVector(peer_socket, UA0);
            co_await sendVector(peer_socket, VB1);
            co_await recvVector(peer_socket, VB0);

            // calculate alpha, beta
            vector<int> alpha(k), beta(k);
            // apha=x0+x1+a0+a1
            for (size_t it = 0; it < k; it++)
            {
                alpha[it] = (UA0[it] + UA1[it]) % modValue;
                beta[it] = (VB0[it] + VB1[it]) % modValue;
            }

            // compute share of mpc dot product of <ui,vi>
            int UdotV1 = mpcDotProduct(
                alpha,
                V1j, //  vectorB1,
                beta,
                vectorA1,
                scalarC1,
                modValue);

            cout << "Dot product is : " << UdotV1 << endl;

            // compute share of 1-<ui,vj>
            int sub1 = (one1 - UdotV1) % modValue;
            sub1 = sub1 < 0 ? sub1 + modValue : sub1;
            cout << "1- dot is : " << sub1 << endl;

            // to compute vj(1- <ui,vj)

            // get beaver triplet of scalar and vector

            // sent blinded value to party S1 ( V1[j]+AShare1, UdotV0+bShare1)
            // blinded_vec,blindV0A0= V1[j]+AShare1;  blinded_scalar,UdotV0BS0=UdotV0+bShare1

            vector<int> blindSV1(k);
            for (size_t it = 0; it < k; it++)
            {
                blindSV1[it] = (V1j[it] + AShare1[it]) % modValue;
            }
            int blind_scalar1 = (sub1 + bShare1) % modValue;

            // get blinded values from party S1
            vector<int> blindSV0(k);
            int blind_scalar0;
            co_await sendVector(peer_socket, blindSV1);
            co_await recvVector(peer_socket, blindSV0);
            co_await send_int(peer_socket, blind_scalar1, "blind_scalar0");
            co_await recv_int(peer_socket, blind_scalar0);
            // now calculate alpha and beta for vector mul scalar

            vector<int> alphaSV(k);
            int betaS;
            for (size_t it = 0; it < k; it++)
            {
                alphaSV[it] = (blindSV0[it] + blindSV1[it]) % modValue;
            }
            betaS = (blind_scalar0 + blind_scalar1) % modValue;
            vector<int> mul1 = mpcVectorandScalarMul(
                alphaSV,
                sub1,
                betaS,
                AShare1,
                CShare1,
                modValue);

            cout << "mul is : ";
            for (int it = 0; it < mul1.size(); it++)
            {
                cout << mul1[it] << " ";
            }
            cout << endl;

            vector<u128> M0;
            M0.reserve(mul1.size());
            for (int v : mul1)
            {
                M0.push_back(static_cast<u128>(v));
            }

            int dpf_size = pow(2, k);
            vector<vector<u128>> result = evalDPF(peer_socket, rootSeed, T1, CW, dpf_size, FCW1, M1);

            cout << "Party0: updated user vector share U0i\n";
            // saveMatrix("U_ShareMatrix0.txt", U1);
            // sent share back to client

            co_await sendFinalSharesToDealer(socket, U1i);
        }
        socket.close();
        peer_socket.close();
        cout << "Party1: Closed all connections.\n";
    }

    catch (exception &e)
    {
        cerr << "Party1 exception: " << e.what() << "\n";
    }

    co_return;
}

int main()
{
    try
    {
        boost::asio::io_context io_context;
        boost::asio::co_spawn(io_context, party1(io_context), boost::asio::detached);
        io_context.run();
    }
    catch (std::exception &e)
    {
        std::cerr << "Party1 exception: " << e.what() << "\n";
    }
    return 0;
}