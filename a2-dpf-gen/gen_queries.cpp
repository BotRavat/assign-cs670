#include <iostream>
#include <random>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <array>

using namespace std;
using u128 = unsigned __int128;

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

u128 bytesToUint128(const array<uint8_t, 16> &arr)
{
    u128 val = 0;
    for (int i = 0; i < 16; ++i)
        val = (val << 8) | arr[i];
    return val;
}

// cout supports 64 bit number
void print_uint128(u128 value)
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

    for (int i = height - 1; i >= 0; i--)
    {
        bits[i] = num & 1; // get LSB
        num >>= 1;         // shift right
    }

    return bits;
}

// helper to get level number
int getLevel(int nodeIndex)
{
    // return (int)floor(log2(nodeIndex + 1));
    return 31 - __builtin_clz(nodeIndex + 1);
}

void handleErrors()
{
    ERR_print_errors_fp(stderr);
    abort();
}

pair<u128, u128> prg2(u128 parentSeed)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        handleErrors();

    // Convert u128 to 16-byte array
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
    vector<u128> V0, V1; // vector containing nodes of tree for respective tree, size= total number of nodes
    vector<bool> T0, T1; // vector containing flag bits for each node, size= total number of nodes
    vector<u128> CW;     // vector containing correction word per level, size= height of tree
};

vector<u128> evalDPF(u128 rootSeed, vector<bool> &T, vector<u128> &CW, int dpf_size)
{
    int height = (int)(ceil(log2(dpf_size)));
    int leaves = dpf_size; // or 1 << (height);
    int totalNodes = 2 * leaves - 1;
    vector<u128> VShare(dpf_size);
    VShare[0] = rootSeed;
    vector<u128> result(leaves);

    // generate tree using provided key {V,T,CW,dpf_size}
    for (int l = 0; l < height - 1; l++)
    {

        int lvlStrt = (1 << l) - 1;
        int nodesAtLevel = 1 << l;

        int childLevel = l + 1;
        int childStrt = (1 << childLevel) - 1;

        // generate childrens
        for (int i = 0; i < nodesAtLevel; i++)
        {
            int parentIdx = lvlStrt + i;
            int leftChildIdx = childStrt + i;
            int rightChildIdx = childStrt + i + 1;
            pair<u128, u128> childSeed0 = prg2(VShare[lvlStrt + i]);
            VShare[leftChildIdx] = childSeed0.first;   // left child
            VShare[rightChildIdx] = childSeed0.second; // right child

            // apply correction word to nodes

            VShare[leftChildIdx] = VShare[leftChildIdx] ^ (T[parentIdx] & CW[childLevel]);
            VShare[rightChildIdx] = VShare[rightChildIdx] ^ (T[parentIdx] & CW[childLevel]);
        }
    }

    int leafStart = leaves - 1;
    for (int i = 0; i < leaves; i++)
    {
        result[i] = VShare[leafStart + i] ^ (T[leafStart + i] ? CW[height - 1] : 0);
        // result[i] = VShare[leafStart + i];
    }

    return result;
}

void EvalFull(DpfKey &dpf, int dpf_size)
{
    int targetLocation = dpf.targetLocation, targetValue = dpf.targetValue;
    vector<u128> V0 = dpf.V0, V1 = dpf.V1;
    vector<bool> T0 = dpf.T0, T1 = dpf.T1;
    vector<u128> CW = dpf.CW;

    vector<u128> eval0, eval1;
    eval0 = evalDPF(V0[0], T0, CW, dpf_size);
    eval1 = evalDPF(V1[0], T1, CW, dpf_size);

    u128 valueAtTarget;
    for (int i = 0; i < dpf_size; i++)
    {
        u128 evalFull = eval0[i] ^ eval1[i];
        cout << endl
             << "For index " << i << " : ";
        print_uint128(eval0[i]);
        cout << " XOR ";
        print_uint128(eval1[i]);
        cout << " = ";
        print_uint128(evalFull);
        if (i == dpf.targetLocation)
            valueAtTarget = evalFull;
    }
    cout << endl;
    if (valueAtTarget == dpf.targetValue)
        cout << " Test Passed";
    else
        cout << " Test Failed";
}

