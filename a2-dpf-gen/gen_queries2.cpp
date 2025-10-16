#include <iostream>
#include <random>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <array>
#include <limits>
#include <vector>
#include <cmath>

using namespace std;
using u128 = unsigned __int128;

// RNGs (fixed seeds for reproducible debugging)
namespace
{
    mt19937_64 &rngLocation()
    {
        static mt19937_64 genLocation(12345);
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
    {
        val = (val << 8) | (u128)arr[i];
    }
    return val;
}

// cout supports 64 bit number printing in pieces
void print_uint128(u128 value)
{
    if (value > UINT64_MAX)
    {
        print_uint128(value >> 64);
        cout << " ";
    }
    cout << (uint64_t)(value & 0xFFFFFFFFFFFFFFFFULL);
}

void print_target_value(u128 value)
{
    print_uint128(value);
    if (value <= (u128)std::numeric_limits<int64_t>::max())
        cout << " (signed: " << (int64_t)value << ")";
    else
        cout << " (signed: " << (int64_t)(value - (u128(1) << 64)) << ")";
}


vector<int> numToBinV(uint64_t num, uint64_t domainSize)
{
    int height = static_cast<int>(ceil(log2((double)domainSize)));
    vector<int> bits(height, 0);
    for (int i = height - 1; i >= 0; --i)
    {
        bits[i] = num & 1;
        num >>= 1;
    }
    return bits;
}

void handleErrors()
{
    ERR_print_errors_fp(stderr);
    abort();
}

// PRG: AES-128-ECB of block 0 and block 1 under key = parentSeed
pair<u128, u128> prg2(u128 parentSeed)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        handleErrors();

    array<uint8_t, 16> key{};
    for (int i = 0; i < 16; ++i)
    {
        key[15 - i] = (uint8_t)(parentSeed >> (8 * i));
    }

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
    // re-init is not strictly necessary here but keep deterministic state
    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), nullptr, key.data(), nullptr) != 1)
        handleErrors();
    if (EVP_EncryptUpdate(ctx, rightChild.data(), &outlen, inR, 16) != 1)
        handleErrors();

    EVP_CIPHER_CTX_free(ctx);

    return {bytesToUint128(leftChild), bytesToUint128(rightChild)};
}

struct DpfKey
{
    int targetLocation = 0;
    int targetValue = 0;
    vector<u128> V0, V1;
    vector<bool> T0, T1;
    vector<u128> CW; // indexes: 0..height ; final correction at CW[height]
};

vector<u128> evalDPF(u128 rootSeed, vector<bool> &T, vector<u128> &CW, int dpf_size)
{
    int height = (int)ceil(log2((double)dpf_size));
    int leaves = 1 << height;
    int totalNodes = 2 * leaves - 1;
    vector<u128> VShare(totalNodes, (u128)0);
    VShare[0] = rootSeed;
    vector<u128> result(leaves, (u128)0);

    // expand tree: levels l = 0 .. height-1 (parents). children reach level = height (leaves)
    for (int l = 0; l < height; ++l)
    {
        int lvlStrt = (1 << l) - 1;
        int nodesAtLevel = 1 << l;
        int childLevel = l + 1;

        for (int i = 0; i < nodesAtLevel; ++i)
        {
            int parentIdx = lvlStrt + i;
            int leftChildIdx = 2 * parentIdx + 1;
            int rightChildIdx = 2 * parentIdx + 2;

            auto childSeed = prg2(VShare[parentIdx]);
            VShare[leftChildIdx] = childSeed.first;
            VShare[rightChildIdx] = childSeed.second;

            // apply correction word if parent flag is set
            if (T[parentIdx])
            {
                VShare[leftChildIdx] ^= CW[childLevel];
                VShare[rightChildIdx] ^= CW[childLevel];
            }
        }
    }

    int leafStart = (1 << height) - 1;

    //   cout << "Party (V) leaf values:\n";
    // for (int i = leafStart; i < leafStart + leaves; ++i)
    // {
    //     cout << "  Leaf " << (i - leafStart) << ": ";
    //     // print_uint128(dpf.V0[i]);
    //     cout << (T[i] ? " [flag=1]" : " [flag=0]") << "\n";
    // }

    // leaves start index
    // int leafStart = (1 << height) - 1;
    cout << "here ";
    
    for (int i = 0; i < leaves; ++i)
    {
        u128 leafVal = VShare[leafStart + i];
        // Always apply the final CW
        cout << " " << T[leafStart + i] << " ";
        if (T[leafStart + i])
            leafVal ^= CW[height + 1];

        result[i] = leafVal;
    }

    return result;
}

