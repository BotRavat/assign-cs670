#include <iostream>
#include <random>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <array>

using namespace std;

// syntax source for below random fun. --> https://learn.microsoft.com/en-us/cpp/standard-library/random?view=msvc-170
/**
 random_device rd;                      // non-deterministic generator
 mt19937 gen(rd());                     // to seed mersenne twister.
 uniform_int_distribution<> dist(1, 6); // distribute results between 1 and 6 inclusive.
 uniform_int_distribution<> dist(std::numeric_limits<int64_t>::min(),std::numeric_limits<int64_t>::max()) // covers negative and positive numbers

 for (int i = 0; i < 5; ++i)
 {
     cout << dist(gen) << " "; // pass the generator to the distribution.
 }
**/

namespace
{
    mt19937_64 &rngLocation() // on every call of distributor it will generate next set of values thats why for generate and and generateShare will generate different numbers
    {
        static mt19937_64 genLocation(12345); // seed 12345 for reproducable result, will use above (random_device rd) later
        return genLocation;
    }
    mt19937_64 &rngValue()
    {
        static mt19937_64 genValue(24135);
        return genValue;
    }
    mt19937_64 &rngShare()
    {
        static mt19937_64 genShare(65754);
        return genShare;
    }
}

void handleErrors()
{
    ERR_print_errors_fp(stderr);
    abort();
}

__uint128_t bytesToUint128(const std::array<uint8_t, 16> &arr)
{
    __uint128_t val = 0;
    for (int i = 0; i < 16; ++i)
        val = (val << 8) | arr[i];
    return val;
}

// cout supports 64 bit number
void print_uint128(__uint128_t value)
{
    if (value > UINT64_MAX)
    {
        print_uint128(value >> 64); // higher 64 bits
        cout << " ";
    }
    cout << (uint64_t)(value & 0xFFFFFFFFFFFFFFFFULL); // then lower 64 bits
}

vector<int> numToBinV(uint64_t num, uint64_t domainSize)
{
    int height = static_cast<int>(ceil(log2(domainSize)));
    vector<int> bits(height, 0);

    for (int i = height - 1; i >= 0; --i)
    {
        bits[i] = num & 1; // get LSB
        num >>= 1;         // shift right
    }

    return bits;
}

// helper to get level number
int getLevel(int nodeIndex)
{
    return (int)floor(log2(nodeIndex + 1));
}

pair<__uint128_t, __uint128_t> prg2(__uint128_t parentSeed)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        handleErrors();

    // Convert __uint128_t to 16-byte array
    array<uint8_t, 16> key{};
    for (int i = 0; i < 16; i++)
        key[15 - i] = parentSeed >> (8 * i);

    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), nullptr, key.data(), nullptr) != 1)
        handleErrors();

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    uint8_t inL[16] = {0};
    uint8_t inR[16] = {0};
    inR[15] = 1;

    int outlen = 0;

    array<uint8_t, 16> leftChild{}, rightChild{};

    if (EVP_EncryptUpdate(ctx, leftChild.data(), &outlen, inL, 16) != 1)
        handleErrors();

    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), nullptr, key.data(), nullptr) != 1)
        handleErrors();

    if (EVP_EncryptUpdate(ctx, rightChild.data(), &outlen, inR, 16) != 1)
        handleErrors();

    EVP_CIPHER_CTX_free(ctx);

    return {bytesToUint128(leftChild), bytesToUint128(rightChild)};
}

struct DpfKey
{
    int targetLocation, targetValue;
    vector<__uint128_t> V0, V1; // vector containing nodes of tree for respective tree, size= total number of nodes
    vector<bool> T0, T1;        // vector containing flag bits for each node, size= total number of nodes
    vector<__uint128_t> CW;     // vector containing correction word per level, size= height of tree
};

void evalDPF(int key, int index)
{
}