//
void generateDPF(DpfKey &dpf, int domainSize)
{
    uniform_int_distribution<int64_t> dist(std::numeric_limits<int64_t>::min(),
                                           std::numeric_limits<int64_t>::max());
    auto &genShare = rngShare();
    int height = (int)(ceil(log2(domainSize)));
    int leaves = 1 << (height);
    // int leaves = 1 << (height - 1);
    int totalNodes = 2 * leaves - 1;
    // int totalNodes = dpf.V0.size();
    int lastParentIdx = (totalNodes - 2) / 2;

    // u128 seed0 = dist(genShare);
    // u128 seed1 = dist(genShare);
    u128 seed0 = (static_cast<u128>(dist(genShare)) << 64) | static_cast<uint64_t>(dist(genShare));
    u128 seed1 = (static_cast<u128>(dist(genShare)) << 64) | static_cast<uint64_t>(dist(genShare));

    seed0 &= ~(u128)1;
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

    // intial flags
    dpf.T0[0] = false;
    dpf.T1[0] = true;

    vector<int> binOfLocation = numToBinV(dpf.targetLocation, domainSize);
    vector<u128> L0(height), R0(height), L1(height), R1(height);
    vector<bool> cwtL(height), cwtR(height);

    for (int l = 0; l < height - 1; l++)
    {

        int lvlStrt = (1 << l) - 1;
        int nodesAtLevel = 1 << l;

        int childLevel = l + 1;
        int childStrt = (1 << childLevel) - 1;

        // generate childrens
        for (int i = 0; i < nodesAtLevel; i++)
        {
            int leftChildIdx = childStrt + 2 * i;
            int rightChildIdx = childStrt + 2 * i + 1;

            //             // binary tree formula:
            // int parentIdx = lvlStrt + i;
            // int leftChildIdx = 2 * parentIdx + 1;
            // int rightChildIdx = 2 * parentIdx + 2;
            pair<u128, u128> childSeed0 = prg2(dpf.V0[lvlStrt + i]); // tree 0
            pair<u128, u128> childSeed1 = prg2(dpf.V1[lvlStrt + i]); // for tree 1
            dpf.V0[leftChildIdx] = childSeed0.first;                 // left child
            dpf.V0[rightChildIdx] = childSeed0.second;               // right child
            dpf.V1[leftChildIdx] = childSeed1.first;
            dpf.V1[rightChildIdx] = childSeed1.second;
        }

        // now we will compute Lb
        u128 l0 = 0, l1 = 0, r0 = 0, r1 = 0;

        for (int i = 0; i < nodesAtLevel; i++)
        {
            int parentIdx = lvlStrt + i;
            int leftChildIdx = childStrt + 2 * i;
            int rightChildIdx = childStrt + 2 * i + 1;

            l0 = l0 ^ dpf.V0[leftChildIdx];
            l1 = l1 ^ dpf.V1[leftChildIdx];
            r0 = r0 ^ dpf.V0[rightChildIdx];
            r1 = r1 ^ dpf.V1[rightChildIdx];
        }

        L0[childLevel] = l0;
        L1[childLevel] = l1;
        R0[childLevel] = r0;
        R1[childLevel] = r1;

        // compute correction word
        if (binOfLocation[childLevel] == 0)
        {
            dpf.CW[childLevel] = r0 ^ r1;
        }
        else
        {
            dpf.CW[childLevel] = l0 ^ l1;
        }
        // dpf.CW[childLevel] = ((binOfLocation[childLevel] * (l0 ^ l1)) ^ ((1 - binOfLocation[childLevel]) * (r0 ^ r1)));

        // now we compute flag and flag correction

        // 4.) control bits for flag  or flag correction
        bool cwtl0, cwtr0, cwtl1, cwtr1;

        // cwtl0 = (l0 & 1) ^ binOfLocation[childLevel];
        // cwtl1 = (l1 & 1) ^ binOfLocation[childLevel];
        // cwtL[childLevel] = cwtl0 ^ cwtl1 ^ 1;
        // cwtR[childLevel] = cwtr0 ^ cwtr1;
        cwtl0 = l0 & 1;
        cwtr0 = r0 & 1;
        cwtl1 = l1 & 1;
        cwtr1 = r1 & 1;

        bool cwtl, cwtr;
        cwtl = (cwtl0 ^ cwtl1 ^ binOfLocation[childLevel] ^ 1);
        cwtr = (cwtr0 ^ cwtr1 ^ binOfLocation[childLevel]);
        cwtL[childLevel] = cwtl;
        cwtR[childLevel] = cwtr;

        // 5.) compute child flags and apply flag correction and correction word
        for (int i = 0; i < nodesAtLevel; i++)
        {
            int parentIdx = lvlStrt + i;
            int leftChildIdx = childStrt + 2 * i;
            int rightChildIdx = childStrt + 2 * i + 1;

            // compute final flag bits for each node
            dpf.T0[leftChildIdx] = (dpf.V0[leftChildIdx] & 1) ^ (dpf.T0[parentIdx] & cwtl);
            dpf.T0[rightChildIdx] = (dpf.V0[rightChildIdx] & 1) ^ (dpf.T0[parentIdx] & cwtr);
            dpf.T1[leftChildIdx] = (dpf.V1[leftChildIdx] & 1) ^ (dpf.T1[parentIdx] & cwtl);
            dpf.T1[rightChildIdx] = (dpf.V1[rightChildIdx] & 1) ^ (dpf.T1[parentIdx] & cwtr);

            // apply correction word to nodes

            dpf.V0[leftChildIdx] = dpf.V0[leftChildIdx] ^ (dpf.T0[parentIdx] & dpf.CW[childLevel]);
            dpf.V0[rightChildIdx] = dpf.V0[rightChildIdx] ^ (dpf.T0[parentIdx] & dpf.CW[childLevel]);
            dpf.V1[leftChildIdx] = dpf.V1[leftChildIdx] ^ (dpf.T1[parentIdx] & dpf.CW[childLevel]);
            dpf.V1[rightChildIdx] = dpf.V1[rightChildIdx] ^ (dpf.T1[parentIdx] & dpf.CW[childLevel]);
        }
    }

    // final leaf-level correction word
    int leafStart = leaves - 1;
    // int leafStart = (1 << (height - 1)) - 1;
    // u128 xorSum0 = 0, xorSum1 = 0;
    // for (int i = 0; i < leaves; i++)
    // {
    //     xorSum0 ^= dpf.V0[leafStart + i];
    //     xorSum1 ^= dpf.V1[leafStart + i];
    // }
    // u128 fCW = xorSum0 ^ xorSum1 ^ (u128)dpf.targetValue;
    // dpf.CW.push_back(fCW);

    // cout << "Final Correction Word (leaf-level): ";
    // print_uint128(fCW);
    // cout << endl;

    int targetLeafIdx = leafStart + dpf.targetLocation;
    u128 leaf0 = dpf.V0[targetLeafIdx];
    u128 leaf1 = dpf.V1[targetLeafIdx];
    u128 fCW = (u128)dpf.targetValue ^ leaf0 ^ leaf1;
    dpf.CW[height - 1] = fCW;

    // to check tree
    // for (int i = 0; i < min(8, (int)dpf.V0.size()); i++) // print first 8 nodes
    // {
    //     cout << "Node " << i
    //          << " V0: " << (uint64_t)(dpf.V0[i] >> 64) << " " << (uint64_t)(dpf.V0[i] & 0xFFFFFFFFFFFFFFFFULL)
    //          << " | V1: " << (uint64_t)(dpf.V1[i] >> 64) << " " << (uint64_t)(dpf.V1[i] & 0xFFFFFFFFFFFFFFFFULL)
    //          << endl;
    // }
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

    int totalNodes = 2 * dpf_size - 1;
    for (int i = 0; i < num_dpfs; i++)
    {
        DpfKey dpf;
        int location;
        int64_t value;
        location = dist(genLocation);
        value = dist2(genValue);
        dpf.targetLocation = location;
        dpf.targetValue = value;

        dpf.V0.resize(totalNodes, (u128)0);
        dpf.V1.resize(totalNodes, (u128)0);
        dpf.T0.resize(totalNodes, false);
        dpf.T1.resize(totalNodes, false);
        int height = (int)(ceil(log2(dpf_size)));
        dpf.CW.resize(height, (u128)0);
        cout << "height is :" << height << endl;
        cout << "Target Index: " << location << " Target Value: " << value << endl;
        generateDPF(dpf, dpf_size);
        cout << " DPFs, generated \n";
        EvalFull(dpf, dpf_size);
    }

    return 0;
}