#include <iostream>
#include <random>
#include <vector>
#include <cmath>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <array>
#include <iomanip>

using namespace std;
using u128 = unsigned __int128;

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
        val = (val << 8) | arr[i];
    return val;
}

void print_uint128(u128 value)
{
    if (value == 0) {
        cout << "0";
        return;
    }
    
    array<uint8_t, 16> bytes;
    for (int i = 0; i < 16; i++) {
        bytes[15 - i] = value & 0xFF;
        value >>= 8;
    }
    
    cout << "0x";
    for (int i = 0; i < 16; i++) {
        cout << hex << setw(2) << setfill('0') << (int)bytes[i];
    }
    cout << dec;
}

vector<bool> numToBitVector(uint64_t num, int bit_length)
{
    vector<bool> bits(bit_length, false);
    for (int i = 0; i < bit_length; i++)
    {
        bits[bit_length - 1 - i] = (num >> i) & 1;
    }
    return bits;
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
    int targetLocation;
    u128 targetValue;
    vector<u128> seeds0, seeds1;
    vector<bool> flags0, flags1;
    vector<u128> correctionWords;
};

vector<u128> evalFull(const DpfKey &key, int party, int domainSize)
{
    int height = (int)(ceil(log2(domainSize)));
    const vector<u128>& seeds = (party == 0) ? key.seeds0 : key.seeds1;
    const vector<bool>& flags = (party == 0) ? key.flags0 : key.flags1;
    const vector<u128>& CW = key.correctionWords;
    
    vector<u128> currentLevel = {seeds[0]};
    vector<bool> currentFlags = {flags[0]};
    
    for (int level = 0; level < height; level++)
    {
        vector<u128> nextLevel;
        vector<bool> nextFlags;
        
        for (int i = 0; i < currentLevel.size(); i++)
        {
            auto children = prg2(currentLevel[i]);
            u128 left = children.first;
            u128 right = children.second;
            
            // Apply correction word if flag is set
            if (currentFlags[i])
            {
                left ^= CW[level];
                right ^= CW[level];
            }
            
            nextLevel.push_back(left);
            nextLevel.push_back(right);
            
            // Compute flags for next level
            bool currentFlag = currentFlags[i];
            nextFlags.push_back((left & 1) ^ currentFlag);
            nextFlags.push_back((right & 1) ^ currentFlag);
        }
        
        currentLevel = nextLevel;
        currentFlags = nextFlags;
    }
    
    // Apply final correction to leaves
    vector<u128> result(domainSize);
    for (int i = 0; i < domainSize; i++)
    {
        result[i] = currentLevel[i];
        if (currentFlags[i])
        {
            result[i] ^= key.correctionWords[height];
        }
    }
    
    return result;
}

void testDPF(const DpfKey &key, int domainSize)
{
    cout << "\nTesting DPF:" << endl;
    cout << "Target index: " << key.targetLocation << endl;
    cout << "Target value: ";
    print_uint128(key.targetValue);
    cout << endl;
    
    vector<u128> eval0 = evalFull(key, 0, domainSize);
    vector<u128> eval1 = evalFull(key, 1, domainSize);
    
    bool allCorrect = true;
    for (int i = 0; i < domainSize; i++)
    {
        u128 combined = eval0[i] ^ eval1[i];
        
        if (i == key.targetLocation)
        {
            if (combined == key.targetValue)
            {
                cout << "Index " << i << " (TARGET): ✓ Correct" << endl;
            }
            else
            {
                cout << "Index " << i << " (TARGET): ✗ WRONG - Got ";
                print_uint128(combined);
                cout << " expected ";
                print_uint128(key.targetValue);
                cout << endl;
                allCorrect = false;
            }
        }
        else
        {
            if (combined == 0)
            {
                cout << "Index " << i << ": ✓ Zero" << endl;
            }
            else
            {
                cout << "Index " << i << ": ✗ NON-ZERO - Value: ";
                print_uint128(combined);
                cout << endl;
                allCorrect = false;
            }
        }
    }
    
    if (allCorrect)
    {
        cout << "\n✓ DPF TEST PASSED!" << endl;
    }
    else
    {
        cout << "\n✗ DPF TEST FAILED!" << endl;
    }
}