void EvalFull(DpfKey &dpf, int dpf_size)
{
    int height = (int)ceil(log2((double)dpf_size));
    int leaves = 1 << height;
    vector<u128> eval0 = evalDPF(dpf.V0[0], dpf.T0, dpf.CW, dpf_size);
    vector<u128> eval1 = evalDPF(dpf.V1[0], dpf.T1, dpf.CW, dpf_size);

    u128 valueAtTarget = 0;
    int leafStart = (1 << height) - 1;
    for (int i = 0; i < leaves; ++i)
    {
        u128 evalFull = eval0[i] ^ eval1[i];
        cout << "\nFor index " << i << " : ";
        print_uint128(eval0[i]);
        cout << (dpf.T0[i + leafStart] ? " [flag=1]" : " [flag=0]") << "\n";

        cout << " XOR ";
        print_uint128(eval1[i]);
        cout << (dpf.T1[i + leafStart] ? " [flag=1]" : " [flag=0]") << "\n";

        cout << " = ";
        print_uint128(evalFull);
        if (i == dpf.targetLocation)
            valueAtTarget = evalFull;
    }
    cout << endl;

    cout << "Target leaf XOR result: ";
print_target_value(valueAtTarget);
cout << endl;

if (valueAtTarget == (u128)dpf.targetValue)
    cout << "Test Passed\n";
else
{
    cout << "Test Failed\n";
    cout << "Expected at target: ";
    print_target_value((u128)dpf.targetValue);
    cout << " Got: ";
    print_target_value(valueAtTarget);
    cout << endl;
}
    // if (valueAtTarget == (u128)dpf.targetValue)
    //     cout << "Test Passed\n";
    // else
    // {
    //     cout << "Test Failed\n";
    //     cout << "Expected at target: ";
    //     print_uint128((u128)dpf.targetValue);
    //     cout << " Got: ";
    //     print_uint128(valueAtTarget);
    //     cout << endl;
    // }
}

void generateDPF(DpfKey &dpf, int domainSize)
{
    // use uint64 distributions to avoid sign extension surprises
    uniform_int_distribution<uint64_t> dist64(0, numeric_limits<uint64_t>::max());
    auto &genShare = rngShare();

    int height = (int)ceil(log2((double)domainSize));
    int leaves = 1 << height;
    int totalNodes = 2 * leaves - 1;

    // initialize storage
    dpf.V0.assign(totalNodes, (u128)0);
    dpf.V1.assign(totalNodes, (u128)0);
    dpf.T0.assign(totalNodes, false);
    dpf.T1.assign(totalNodes, false);
    dpf.CW.assign(height + 2, (u128)0); // CW[1..height], final at CW[height]

    // seeds for roots (ensure different LSB)
    u128 seed0 = ((u128)dist64(genShare) << 64) | (u128)dist64(genShare);
    u128 seed1 = ((u128)dist64(genShare) << 64) | (u128)dist64(genShare);

    seed0 &= ~(u128)1; // LSB = 0
    seed1 |= (u128)1;  // LSB = 1

    dpf.V0[0] = seed0;
    dpf.V1[0] = seed1;

    dpf.T0[0] = false;
    dpf.T1[0] = true;

    vector<int> binOfLocation = numToBinV((uint64_t)dpf.targetLocation, domainSize);

    // temporary aggregates per level
    vector<u128> L0(height + 1), L1(height + 1), R0(height + 1), R1(height + 1);

    for (int l = 0; l < height; ++l)
    {
        int lvlStrt = (1 << l) - 1;
        int nodesAtLevel = 1 << l;
        int childLevel = l + 1;

        // generate children seeds for all parents at level l
        for (int i = 0; i < nodesAtLevel; ++i)
        {
            int parentIdx = lvlStrt + i;
            int leftChildIdx = 2 * parentIdx + 1;
            int rightChildIdx = 2 * parentIdx + 2;

            auto childSeed0 = prg2(dpf.V0[parentIdx]);
            auto childSeed1 = prg2(dpf.V1[parentIdx]);

            dpf.V0[leftChildIdx] = childSeed0.first;
            dpf.V0[rightChildIdx] = childSeed0.second;
            dpf.V1[leftChildIdx] = childSeed1.first;
            dpf.V1[rightChildIdx] = childSeed1.second;
        }

        // compute XOR aggregates across this child level
        u128 l0 = 0, l1 = 0, r0 = 0, r1 = 0;
        for (int i = 0; i < nodesAtLevel; ++i)
        {
            int parentIdx = lvlStrt + i;
            int leftChildIdx = 2 * parentIdx + 1;
            int rightChildIdx = 2 * parentIdx + 2;
            l0 ^= dpf.V0[leftChildIdx];
            l1 ^= dpf.V1[leftChildIdx];
            r0 ^= dpf.V0[rightChildIdx];
            r1 ^= dpf.V1[rightChildIdx];
        }
        L0[childLevel] = l0;
        L1[childLevel] = l1;
        R0[childLevel] = r0;
        R1[childLevel] = r1;

        // choose CW for this child level according to target bit at this parent level
        // if target bit is 0 -> target path goes to left child at this level, else right child
        if (binOfLocation[l] == 0)
        {
            dpf.CW[childLevel] = r0 ^ r1; // correct non-target (right) aggregate
        }
        else
        {
            dpf.CW[childLevel] = l0 ^ l1; // correct non-target (left) aggregate
        }

        // compute control bits for flags (cwtl, cwtr)
        bool cwtl0 = (bool)((l0 & 1));
        bool cwtr0 = (bool)((r0 & 1));
        bool cwtl1 = (bool)((l1 & 1));
        bool cwtr1 = (bool)((r1 & 1));

        bool cwtl = (cwtl0 ^ cwtl1 ^ binOfLocation[l] ^ 1);
        bool cwtr = (cwtr0 ^ cwtr1 ^ binOfLocation[l]);

        // compute child flags and apply level correction words (conditioned on parent flag)
        for (int i = 0; i < nodesAtLevel; ++i)
        {
            int parentIdx = lvlStrt + i;
            int leftChildIdx = 2 * parentIdx + 1;
            int rightChildIdx = 2 * parentIdx + 2;

            // final flag bits
            dpf.T0[leftChildIdx] = (bool)((dpf.V0[leftChildIdx] & 1)) ^ (dpf.T0[parentIdx] && cwtl);
            dpf.T0[rightChildIdx] = (bool)((dpf.V0[rightChildIdx] & 1)) ^ (dpf.T0[parentIdx] && cwtr);
            dpf.T1[leftChildIdx] = (bool)((dpf.V1[leftChildIdx] & 1)) ^ (dpf.T1[parentIdx] && cwtl);
            dpf.T1[rightChildIdx] = (bool)((dpf.V1[rightChildIdx] & 1)) ^ (dpf.T1[parentIdx] && cwtr);

            // apply the level correction word to children if parent flag says so
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
        }
    }

    // final correction word at leaves: ensure XOR at target leaf equals targetValue
    int leafStart = (1 << height) - 1;
    int targetLeaf = leafStart + dpf.targetLocation;
    u128 fCW = (u128)dpf.targetValue ^ dpf.V0[targetLeaf] ^ dpf.V1[targetLeaf];
    dpf.CW[height + 1] = fCW;

    // apply final CW to leaf values only if the leaf flag indicates
    if (dpf.T0[targetLeaf])
        dpf.V0[targetLeaf] ^= dpf.CW[height + 1];
    if (dpf.T1[targetLeaf])
        dpf.V1[targetLeaf] ^= dpf.CW[height + 1];

    // debug printing (leaf arrays and final check)
    cout << "\n=== Final DPF Check ===\n";
    cout << "Target Index: " << dpf.targetLocation << "  Target Value: " << dpf.targetValue << "\n\n";

    cout << "Party 0 (V0) leaf values:\n";
    for (int i = leafStart; i < leafStart + leaves; ++i)
    {
        cout << "  Leaf " << (i - leafStart) << ": ";
        print_uint128(dpf.V0[i]);
        cout << (dpf.T0[i] ? " [flag=1]" : " [flag=0]") << "\n";
    }

    cout << "\nParty 1 (V1) leaf values:\n";
    for (int i = leafStart; i < leafStart + leaves; ++i)
    {
        cout << "  Leaf " << (i - leafStart) << ": ";
        print_uint128(dpf.V1[i]);
        cout << (dpf.T1[i] ? " [flag=1]" : " [flag=0]") << "\n";
    }

    // cout << "\nFinal correction word fCW: ";
    // print_uint128(fCW);
    // cout << "\n\nTarget leaf XOR check:\n";
    // cout << "  V0[target] = ";
    // print_uint128(dpf.V0[targetLeaf]);
    // cout << "\n  V1[target] = ";
    // print_uint128(dpf.V1[targetLeaf]);
    // cout << "\n  XOR result = ";
    // print_uint128(dpf.V0[targetLeaf] ^ dpf.V1[targetLeaf]);
    // cout << "\n  Expected   = ";
    // print_uint128((u128)dpf.targetValue);
    // cout << "\n\n";
}

