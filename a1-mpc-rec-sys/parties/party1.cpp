#include <utility>
#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/asio.hpp>
#include "../headerFiles/mpcOperations.h"
#include "../common.hpp"

using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;

using namespace std;
// Coroutine to receive all shares and triplets from dealer
awaitable<void> receiveSharesFromDealer(
    tcp::socket &socket,
    int &n, int &k, int &modValue,
    vector<int> &AVShare1, vector<vector<int>> &BMShare1, vector<int> &CVShare1,
    vector<int> &vectorA1, vector<int> &vectorB1, int &scalarC1,
    vector<int> &AShare1, int &bShare1, vector<int> &CShare1,
    vector<int> &eShare1,
    int &one1, int &query_i)
{

    co_await recv_int(socket, n);
    cout << "Party0: received n = " << n << "\n"
         << flush;

    co_await recv_int(socket, k);
    cout << "Party0: received k = " << k << "\n"
         << flush;

    co_await recv_int(socket, modValue);
    cout << "Party0: received modValue = " << modValue << "\n"
         << flush;

    // matrix-vector triplet
    co_await recvVector(socket, AVShare1);
    co_await recvMatrix(socket, BMShare1);
    co_await recvVector(socket, CVShare1);
    cout << "Party0: received C0 size = " << CVShare1.size() << "\n"
         << flush;

    AVShare1.resize(n);
    BMShare1.resize(n, vector<int>(k));

    vectorA1.resize(k);
    vectorB1.resize(k);

    AShare1.resize(k);
    CShare1.resize(k);

    eShare1.resize(n);

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

    co_return;
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
        // boost::asio::io_context io_context;
        cout << "hello from inside fun party1";
        // connect to dealer
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", "9002");
        co_await boost::asio::async_connect(socket, endpoints, use_awaitable);
        // boost::asio::connect(socket, endpoints);

        cout << "Party1: connected to dealer\n";

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

        // receive all shares from dealer
        co_await receiveSharesFromDealer(socket, n, k, modValue, AVShare1, BMShare1, CVShare1, vectorA1, vectorB1, scalarC1, AShare1, bShare1, CShare1, eShare1, one1, query_i);
        // boost::asio::co_spawn(io_context,
        //                       receiveSharesFromDealer(socket, n, k, modValue, eShare1, VShare0, BB0, C0, A10, B10, c0, AShare1, CShare1, bShare1, one1),
        //                       boost::asio::detached);

        // io_context.run();

        cout << "Party1: received all shares, ready for MPC computation\n";
        int i = query_i;

        // Load party0 shares
        vector<vector<int>> U1 = loadMatrix("U_ShareMatrix0.txt");
        vector<vector<int>> V1 = loadMatrix("V_ShareMatrix0.txt");

        size_t m = U1.size();

        tcp::socket peer_socket(io_context);
        auto peer_endpoints = resolver.resolve("127.0.0.1", "9003"); // listening
        co_await async_connect(peer_socket, peer_endpoints, use_awaitable);
        // boost::asio::connect(peer_socket, peer_endpoints);
        cout << "Party1: connected to Party0\n";

        // send and reciever blinds

        vector<int> blindV0(n), blindV1(n);
        vector<vector<int>> blindM0(n, vector<int>(k)), blindM1(n, vector<int>(k)); // get blindM1 from other party

        for (size_t it = 0; it < n; it++)
        {
            blindV1[it] = (eShare1[it] + AVShare1[it]) % modValue;
        }

        for (size_t it = 0; it < n; it++)
            for (size_t j = 0; j < k; j++)
                blindM1[it][j] = (V1[it][j] + BMShare1[it][j]) % modValue;

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
        vector<int> V1j = mpcVectorandMatrixMul(
            alphaV,
            V1,
            betaM,
            AVShare1,
            CShare1,
            modValue);

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
        int UdotV1 = mpcDotProduct(alpha, vectorB1, beta, vectorA1, scalarC1, modValue);

        // compute share of 1-<ui,vj>
        int sub1 = (one1 - UdotV1) % modValue;
        sub1 = sub1 < 0 ? sub1 + modValue : sub1;

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

        // now calculate ui=ui+vj(1-<ui,vj>)
        for (size_t it = 0; it < k; it++)
        {
            U1i[it] = (U1i[it] + mul1[it]) % modValue;
            cout << U1i[it] << " ";
        }
        U1[i] = U1i;
       cout<<"Party0: updated user vector share U0i\n";
        saveMatrix("U_ShareMatrix0.txt", U1);
        // sent share back to client

        co_await sendFinalSharesToDealer(socket, U1i);
    }
    catch (exception &e)
    {
        cerr << "Party0 exception: " << e.what() << "\n";
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