void generateDPF(DpfKey &key, int domainSize)
{
    uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
    auto &genShare = rngShare();
    
    int height = (int)(ceil(log2(domainSize)));
    
    // Initialize seeds with proper LSB
    u128 seed0 = ((u128)dist(genShare) << 64) | dist(genShare);
    u128 seed1 = ((u128)dist(genShare) << 64) | dist(genShare);
    
    seed0 &= ~(u128)1;  // LSB = 0
    seed1 |= (u128)1;   // LSB = 1
    
    key.seeds0 = {seed0};
    key.seeds1 = {seed1};
    key.flags0 = {false};
    key.flags1 = {true};
    key.correctionWords.resize(height + 1, 0);
    
    vector<bool> targetBits = numToBitVector(key.targetLocation, height);
    
    cout << "Target index " << key.targetLocation << " in binary: ";
    for (bool b : targetBits) cout << b;
    cout << endl;
    
    // Generate the tree level by level
    vector<u128> currentSeeds0 = {seed0};
    vector<u128> currentSeeds1 = {seed1};
    vector<bool> currentFlags0 = {false};
    vector<bool> currentFlags1 = {true};
    
    for (int level = 0; level < height; level++)
    {
        vector<u128> nextSeeds0, nextSeeds1;
        vector<bool> nextFlags0, nextFlags1;
        
        u128 L0 = 0, L1 = 0, R0 = 0, R1 = 0;
        
        // Expand current level
        for (int i = 0; i < currentSeeds0.size(); i++)
        {
            auto children0 = prg2(currentSeeds0[i]);
            auto children1 = prg2(currentSeeds1[i]);
            
            nextSeeds0.push_back(children0.first);
            nextSeeds0.push_back(children0.second);
            nextSeeds1.push_back(children1.first);
            nextSeeds1.push_back(children1.second);
            
            L0 ^= children0.first;
            L1 ^= children1.first;
            R0 ^= children0.second;
            R1 ^= children1.second;
        }
        
        // Compute correction word for this level
        if (targetBits[level] == 0)
        {
            key.correctionWords[level] = R0 ^ R1;
        }
        else
        {
            key.correctionWords[level] = L0 ^ L1;
        }
        
        // Compute flag correction bits
        bool cwt_L = ((L0 & 1) ^ (L1 & 1) ^ targetBits[level] ^ 1);
        bool cwt_R = ((R0 & 1) ^ (R1 & 1) ^ targetBits[level]);
        
        cout << "Level " << level << " (bit " << targetBits[level] << "): CW=";
        print_uint128(key.correctionWords[level]);
        cout << ", cwt_L=" << cwt_L << ", cwt_R=" << cwt_R << endl;
        
        // Apply corrections and compute flags
        for (int i = 0; i < currentSeeds0.size(); i++)
        {
            bool flag0 = currentFlags0[i];
            bool flag1 = currentFlags1[i];
            
            // Apply correction to party 0
            if (flag0)
            {
                nextSeeds0[2*i] ^= key.correctionWords[level];
                nextSeeds0[2*i + 1] ^= key.correctionWords[level];
            }
            
            // Apply correction to party 1
            if (flag1)
            {
                nextSeeds1[2*i] ^= key.correctionWords[level];
                nextSeeds1[2*i + 1] ^= key.correctionWords[level];
            }
            
            // Compute flags for next level
            nextFlags0.push_back((nextSeeds0[2*i] & 1) ^ (flag0 && cwt_L));
            nextFlags0.push_back((nextSeeds0[2*i + 1] & 1) ^ (flag0 && cwt_R));
            nextFlags1.push_back((nextSeeds1[2*i] & 1) ^ (flag1 && cwt_L));
            nextFlags1.push_back((nextSeeds1[2*i + 1] & 1) ^ (flag1 && cwt_R));
        }
        
        // Store all seeds and flags for evaluation
        for (u128 seed : nextSeeds0) key.seeds0.push_back(seed);
        for (u128 seed : nextSeeds1) key.seeds1.push_back(seed);
        for (bool flag : nextFlags0) key.flags0.push_back(flag);
        for (bool flag : nextFlags1) key.flags1.push_back(flag);
        
        currentSeeds0 = nextSeeds0;
        currentSeeds1 = nextSeeds1;
        currentFlags0 = nextFlags0;
        currentFlags1 = nextFlags1;
    }
    
    // Compute final correction word
    int leafStart = (1 << height) - 1;
    u128 leaf0 = key.seeds0[leafStart + key.targetLocation];
    u128 leaf1 = key.seeds1[leafStart + key.targetLocation];
    u128 current_xor = leaf0 ^ leaf1;
    key.correctionWords[height] = key.targetValue ^ current_xor;
    
    cout << "Final CW: current_xor=";
    print_uint128(current_xor);
    cout << ", target=";
    print_uint128(key.targetValue);
    cout << ", CW=";
    print_uint128(key.correctionWords[height]);
    cout << endl;
}

int main()
{
    int domainSize, numDPFs;
    cout << "Enter domain size (power of 2) and number of DPFs: ";
    cin >> domainSize >> numDPFs;

    // Verify domainSize is power of 2
    if ((domainSize & (domainSize - 1)) != 0)
    {
        cout << "Error: Domain size must be a power of 2" << endl;
        return 1;
    }

    int height = (int)(log2(domainSize));
    cout << "Domain size: " << domainSize << " (2^" << height << ")" << endl;
    cout << "Bit length: " << height << endl;

    uniform_int_distribution<uint64_t> dist_loc(0, domainSize - 1);
    uniform_int_distribution<uint64_t> dist_val(0, UINT64_MAX);
    auto &genLocation = rngLocation();
    auto &genValue = rngValue();

    for (int i = 0; i < numDPFs; i++)
    {
        DpfKey key;
        key.targetLocation = dist_loc(genLocation);
        key.targetValue = ((u128)dist_val(genValue) << 64) | dist_val(genValue);

        cout << "\n=== Generating DPF " << (i + 1) << " ===" << endl;
        generateDPF(key, domainSize);
        testDPF(key, domainSize);
    }

    return 0;
}