int main()
{
    int dpf_size, num_dpfs;
    cout << "Enter DPF size (domain size, will be rounded up to next power-of-two) and number of dpfs: ";
    cin >> dpf_size >> num_dpfs;
    if (dpf_size <= 0)
        return 0;

    auto &genLocation = rngLocation();
    auto &genValue = rngValue();
    // uniform_int_distribution<uint64_t> distLocation(0, (uint64_t)max(1, dpf_size) - 1);
    uniform_int_distribution<uint64_t> dist(0, dpf_size - 1);
    uniform_int_distribution<int64_t> dist2(std::numeric_limits<int64_t>::min(),
                                            std::numeric_limits<int64_t>::max());

    int height = (int)ceil(log2((double)max(1, dpf_size)));
    int leaves = 1 << height;
    int totalNodes = 2 * leaves - 1;
    cout << "Total nodes (tree with leaves = " << leaves << "): " << totalNodes << endl;

    for (int i = 0; i < num_dpfs; ++i)
    {
        DpfKey dpf;
        int location = dist(genLocation); //(int)distLocation(genLocation);
        int value = dist2(genValue);      // deterministic target value for testing
        dpf.targetLocation = location;
        dpf.targetValue = value;

        cout << "\n=== Generating DPF #" << (i + 1) << " ===\n";
        cout << "Target Index: " << location << "  Target Value: " << value << "\n";

        generateDPF(dpf, dpf_size);

        // Evaluate both keys and check full function
        EvalFull(dpf, dpf_size);
        cout << "====================================\n";
    }

    return 0;
}
