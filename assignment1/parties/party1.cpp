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

// Coroutine to receive all shares and triplets from dealer
awaitable<void> receiveSharesFromDealer(
    tcp::socket &socket,
    int &n, int &k, int &modValue,
    std::vector<int> &eShare1,
    std::vector<std::vector<int>> &VShare1,
    std::vector<int> &B1, std::vector<int> &C1,
    std::vector<int> &A11, std::vector<int> &B11, int &c1,
    std::vector<int> &AShare1, std::vector<int> &CShare1, int &bShare1,
    int &one1, int &query_i)
{
    co_await recv_int(socket, n);
    co_await recv_int(socket, k);
    co_await recv_int(socket, modValue);
    std::cout << "Party0: received one0 = " << modValue << "\n"
              << std::flush;

    eShare1.resize(n);
    VShare1.resize(n, std::vector<int>(k));
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
    std::cout << "Party0: received c1 = " << c1 << "\n"
              << std::flush;

    // e-share
    co_await recvVector(socket, eShare1);

    // matrix-vector triplet
    co_await recvMatrix(socket, VShare1);
    co_await recvVector(socket, B1);
    co_await recvVector(socket, C1);

    co_await recv_int(socket, one1);
    std::cout << "Party0: received one1 = " << one1 << "\n"
              << std::flush;
    co_await recv_int(socket, query_i);
    std::cout << "Party0: received query_i = " << query_i << "\n"
              << std::flush;
    co_return;
}

awaitable<void> sendFinalSharesToDealer(tcp::socket &socket, const std::vector<int> &U1i)
{
    co_await sendVector(socket, U1i);

    std::cout << "Party: sent final user vector share to dealer\n";
    co_return;
}