void generateDPF(DpfKey dpf, int domainSize)
{
    uniform_int_distribution<int64_t> dist(std::numeric_limits<int64_t>::min(),
                                           std::numeric_limits<int64_t>::max());
    auto &genShare = rngLocation();

    // __uint128_t seed0 = dist(genShare);
    // __uint128_t seed1 = dist(genShare);

    __uint128_t seed0 = (static_cast<__uint128_t>(dist(genShare)) << 64) | static_cast<uint64_t>(dist(genShare));
    __uint128_t seed1 = (static_cast<__uint128_t>(dist(genShare)) << 64) | static_cast<uint64_t>(dist(genShare));

    seed0 &= ~(__uint128_t)1;
    seed1 |= 1;

    dpf.V0[0] = seed0;
    dpf.V1[0] = seed1;

    // checking lsb bits
    // cout << "V0[0] LSB: ";
    // print_uint128(dpf.V0[0]&1);
    // cout << endl;
    // cout << "V1[0] LSB: ";
    // print_uint128(dpf.V1[0]&1);
    // cout << endl;

    int totalNodes = dpf.V0.size();
    int lastParentIdx = (totalNodes - 2) / 2;
    for (int i = 0; i <= lastParentIdx; i++)
    {
        pair<__uint128_t, __uint128_t> childSeed0 = prg2(dpf.V0[i]), childSeed1 = prg2(dpf.V1[i]);
        dpf.V0[2 * i + 1] = childSeed0.first;
        dpf.V0[2 * i + 2] = childSeed0.second;
        dpf.V1[2 * i + 1] = childSeed1.first;
        dpf.V1[2 * i + 2] = childSeed1.second;
    }

    int height = dpf.CW.size();

    vector<int> binOfLocation = numToBinV(dpf.targetLocation, domainSize);
    vector<__uint128_t> L0(height), R0(height), L1(height), R1(height);
    vector<bool> cwtL(height), cwtR(height);
    for (int l = 0; l < height; l++)
    {
        __u128 l0 = 0, l1 = 0, r0 = 0, r1 = 0;
        for (int nd = 0; nd < pow(2, l); nd++)
        {
            int currNode = pow(2, l) - 1 + nd;
            l0 = l0 ^ dpf.V0[currNode];
            l1 = l1 ^ dpf.V1[currNode];
            r0 = r0 ^ dpf.V0[currNode + 1];
            r1 = r1 ^ dpf.V1[currNode + 1];
        }
        L0[l] = l0;
        L1[l] = l1;
        R0[l] = r0;
        R1[l] = r1;

        // compute correction word
        dpf.CW[l] = ((binOfLocation[l] * (l0 ^ l1)) ^ ((1 - binOfLocation[l]) * (r0 ^ r1)));

        // now we compute flag and flag correction

        // flag correction
        bool cwtl0, cwtr0, cwtl1, cwtr1;
        cwtl0 = l0 & 1;
        cwtr0 = r0 & 1;
        cwtl1 = l1 & 1;
        cwtr1 = r1 & 1;

        bool cwtl, cwtr;
        cwtl = (cwtl0 ^ cwtl1 ^ binOfLocation[l] ^ 1);
        cwtr = (cwtr0 ^ cwtr1 ^ binOfLocation[l]);
        cwtL[l] = cwtl;
        cwtR[l] = cwtr;
    }

    for (int i = 0; i <= lastParentIdx; i++)
    {
        // compute final flag bits for each node
        dpf.T0[2 * i] = (dpf.V0[2 * i] & 1) ^ (dpf.T0[i] & cwtL[getLevel(2 * i)]);
        dpf.T0[2 * i + 1] = (dpf.V0[2 * i + 1] & 1) ^ (dpf.T0[i] & cwtR[getLevel(2 * i + 1)]);
        dpf.T1[2 * i] = (dpf.V1[2 * i] & 1) ^ (dpf.T1[i] & cwtL[getLevel(2 * i)]);
        dpf.T1[2 * i + 1] = (dpf.V1[2 * i + 1] & 1) ^ (dpf.T1[i] & cwtR[getLevel(2 * i + 1)]);

        // apply correction word to each node
        dpf.V0[2 * i] = dpf.V0[2 * i] ^ (dpf.T0[i] & dpf.CW[getLevel(2 * i)]);
        dpf.V0[2 * i + 1] = dpf.V0[2 * i + 1] ^ (dpf.T0[i] & dpf.CW[getLevel(2 * i + 1)]);
        dpf.V1[2 * i] = dpf.V1[2 * i] ^ (dpf.T1[i] & dpf.CW[getLevel(2 * i)]);
        dpf.V1[2 * i + 1] = dpf.V1[2 * i + 1] ^ (dpf.T1[i] & dpf.CW[getLevel(2 * i + 1)]);
    }

    // to check tree
    // for (int i = 0; i < min(8, (int)dpf.V0.size()); i++) // print first 8 nodes
    // {
    //     cout << "Node " << i
    //          << " V0: " << (uint64_t)(dpf.V0[i] >> 64) << " " << (uint64_t)(dpf.V0[i] & 0xFFFFFFFFFFFFFFFFULL)
    //          << " | V1: " << (uint64_t)(dpf.V1[i] >> 64) << " " << (uint64_t)(dpf.V1[i] & 0xFFFFFFFFFFFFFFFFULL)
    //          << endl;
    // }
}

void EvalFull()
{
}

int main()
{

    int dpf_size, num_dpfs;
    cout << "Enter DPF size and number of dpfs: ";
    cin >> dpf_size >> num_dpfs;

    uniform_int_distribution<uint64_t> dist(0, dpf_size - 1);
    uniform_int_distribution<int64_t> dist2(std::numeric_limits<int64_t>::min(),
                                            std::numeric_limits<int64_t>::max());
    auto &genLocation = rngLocation();
    auto &genValue = rngValue();

    vector<pair<int, int>> queries;

    int totalNodes = 2 * dpf_size - 1;
    for (int i = 0; i < num_dpfs; i++)
    {
        DpfKey dpf;
        int location, value;
        location = dist(genLocation);
        value = dist2(genValue);
        dpf.targetLocation = location;
        dpf.targetValue = value;
        dpf.V0.resize(totalNodes);
        dpf.V1.resize(totalNodes);
        dpf.T0.resize(totalNodes);
        dpf.T1.resize(totalNodes);
        int height = static_cast<int>(ceil(log2(dpf_size)));
        dpf.CW.resize(height);
        cout << "height is :" << height << endl;
        generateDPF(dpf, dpf_size);
    }

    cout << "DPFs, generated \n";

    return 0;
}