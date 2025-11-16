#pragma once
#include <utility>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <iostream>
#include <random>
#include <fstream>
#include <sstream>

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
using namespace std;
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


// for u128

using u128 = unsigned __int128;

inline std::array<uint8_t,16> encode_u128(u128 v) {
    std::array<uint8_t,16> b{};
    for (int i = 15; i >= 0; --i) {
        b[i] = static_cast<uint8_t>(v & 0xFF);
        v >>= 8;
    }
    return b;
}

inline u128 decode_u128(const std::array<uint8_t,16>& b) {
    u128 v = 0;
    for (int i = 0; i < 16; ++i) {
        v = (v << 8) | (u128)b[i];
    }
    return v;
}

inline awaitable<void> send_u128(tcp::socket& socket, u128 x) {
    auto buf = encode_u128(x);
    co_await boost::asio::async_write(socket,
        boost::asio::buffer(buf.data(), buf.size()),
        boost::asio::use_awaitable);
}

inline awaitable<void> recv_u128(tcp::socket& socket, u128 &out) {
    std::array<uint8_t,16> buf;
    co_await boost::asio::async_read(socket,
        boost::asio::buffer(buf.data(), buf.size()),
        boost::asio::use_awaitable);
    out = decode_u128(buf);
}

inline awaitable<void> sendVector_u128(tcp::socket& socket,
                                       const std::vector<u128>& vec) {
    uint64_t n = vec.size();
    co_await boost::asio::async_write(socket, boost::asio::buffer(&n, sizeof(n)),
                                      boost::asio::use_awaitable);

    for (uint64_t i = 0; i < n; ++i) {
        auto b = encode_u128(vec[i]);
        co_await boost::asio::async_write(socket,
            boost::asio::buffer(b.data(), b.size()),
            boost::asio::use_awaitable);
    }
}

inline awaitable<void> recvVector_u128(tcp::socket& socket,
                                       std::vector<u128>& vec) {
    uint64_t n;
    co_await boost::asio::async_read(socket,
        boost::asio::buffer(&n, sizeof(n)),
        boost::asio::use_awaitable);

    vec.resize(n);
    for (uint64_t i = 0; i < n; ++i) {
        std::array<uint8_t,16> b;
        co_await boost::asio::async_read(socket,
            boost::asio::buffer(b.data(), b.size()),
            boost::asio::use_awaitable);
        vec[i] = decode_u128(b);
    }
}

inline awaitable<void> sendVectorBool(tcp::socket& socket,
                                      const std::vector<bool>& vec) {
    uint64_t n = vec.size();
    co_await boost::asio::async_write(socket, boost::asio::buffer(&n, sizeof(n)),
                                      boost::asio::use_awaitable);

    if (n == 0) co_return;

    size_t nb = (n + 7) / 8;
    std::vector<uint8_t> out(nb, 0);

    for (uint64_t i = 0; i < n; ++i)
        if (vec[i])
            out[i >> 3] |= (1u << (i & 7));

    co_await boost::asio::async_write(socket,
        boost::asio::buffer(out.data(), out.size()),
        boost::asio::use_awaitable);
}

inline awaitable<void> recvVectorBool(tcp::socket& socket,
                                      std::vector<bool>& vec) {
    uint64_t n;
    co_await boost::asio::async_read(socket,
        boost::asio::buffer(&n, sizeof(n)),
        boost::asio::use_awaitable);

    vec.assign(n, false);

    if (n == 0) co_return;

    size_t nb = (n + 7) / 8;
    std::vector<uint8_t> in(nb);

    co_await boost::asio::async_read(socket,
        boost::asio::buffer(in.data(), in.size()),
        boost::asio::use_awaitable);

    for (uint64_t i = 0; i < n; ++i)
        vec[i] = (in[i >> 3] >> (i & 7)) & 1;
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

