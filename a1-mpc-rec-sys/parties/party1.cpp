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
    vector<int> &eShare1,
    vector<vector<int>> &VShare1,
    vector<int> &B1, vector<int> &C1,
    vector<int> &A11, vector<int> &B11, int &c1,
    vector<int> &AShare1, vector<int> &CShare1, int &bShare1,
    int &one1, int &query_i)
{
    co_await recv_int(socket, n);
    co_await recv_int(socket, k);
    co_await recv_int(socket, modValue);
    cout << "Party0: received one0 = " << modValue << "\n"
              << flush;

    eShare1.resize(n);
    VShare1.resize(n, vector<int>(k));
    B1.resize(n);
    C1.resize(n);
    A11.resize(k);
    B11.resize(k);
    AShare1.resize(k);
    CShare1.resize(k);

    // receive scalar-vector triplet
    co_await recvVector(socket, AShare1);
    co_await recv_int(socket, bShare1);
    co_await recvVector(socket, CShare1);

    // vector dot product triplet
    // additional vector triplet for U-V computation
    co_await recvVector(socket, A11);
    co_await recvVector(socket, B11);
    co_await recv_int(socket, c1);
    cout << "Party0: received c1 = " << c1 << "\n"
              << flush;

    // e-share
    co_await recvVector(socket, eShare1);

    // matrix-vector triplet
    co_await recvMatrix(socket, VShare1);
    co_await recvVector(socket, B1);
    co_await recvVector(socket, C1);

    co_await recv_int(socket, one1);
    cout << "Party0: received one1 = " << one1 << "\n"
              << flush;
    co_await recv_int(socket, query_i);
    cout << "Party0: received query_i = " << query_i << "\n"
              << flush;
    co_return;
}

awaitable<void> sendFinalSharesToDealer(tcp::socket &socket, const vector<int> &U1i)
{
    co_await sendVector(socket, U1i);

    cout << "Party: sent final user vector share to dealer\n";
    co_return;
}

