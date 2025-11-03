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
    vector<int> &AVShare0, vector<vector<int>> &BMShare0, vector<int> &CVShare0,
    vector<int> &vectorA0, vector<int> &vectorB0, int &scalarC0,
    vector<int> &AShare0, int &bShare0, vector<int>& CShare0,
    vector<int> &eShare0,
    int &one0, int &query_i)
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

    AVShare0.resize(n);
    BMShare0.resize(n, vector<int>(k));

    vectorA0.resize(k);
    vectorB0.resize(k);

    AShare0.resize(k);
    CShare0.resize(k);

    eShare0.resize(n);

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

    co_return;
}

awaitable<void> sendFinalSharesToDealer(tcp::socket &socket, const vector<int> &U0i)
{
    co_await sendVector(socket, U0i);

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

        // receive all shares from dealer
        co_await receiveSharesFromDealer(socket, n, k, modValue, AVShare0, BMShare0, CVShare0, vectorA0, vectorB0, scalarC0, AShare0, bShare0, CShare0, eShare0, one0, query_i);
        // boost::asio::co_spawn(io_context,
        //                       receiveSharesFromDealer(socket, n, k, modValue, eShare0, VShare0, BB0, C0, A10, B10, c0, AShare0, CShare0, bShare0, one0),
        //                       boost::asio::detached);

        // io_context.run();

        cout << "Party0: received all shares, ready for MPC computation\n";
        int i = query_i;

        // Load party0 shares
        vector<vector<int>> U0 = loadMatrix("U_ShareMatrix0.txt");
        vector<vector<int>> V0 = loadMatrix("V_ShareMatrix0.txt");

        size_t m = U0.size();

        tcp::socket peer_socket(io_context);
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 9003));

        cout << "Party0: waiting for Party1 to connect...\n";
        co_await acceptor.async_accept(peer_socket, use_awaitable);
        // acceptor.accept(peer_socket);
        cout << "Party0: Party1 connected\n";

        // send and reciever blinds

        vector<int> blindV0(n), blindV1(n);
        vector<vector<int>> blindM0(n, vector<int>(k)), blindM1(n, vector<int>(k)); // get blindM1 from other party

        for (size_t it = 0; it < n; it++)
        {
            blindV0[it] = (eShare0[it] + AVShare0[it]) % modValue;
        }

        for (size_t it = 0; it < n; it++)
            for (size_t j = 0; j < k; j++)
                blindM0[it][j] = (V0[it][j] + BMShare0[it][j]) % modValue;

        co_await sendVector(peer_socket, blindV0);
        co_await recvVector(peer_socket, blindV1);
        co_await sendMatrix(peer_socket, blindM0);
        co_await recvMatrix(peer_socket, blindM1);

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
        vector<int> V0j = mpcVectorandMatrixMul(
            alphaV,
            V0,
            betaM,
            AVShare0,
            CShare0,
            modValue);

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

        // calculate alpha, beta
        vector<int> alpha(k), beta(k);
        // apha=x0+x1+a0+a1
        for (size_t it = 0; it < k; it++)
        {
            alpha[it] = (UA0[it] + UA1[it]) % modValue;
            beta[it] = (VB0[it] + VB1[it]) % modValue;
        }

        // compute share of mpc dot product of <ui,vi>
        int UdotV0 = mpcDotProduct(alpha, vectorB0, beta, vectorA0, scalarC0, modValue);

        // compute share of 1-<ui,vj>
        int sub0 = (one0 - UdotV0) % modValue;
        sub0 = sub0 < 0 ? sub0 + modValue : sub0;

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

        // now calculate ui=ui+vj(1-<ui,vj>)
        cout << "Party 0 sending : ";
        for (size_t it = 0; it < k; it++)
        {
            U0i[it] = (U0i[it] + mul0[it]) % modValue;
            cout << U0i[it] << " ";
        }
        U0[i] = U0i;
        saveMatrix("U_ShareMatrix0.txt", U0);
        // sent share back to client

        co_await sendFinalSharesToDealer(socket, U0i);
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
        boost::asio::co_spawn(io_context, party0(io_context), boost::asio::detached);
        io_context.run();
    }
    catch (exception &e)
    {
        cerr << "Party0 exception: " << e.what() << "\n";
    }
    return 0;
}