#include <iostream>
#include <random>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <array>

using namespace std;
using u128 = unsigned __int128;

random_device rd;
// only one can be used here but for result reproducability we can give choosen seed in place of rd()
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

void print_target_value(u128 value)
{
    print_uint128(value);
    if (value <= (u128)std::numeric_limits<int64_t>::max())
        cout << " (signed: " << (int64_t)value << ")";
    else
        cout << " (signed: " << (int64_t)(value - (u128(1) << 64)) << ")";
}

// gives binary vector of a number
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

void handleErrors()
{
    ERR_print_errors_fp(stderr);
    abort();
}

// PRG : gives output 2 child seeds for a single parent seed
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

vector<u128> prgVector(u128 seed, int dim)
{
    vector<u128> out(dim);
    array<uint8_t, 16> key{};
    for (int k = 0; k < 16; k++)
        key[15 - k] = seed >> (8 * k);

    for (int i = 0; i < dim; i++)
    {
        uint8_t in[16] = {0};
        // Store counter (supports dim up to 2^32)
        for (int j = 0; j < 4; ++j)
            in[15 - j] = (i >> (8 * j)) & 0xFF;

        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
            handleErrors();
        if (EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), nullptr, key.data(), nullptr) != 1)
            handleErrors();
        EVP_CIPHER_CTX_set_padding(ctx, 0);

        uint8_t outbuf[16];
        int outlen = 0;
        if (EVP_EncryptUpdate(ctx, outbuf, &outlen, in, 16) != 1)
            handleErrors();
        EVP_CIPHER_CTX_free(ctx);

        array<uint8_t, 16> arr{};
        for (int b = 0; b < 16; b++)
            arr[b] = outbuf[b];
        out[i] = bytesToUint128(arr);
    }
    return out;
}

struct DpfKey
{
    int targetLocation;
    vector<u128> M1, M2; // target value at target location for respective tree
    vector<u128> V0, V1; // vector containing nodes of tree for respective tree, size= total number of nodes
    vector<bool> T0, T1; // vector containing flag bits for each node, size= total number of nodes
    vector<u128> CW;     // vector containing correction word per level, size= height of tree+1
    vector<u128> FCW0;   // final correction word at leaf level for party 0
    vector<u128> FCW1;   // final correction word at leaf level for party 1
};

vector<vector<u128>> evalDPF(u128 rootSeed, vector<bool> &T, vector<u128> &CW, int dpf_size, vector<u128> &FCW, vector<u128> &M)
{
    int height = (int)(ceil(log2(dpf_size)));
    int leaves = 1 << (height);
    int totalNodes = 2 * leaves - 1;
    int lastParentIdx = (totalNodes - 2) / 2;
    vector<u128> VShare(totalNodes);
    VShare[0] = rootSeed;
    vector<vector<u128>> result(leaves);

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

    // modify fcw vector and echange with other party
    int rowLength = M.size();
    vector<u128> P0(rowLength), P1(rowLength);
    for (int i = 0; i < rowLength; i++)
    {
        P0[i] = FCW[i] - M[i];
    }
    vector<u128> fcwm(rowLength);

    for (int i = 0; i < rowLength; i++)
    {
        fcwm[i] = P0[i] + P1[i];
    }

    // Apply final correction word to target leaf
    int leafStart = (1 << (height)) - 1;
    for (int i = 0; i < dpf_size; i++)
    {

        vector<u128> currLeaf(rowLength);
        // result[i] = VShare[leafStart + i];
        currLeaf = prgVector(VShare[i+leafStart], rowLength);
        for (int j = 0; j < rowLength; j++)
            if (T[leafStart + i])
            {
                currLeaf[j] += fcwm[j];
            }

        result.push_back(currLeaf);
    }

    return result;
}