vector<vector<int>> loadMatrix(const string &filename)
{
    ifstream inFile(filename);
    if (!inFile)
    {
        cerr << "Error opening file for reading: " << filename << endl;
        return {};
    }

    vector<vector<int>> matrix;
    string line;

    while (getline(inFile, line))
    {
        stringstream ss(line);
        int val;
        vector<int> row;
        while (ss >> val)
        {
            row.push_back(val);
        }
        if (!row.empty())
            matrix.push_back(row);
    }

    inFile.close();
    return matrix;
}
void saveMatrix(const string &filename, const vector<vector<int>> &matrix)
{
    ofstream outFile(filename);
    if (!outFile)
    {
        cerr << "Error opening file for writing: " << filename << endl;
        return;
    }

    for (const auto &row : matrix)
    {
        for (size_t i = 0; i < row.size(); ++i)
        {
            outFile << row[i];
            if (i != row.size() - 1)
                outFile << " ";
        }
        outFile << "\n";
    }
    outFile.close();
    cout << "Matrix saved successfully to " << filename << "\n";
}
awaitable<void> party1(boost::asio::io_context &io_context)
{
    try
    {
        // boost::asio::io_context io_context;

        // boost::asio::steady_timer timer(io_context, chrono::milliseconds(500));
        // co_await timer.async_wait(use_awaitable);

        // connect to dealer
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", "9002");
        co_await boost::asio::async_connect(socket, endpoints, use_awaitable);
        // boost::asio::connect(socket, endpoints);

        cout << "Party1: connected to dealer\n";

        // placeholders for received data
        int n, k, modValue;
        vector<int> eShare1;
        vector<vector<int>> VShare1;
        vector<int> B1, C1;
        vector<int> A11, B11;
        int c1;
        vector<int> AShare1, CShare1;
        int bShare1, one1, query_i;

        // receive all shares from dealer
        co_await receiveSharesFromDealer(socket, n, k, modValue, eShare1, VShare1, B1, C1, A11, B11, c1, AShare1, CShare1, bShare1, one1, query_i);

        // io_context.run();

        cout << "Party1: received all shares, ready for MPC computation\n";

        int i = query_i;

        // Load party0 shares
        vector<vector<int>> U1 = loadMatrix("U_ShareMatrix1.txt");
        vector<vector<int>> V1 = loadMatrix("V_ShareMatrix1.txt");

        size_t m = U1.size();

        tcp::socket peer_socket(io_context);
        auto peer_endpoints = resolver.resolve("127.0.0.1", "9003"); // listening
        co_await async_connect(peer_socket, peer_endpoints, use_awaitable);
        // boost::asio::connect(peer_socket, peer_endpoints);
        cout << "Party1: connected to Party0\n";

        // send and reciever blinds
        vector<vector<int>> blindM0, blindM1(n, vector<int>(k)); // get blindM0 from other party
        for (size_t it = 0; it < n; it++)
        {
            for (size_t j = 0; j < k; j++)
                blindM1[it][j] = (V1[it][j] + VShare1[it][j]) % modValue;
        }
        co_await recvMatrix(peer_socket, blindM0);
        co_await sendMatrix(peer_socket, blindM1);

        vector<int> blindV0, blindV1(n);
        for (size_t it = 0; it < n; it++)
        {
            blindV1[it] = (eShare1[it] + B1[it]) % modValue;
        }
        co_await recvVector(peer_socket, blindV0);
        co_await sendVector(peer_socket, blindV1);

        // compute alphaM, betaV
        vector<vector<int>> alphaM(n, vector<int>(k));
        vector<int> betaV(n);
        for (size_t it = 0; it < n; it++)
        {
            betaV[it] = (blindV0[it] + blindV1[it]) % modValue;
            for (size_t j = 0; j < k; j++)
                alphaM[it][j] = (blindM0[it][j] + blindM1[it][j]) % modValue;
        }

        // get the share of row vi and compute V1j
        vector<int> V1j = mpcVectorandMatrixMul(
            alphaM,
            B1,
            betaV,
            VShare1,
            C1,
            1,
            modValue);

        // take ith row of matrix user
        vector<int> U1i = U1[i];
        /*

            // finally we have both shares of both ith row of user- U0i and jth row of item V0j
          U0i and V0j

        */

        // get beavers triplet for vectors from dealer

        // sent blinded value to party S1
        // UA0=U0[i]+A0, VB0=V0[i]+B0   // vector addition
        vector<int> UA1(k), VB1(k);
        for (size_t it = 0; it < k; it++)
        {
            UA1[it] = (U1i[it] + A11[it]) % modValue;
            VB1[it] = (V1j[it] + B11[it]) % modValue;
        }

        // get blinded values f1om party S1
        vector<int> UA0, VB0; // U1[i]+A1,V1[j]+B1

        co_await recvVector(peer_socket, UA0);
        co_await sendVector(peer_socket, UA1);
        co_await recvVector(peer_socket, VB0);
        co_await sendVector(peer_socket, VB1);

        // calculate alpha, beta
        vector<int> alpha(k), beta(k);
        // apha=x0+x1+a0+a1
        for (size_t it = 0; it < k; it++)
        {
            alpha[it] = (UA0[it] + UA1[it]) % modValue;
            beta[it] = (VB0[it] + VB1[it]) % modValue;
        }

        // compute share of mpc dot product of <ui,vi>
        int UdotV1 = mpcDotProduct(alpha, B11, beta, A11, c1, 1, modValue);

        // compute share of 1-<ui,vj>
        int sub1 = (one1 - UdotV1) % modValue;
        sub1 = sub1 < 0 ? sub1 + modValue : sub1;

        // to compute vj(1- <ui,vj)

        // get beaver triplet of scalar and vector

        // sent blinded value to party S1 ( V0[j]+AShare0, UdotV0+bShare0)
        // blinded_vec,blindV0A0= V0[j]+AShare0;  blinded_scalar,UdotV0BS0=UdotV0+bShare0

        vector<int> blindSV1(k);
        for (size_t it = 0; it < k; it++)
        {
            blindSV1[it] = (V1j[it] + AShare1[it]) % modValue;
        }
        int blind_scalar1 = (sub1 + bShare1) % modValue;

        // get blinded values from party S1
        vector<int> blindSV0;
        int blind_scalar0;
        co_await recvVector(peer_socket, blindSV0);
        co_await sendVector(peer_socket, blindSV1);
        co_await recv_int(peer_socket, blind_scalar0);
        co_await send_int(peer_socket, blind_scalar1, "blind_scalar1");
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
            bShare1,
            betaS,
            AShare1,
            CShare1,
            1,
            modValue);

        // now calculate ui=ui+vj(1-<ui,vj>)
        for (size_t it = 0; it < k; it++)
        {
            U1i[it] = (U1i[it] + mul1[it]) % modValue;
        }


          U1[i] = U1i;
        saveMatrix("U_ShareMatrix1.txt",U1);

        // sent share back to client
        co_await sendFinalSharesToDealer(socket, U1i);

        // boost::asio::co_spawn(io_context,
        //                       sendFinalSharesToDealer(socket, U1i),
        //                       boost::asio::detached);
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
    catch (exception &e)
    {
        cerr << "Party1 exception: " << e.what() << "\n";
    }
    return 0;
}