#pragma once
#include <utility>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <iostream>
#include <random>

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
namespace this_coro = boost::asio::this_coro;


inline uint32_t random_uint32() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis;
    return dis(gen);
}

// Blind by XOR mask
inline uint32_t blind_value(uint32_t vectorV) {
    return vectorV ^ 0xDEADBEEF;
}


inline awaitable<void> send_int(tcp::socket& socket, int32_t x,const std::string& name) {
    std::cout << "P2 sending to " << name << ": " << x << "\n";
    co_await boost::asio::async_write(socket, boost::asio::buffer(&x, sizeof(x)), boost::asio::use_awaitable);
}


inline awaitable<void> recv_int(tcp::socket& socket, int32_t &out) {
    co_await boost::asio::async_read(socket, boost::asio::buffer(&out, sizeof(out)), boost::asio::use_awaitable);
}


// to send and recieve vector
inline awaitable<void> sendVector(tcp::socket& socket, const std::vector<int>& vectorV) {
    uint64_t n = vectorV.size();
    co_await boost::asio::async_write(socket, boost::asio::buffer(&n, sizeof(n)),boost::asio::use_awaitable);
    if (n) co_await boost::asio::async_write(socket, boost::asio::buffer(vectorV.data(), n * sizeof(int)), boost::asio::use_awaitable);
}
inline awaitable<void> recvVector(tcp::socket& socket, std::vector<int>& vectorV) {
    uint64_t n;
    co_await boost::asio::async_read(socket, boost::asio::buffer(&n, sizeof(n)), boost::asio::use_awaitable);
    vectorV.resize(n);
    if (n) co_await boost::asio::async_read(socket, boost::asio::buffer(vectorV.data(), n * sizeof(int)), boost::asio::use_awaitable);
}


// to send and recieve matrix

inline awaitable<void> sendMatrix(tcp::socket& socket, const std::vector<std::vector<int>>& matrixM) {
    uint64_t rows = matrixM.size();
    uint64_t cols = (rows ? matrixM[0].size() : 0);
    co_await boost::asio::async_write(socket, boost::asio::buffer(&rows, sizeof(rows)), boost::asio::use_awaitable);
    co_await boost::asio::async_write(socket, boost::asio::buffer(&cols, sizeof(cols)), boost::asio::use_awaitable);
    for (const auto &row : matrixM) {
        co_await boost::asio::async_write(socket, boost::asio::buffer(row.data(), cols * sizeof(int)), boost::asio::use_awaitable);
    }
}


inline awaitable<void> recvMatrix(tcp::socket& socket, std::vector<std::vector<int>>& matrixM) {
    uint64_t rows, cols;
    co_await boost::asio::async_read(socket, boost::asio::buffer(&rows, sizeof(rows)), boost::asio::use_awaitable);
    co_await boost::asio::async_read(socket, boost::asio::buffer(&cols, sizeof(cols)), boost::asio::use_awaitable);
    matrixM.assign(rows, std::vector<int>(cols));
    for (std::size_t r = 0; r < rows; ++r) {
        co_await boost::asio::async_read(socket, boost::asio::buffer(matrixM[r].data(), cols * sizeof(int)), boost::asio::use_awaitable);
    }
}