std::vector<std::vector<int>> loadMatrix(const std::string &filename)
{
    std::ifstream inFile(filename);
    if (!inFile)
    {
        std::cerr << "Error opening file for reading: " << filename << std::endl;
        return {};
    }

    std::vector<std::vector<int>> matrix;
    std::string line;

    while (std::getline(inFile, line))
    {
        std::stringstream ss(line);
        int val;
        std::vector<int> row;
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
void saveMatrix(const std::string &filename, const std::vector<std::vector<int>> &matrix)
{
    std::ofstream outFile(filename);
    if (!outFile)
    {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
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
    std::cout << "Matrix saved successfully to " << filename << "\n";
}
awaitable<void> party1(boost::asio::io_context &io_context)
{
    try
    {
        // boost::asio::io_context io_context;

        // boost::asio::steady_timer timer(io_context, std::chrono::milliseconds(500));
        // co_await timer.async_wait(use_awaitable);

        // connect to dealer
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", "9002");
        co_await boost::asio::async_connect(socket, endpoints, use_awaitable);
        // boost::asio::connect(socket, endpoints);

        std::cout << "Party1: connected to dealer\n";

        // placeholders for received data
        int n, k, modValue;
        std::vector<int> eShare1;
        std::vector<std::vector<int>> VShare1;
        std::vector<int> B1, C1;
        std::vector<int> A11, B11;
        int c1;
        std::vector<int> AShare1, CShare1;
        int bShare1, one1, query_i;

        // receive all shares from dealer
        co_await receiveSharesFromDealer(socket, n, k, modValue, eShare1, VShare1, B1, C1, A11, B11, c1, AShare1, CShare1, bShare1, one1, query_i);

        // io_context.run();

        std::cout << "Party1: received all shares, ready for MPC computation\n";

        int i = query_i;

        // Load party0 shares
        std::vector<std::vector<int>> U1 = loadMatrix("U_ShareMatrix1.txt");
        std::vector<std::vector<int>> V1 = loadMatrix("V_ShareMatrix1.txt");

        std::size_t m = U1.size();

        tcp::socket peer_socket(io_context);
        auto peer_endpoints = resolver.resolve("127.0.0.1", "9003"); // listening
        co_await async_connect(peer_socket, peer_endpoints, use_awaitable);
        // boost::asio::connect(peer_socket, peer_endpoints);
        std::cout << "Party1: connected to Party0\n";

        // send and reciever blinds
        std::vector<std::vector<int>> blindM0, blindM1(n, std::vector<int>(k)); // get blindM0 from other party
        for (std::size_t it = 0; it < n; it++)
        {
            for (std::size_t j = 0; j < k; j++)
                blindM1[it][j] = (V1[it][j] + VShare1[it][j]) % modValue;
        }
        co_await recvMatrix(peer_socket, blindM0);
        co_await sendMatrix(peer_socket, blindM1);

        std::vector<int> blindV0, blindV1(n);
        for (std::size_t it = 0; it < n; it++)
        {
            blindV1[it] = (eShare1[it] + B1[it]) % modValue;
        }
        co_await recvVector(peer_socket, blindV0);
        co_await sendVector(peer_socket, blindV1);

        // compute alphaM, betaV
        std::vector<std::vector<int>> alphaM(n, std::vector<int>(k));
        std::vector<int> betaV(n);
        for (std::size_t it = 0; it < n; it++)
        {
            betaV[it] = (blindV0[it] + blindV1[it]) % modValue;
            for (std::size_t j = 0; j < k; j++)
                alphaM[it][j] = (blindM0[it][j] + blindM1[it][j]) % modValue;
        }

        // get the share of row vi and compute V1j
        std::vector<int> V1j = mpcVectorandMatrixMul(
            alphaM,
            B1,
            betaV,
            VShare1,
            C1,
            1,
            modValue);

        // take ith row of matrix user
        std::vector<int> U1i = U1[i];
        /*

            // finally we have both shares of both ith row of user- U0i and jth row of item V0j
          U0i and V0j

        */

        // get beavers triplet for vectors from dealer

        // sent blinded value to party S1
        // UA0=U0[i]+A0, VB0=V0[i]+B0   // vector addition
        std::vector<int> UA1(k), VB1(k);
        for (std::size_t it = 0; it < k; it++)
        {
            UA1[it] = (U1i[it] + A11[it]) % modValue;
            VB1[it] = (V1j[it] + B11[it]) % modValue;
        }

        // get blinded values f1om party S1
        std::vector<int> UA0, VB0; // U1[i]+A1,V1[j]+B1

        co_await recvVector(peer_socket, UA0);
        co_await sendVector(peer_socket, UA1);
        co_await recvVector(peer_socket, VB0);
        co_await sendVector(peer_socket, VB1);

        // calculate alpha, beta
        std::vector<int> alpha(k), beta(k);
        // apha=x0+x1+a0+a1
        for (std::size_t it = 0; it < k; it++)
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

        std::vector<int> blindSV1(k);
        for (std::size_t it = 0; it < k; it++)
        {
            blindSV1[it] = (V1j[it] + AShare1[it]) % modValue;
        }
        int blind_scalar1 = (sub1 + bShare1) % modValue;

        // get blinded values from party S1
        std::vector<int> blindSV0;
        int blind_scalar0;
        co_await recvVector(peer_socket, blindSV0);
        co_await sendVector(peer_socket, blindSV1);
        co_await recv_int(peer_socket, blind_scalar0);
        co_await send_int(peer_socket, blind_scalar1, "blind_scalar1");
        // now calculate alpha and beta for vector mul scalar

        std::vector<int> alphaSV(k);
        int betaS;
        for (std::size_t it = 0; it < k; it++)
        {
            alphaSV[it] = (blindSV0[it] + blindSV1[it]) % modValue;
        }
        betaS = (blind_scalar0 + blind_scalar1) % modValue;
        std::vector<int> mul1 = mpcVectorandScalarMul(
            alphaSV,
            bShare1,
            betaS,
            AShare1,
            CShare1,
            1,
            modValue);

        // now calculate ui=ui+vj(1-<ui,vj>)
        for (std::size_t it = 0; it < k; it++)
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
    catch (std::exception &e)
    {
        std::cerr << "Party1 exception: " << e.what() << "\n";
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