void EvalFull(DpfKey &dpf, int dpf_size, int totaldpfs)
{
    int targetLocation = dpf.targetLocation;
    vector<u128> V0 = dpf.V0, V1 = dpf.V1;
    vector<bool> T0 = dpf.T0, T1 = dpf.T1;
    vector<u128> CW = dpf.CW;
    vector<u128> FCW0 = dpf.FCW0;
    vector<u128> FCW1 = dpf.FCW1;
    vector<u128> M0,M1; // will integrate M later for now using dummy M

    vector<vector<u128>> eval0, eval1;
    eval0 = evalDPF(V0[0], T0, CW, dpf_size, FCW0,M0);
    eval1 = evalDPF(V1[0], T1, CW, dpf_size, FCW1,M1);

}

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
            pair<u128, u128> childSeed0 = prg2(dpf.V0[parentIdx]); // tree 0
            pair<u128, u128> childSeed1 = prg2(dpf.V1[parentIdx]); // for tree 1
            dpf.V0[leftChildIdx] = childSeed0.first;               // left child
            dpf.V0[rightChildIdx] = childSeed0.second;             // right child
            dpf.V1[leftChildIdx] = childSeed1.first;
            dpf.V1[rightChildIdx] = childSeed1.second;
        }

        // now we will compute Lb and Rb (xor of all left and right child of level)
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
        dpf.CW[childLevel] = ((binOfLocation[l] * (l0 ^ l1)) ^ ((1 ^ binOfLocation[l]) * (r0 ^ r1)));

        // now we compute flag and flag correction
        // control bits for flag  or flag correction
        bool cwtl0, cwtr0, cwtl1, cwtr1;

        cwtl0 = l0 & 1;
        cwtr0 = r0 & 1;
        cwtl1 = l1 & 1;
        cwtr1 = r1 & 1;

        bool cwtl, cwtr;
        cwtl = (cwtl0 ^ cwtl1 ^ binOfLocation[l] ^ 1);
        cwtr = (cwtr0 ^ cwtr1 ^ binOfLocation[l]);

        // compute child flags and apply flag correction and correction word
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

    int leafStart = (1 << (height)) - 1;
    int targetLeaf = leafStart + dpf.targetLocation;

    int k = dpf.M1.size(); // elements in 1 row in item matrix
    vector<u128> leafVecAtTarget0 = prgVector(dpf.V0[targetLeaf], k);
    vector<u128> leafVecAtTarget1 = prgVector(dpf.V1[targetLeaf], k);
    dpf.FCW0 = leafVecAtTarget0;
    dpf.FCW1 = leafVecAtTarget1;
}

int main()
{

    int dpf_size, num_dpfs;
    cout << "Enter DPF size (e.g 2, 4, 8...) and number of dpfs: ";
    cin >> dpf_size >> num_dpfs;

    while (dpf_size % 2 != 0 || dpf_size <= 0)
    {
        cout << "Please enter dpf size in multiple of 2 and at least 2 try again:" << endl;
        cout << "Enter DPF size (e.g 2, 4, 8...) again: ";
        cin >> dpf_size;
    }
    uniform_int_distribution<uint64_t> dist(0, dpf_size - 1);
    uniform_int_distribution<int64_t> dist2(std::numeric_limits<int64_t>::min(),
                                            std::numeric_limits<int64_t>::max());
    auto &genLocation = rngLocation();
    auto &genValue = rngValue();

    int totalNodes = 2 * dpf_size - 1;
    cout << "Total nodes: " << totalNodes << endl;
    cout << "Total dpfs: " << num_dpfs << endl;
    for (int i = 0; i < num_dpfs; i++)
    {
        cout << endl
             << "For dpf " << i + 1 << " :" << endl;
        DpfKey dpf;
        int location;
        int64_t value;
        location = dist(genLocation);
        dpf.targetLocation = location;
        dpf.M1 = {32, 5, 7, 15};
        dpf.M2 = {45, 8, 9, 20};
        dpf.V0.resize(totalNodes, (u128)0);
        dpf.V1.resize(totalNodes, (u128)0);
        dpf.T0.resize(totalNodes, false);
        dpf.T1.resize(totalNodes, false);
        int height = (int)(ceil(log2(dpf_size)));
        dpf.CW.resize(height + 2, (u128)0);
        cout << "height :" << height << endl;
        generateDPF(dpf, dpf_size);
        EvalFull(dpf, dpf_size, num_dpfs);
    }

    return 0;

    /*
    run below 2 commands to execute the code
    g++ gen_queries.cpp -o a -lssl -lcrypto
    ./a
    */
}
