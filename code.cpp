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
 for (int i = 0; i < 5; ++i)
 {
     cout << dist(gen) << " "; // pass the generator to the distribution.
 }
**/

random_device rd; // non-deterministic generator

namespace
{
    mt19937_64 &rngLocation()
    {

        static mt19937_64 genLocation(rd());
        return genLocation;
    }
    mt19937_64 &rngValue()
    {
        static mt19937_64 genValue(rd());
        return genValue;
    }
    mt19937_64 &rngShare()
    {
        static mt19937_64 genShare(rd());
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
    vector<u128> CW;     // vector containing correction word per level, size= height of tree+1
};

vector<u128> evalDPF(u128 rootSeed, vector<bool> &T, vector<u128> &CW, int dpf_size)
{
    int height = (int)(ceil(log2(dpf_size)));
    int leaves = 1 << (height);
    int totalNodes = 2 * leaves - 1;
    int lastParentIdx = (totalNodes - 2) / 2;
    vector<u128> VShare(totalNodes);
    VShare[0] = rootSeed;
    vector<u128> result(leaves);

    // generate tree using provided key {V,T,CW,dpf_size}
    for (int l = 0; l < height; l++)
    {

        int lvlStrt = (1 << l) - 1;
        int nodesAtLevel = 1 << l;
        int childLevel = l + 1;

        // generate childrens
        for (int i = 0; i < nodesAtLevel; i++)
        {
            int parentIdx = lvlStrt + i;
            int leftChildIdx = 2 * parentIdx + 1;
            int rightChildIdx = 2 * parentIdx + 2;

            pair<u128, u128> childSeed = prg2(VShare[parentIdx]);
            VShare[leftChildIdx] = childSeed.first;   // left child
            VShare[rightChildIdx] = childSeed.second; // right child

            // apply correction word to nodes
            if (T[parentIdx])
            {
                VShare[leftChildIdx] ^= CW[childLevel];
                VShare[rightChildIdx] ^= CW[childLevel];
            }
        }
    }

    // Apply final correction word to target leaf
    int leafStart = (1 << (height)) - 1;
    for (int i = 0; i < dpf_size; i++)
    {
        result[i] = VShare[leafStart + i];
        if (T[leafStart + i])
        {
            result[i] ^= CW[height + 1];
        }
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
    cout << endl;
}

//
void generateDPF(DpfKey &dpf, int domainSize)
{
    uniform_int_distribution<uint64_t> dist64(0, UINT64_MAX);

    auto &genShare = rngShare();
    int height = (int)(ceil(log2(domainSize)));
    int leaves = 1 << (height);
    int totalNodes = 2 * leaves - 1;
    int lastParentIdx = (totalNodes - 2) / 2;

    u128 seed0 = (static_cast<u128>(dist64(genShare)) << 64) | (dist64(genShare));
    u128 seed1 = (static_cast<u128>(dist64(genShare)) << 64) | (dist64(genShare));

    seed0 &= ~(u128)1;
    seed1 |= 1;

    dpf.V0[0] = seed0;
    dpf.V1[0] = seed1;

    // intial flags
    dpf.T0[0] = false;
    dpf.T1[0] = true;

    vector<int> binOfLocation = numToBinV(dpf.targetLocation, domainSize);
    vector<u128> L0(height + 1), R0(height + 1), L1(height + 1), R1(height + 1);
    vector<bool> cwtL(height + 1), cwtR(height + 1);

    for (int l = 0; l < height; l++)
    {

        int lvlStrt = (1 << l) - 1;
        int nodesAtLevel = 1 << l;

        int childLevel = l + 1;

        // generate childrens
        cout << endl
             << "level: " << l << " total Nodes at level: " << nodesAtLevel << endl;
        for (int i = 0; i < nodesAtLevel; i++)
        {
            int parentIdx = lvlStrt + i;
            int leftChildIdx = 2 * parentIdx + 1;
            int rightChildIdx = 2 * parentIdx + 2;
            pair<u128, u128> childSeed0 = prg2(dpf.V0[parentIdx]); // tree 0
            pair<u128, u128> childSeed1 = prg2(dpf.V1[parentIdx]); // for tree 1
            dpf.V0[leftChildIdx] = childSeed0.first;               // left child
            dpf.V0[rightChildIdx] = childSeed0.second;             // right child
            dpf.V1[leftChildIdx] = childSeed1.first;
            dpf.V1[rightChildIdx] = childSeed1.second;

            // cout << "Parent Node: ";
            // print_uint128(dpf.V0[parentIdx]);
            // cout << " first child: ";
            // print_uint128(childSeed0.first);
            // cout << " second child: ";
            // print_uint128(childSeed0.second);
            // cout << endl;
            // cout << "Parent Node1: ";
            // print_uint128(dpf.V1[parentIdx]);
            // cout << " first child: ";
            // print_uint128(childSeed1.first);
            // cout << " second child: ";
            // print_uint128(childSeed1.second);
            // cout << endl;
        }

        // now we will compute Lb
        u128 l0 = 0, l1 = 0, r0 = 0, r1 = 0;
        for (int i = 0; i < nodesAtLevel; i++)
        {
            int parentIdx = lvlStrt + i;
            int leftChildIdx = 2 * parentIdx + 1;
            int rightChildIdx = 2 * parentIdx + 2;

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
        // if (binOfLocation[l] == 0)
        // {
        //     dpf.CW[childLevel] = r0 ^ r1;
        // }
        // else
        // {
        //     dpf.CW[childLevel] = l0 ^ l1;
        // }
        dpf.CW[childLevel] = ((binOfLocation[l] * (l0 ^ l1)) ^ ((1 ^ binOfLocation[l]) * (r0 ^ r1)));

        // if (binOfLocation[l] == 0)
        // {
        //     dpf.CW[childLevel] = r0 ^ r1; // correct non-target (right) aggregate
        // }
        // else
        // {
        //     dpf.CW[childLevel] = l0 ^ l1; // correct non-target (left) aggregate
        // }

        // now we compute flag and flag correction
        // 4.) control bits for flag  or flag correction
        bool cwtl0, cwtr0, cwtl1, cwtr1;

        cwtl0 = l0 & 1;
        cwtr0 = r0 & 1;
        cwtl1 = l1 & 1;
        cwtr1 = r1 & 1;

        bool cwtl, cwtr;
        cwtl = (cwtl0 ^ cwtl1 ^ binOfLocation[l] ^ 1);
        cwtr = (cwtr0 ^ cwtr1 ^ binOfLocation[l]);
        // cwtL[childLevel] = cwtl;
        // cwtR[childLevel] = cwtr;

        // 5.) compute child flags and apply flag correction and correction word
        for (int i = 0; i < nodesAtLevel; i++)
        {
            int parentIdx = lvlStrt + i;
            int leftChildIdx = 2 * parentIdx + 1;
            int rightChildIdx = 2 * parentIdx + 2;

            // compute final flag bits for each node
            dpf.T0[leftChildIdx] = (dpf.V0[leftChildIdx] & 1) ^ (dpf.T0[parentIdx] & cwtl);
            dpf.T0[rightChildIdx] = (dpf.V0[rightChildIdx] & 1) ^ (dpf.T0[parentIdx] & cwtr);
            dpf.T1[leftChildIdx] = (dpf.V1[leftChildIdx] & 1) ^ (dpf.T1[parentIdx] & cwtl);
            dpf.T1[rightChildIdx] = (dpf.V1[rightChildIdx] & 1) ^ (dpf.T1[parentIdx] & cwtr);

            // apply correction word to nodes
            cout << endl
                 << "level is:" << l << " out of " << height;

            if (dpf.T0[parentIdx])
            {
                dpf.V0[leftChildIdx] ^= dpf.CW[childLevel];
                dpf.V0[rightChildIdx] ^= dpf.CW[childLevel];
            }
            if (dpf.T1[parentIdx])
            {
                dpf.V1[leftChildIdx] ^= dpf.CW[childLevel];
                dpf.V1[rightChildIdx] ^= dpf.CW[childLevel];
            }

            // logical and (&)  only works for lsb
            // dpf.V0[leftChildIdx] = dpf.V0[leftChildIdx] ^ (dpf.T0[parentIdx] & dpf.CW[childLevel]);
            // dpf.V0[rightChildIdx] = dpf.V0[rightChildIdx] ^ (dpf.T0[parentIdx] & dpf.CW[childLevel]);
            // dpf.V1[leftChildIdx] = dpf.V1[leftChildIdx] ^ (dpf.T1[parentIdx] & dpf.CW[childLevel]);
            // dpf.V1[rightChildIdx] = dpf.V1[rightChildIdx] ^ (dpf.T1[parentIdx] & dpf.CW[childLevel]);
        }
    }

    int leafStart = (1 << (height)) - 1;
    int targetLeaf = leafStart + dpf.targetLocation;
    u128 fCW = (u128)dpf.targetValue ^ dpf.V0[targetLeaf] ^ dpf.V1[targetLeaf];
    dpf.CW[height + 1] = fCW;
    // Apply the final correction word only at the target leaf
    if (dpf.T0[targetLeaf])
        dpf.V0[targetLeaf] ^= dpf.CW[height + 1];
    if (dpf.T1[targetLeaf])
        dpf.V1[targetLeaf] ^= dpf.CW[height + 1];

    // dpf.V0[targetLeaf] = dpf.V0[targetLeaf] ^ (dpf.T0[targetLeaf] & dpf.CW[height + 1]);
    // dpf.V1[targetLeaf] = dpf.V1[targetLeaf] ^ (dpf.T1[targetLeaf] & dpf.CW[height + 1]);

    // === print leaf nodes ===
    int targetLeafIdx = leafStart + dpf.targetLocation;
    u128 leaf0 = dpf.V0[targetLeafIdx];
    u128 leaf1 = dpf.V1[targetLeafIdx];
    cout << "\n=== Final DPF Check ===" << endl;
    cout << "Target Index: " << dpf.targetLocation << endl;
    cout << "Target Value: " << dpf.targetValue << endl;

    cout << "\nParty 0 (V0) leaf values:\n";
    for (int i = leafStart; i < leafStart + leaves; i++)
    {
        cout << "  Leaf " << i - leafStart << ": ";
        print_uint128(dpf.V0[i]);
        cout << endl;
    }

    cout << "\nParty 1 (V1) leaf values:\n";
    for (int i = leafStart; i < leafStart + leaves; i++)
    {
        cout << "  Leaf " << i - leafStart << ": ";
        print_uint128(dpf.V1[i]);
        cout << endl;
    }

    cout << "\nFinal correction word fCW: ";
    print_uint128(fCW);
    cout << endl;

    cout << "\nTarget leaf XOR check:\n";
    cout << "  V0[target] = ";
    print_uint128(leaf0);
    cout << "\n  V1[target] = ";
    print_uint128(leaf1);
    cout << "\n  XOR result = ";
    print_uint128(leaf0 ^ leaf1);
    cout << "\n  Expected   = ";
    print_uint128((u128)dpf.targetValue);
    cout << endl;
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
    cout << "Total nodes: " << totalNodes << endl;
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
        dpf.CW.resize(height + 2, (u128)0);
        cout << "height is :" << height << endl;
        cout << "Target Index: " << location << " Target Value: " << value << endl;
        generateDPF(dpf, dpf_size);
        cout << " DPFs, generated \n";
        EvalFull(dpf, dpf_size);
    }

    return 0;
}