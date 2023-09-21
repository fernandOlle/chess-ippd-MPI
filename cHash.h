#ifndef HASH_H_
#define HASH_H_

#include <random>
#include <unordered_map>
#include <iostream>
#include <semaphore.h>
#include "types.h"

extern int rank;

class cHash
{
private:
  unsigned long long mPieceHashes[832]; // 13 pieces * 64 board positions
  unsigned long long mBoardHash;
  unsigned long long mHashTableSize;
  std::unordered_map<unsigned long long, hashState> mHashTable;

public:
  cHash(int board[][8]);
  void fInitSemaphore();
  unsigned long long fGetHash();
  void fSetBoard(int board[][8]);
  void fUpdateHash(gameMove *m, unsigned long long &hash);
  hashState *fFindState(unsigned long long &hash);
  void fAddState(hashState &s);
  unsigned long long fBoardToHash(int board[][8]);
};

#endif
