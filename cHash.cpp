#include "cHash.h"

sem_t gHashTableAccess;

// http://www.cplusplus.com/reference/random/mersenne_twister_engine/operator()/
cHash::cHash(int board[][8])
{
    int i, j, index;
    std::random_device r;
    std::mt19937_64 gen(r()); // the seed doesnt really matter

    mHashTableSize = (unsigned long long)(2) << 33; // 8 gigs (~1 billion hashes) lmao change this to fit your system
    mBoardHash = 0;

    for (i = 0; i < 832; i++)
    {
        mPieceHashes[i] = gen();
    }

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
        {
            // each piece type gets 64 values in the array
            //      offset to piece's chunk         offset to position
            index = ((board[i][j] + 6) << 6) + ((i << 3) * j);
            mBoardHash ^= mPieceHashes[index];
        }
}

void cHash::fInitSemaphore()
{
    if (sem_init(&gHashTableAccess, 0, 1) == -1)
    {
        std::cout << "Rank " << rank << ": Could not initialize semaphore" << std::endl;
        exit(-1);
    }
}

void cHash::fSetBoard(int board[][8])
{
    int index, i, j;
    mBoardHash = 0;

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
        {
            // each piece type gets 64 values in the array
            //      offset to piece's chunk         offset to position
            index = ((board[i][j] + 6) << 6) + ((i << 3) * j);
            mBoardHash ^= mPieceHashes[index];
        }
}

void cHash::fUpdateHash(gameMove *m, unsigned long long &hash)
{
    int index;
    int len = m->toJ - m->fromJ;

    // remove hash of piece that moved
    index = ((m->piece + 6) << 6) + ((m->fromI << 3) * m->fromJ);
    hash ^= mPieceHashes[index];

    // add hash of empty piece at the original location
    index = ((EMPTY + 6) << 6) + ((m->fromI << 3) * m->fromJ);
    hash ^= mPieceHashes[index];

    // remove hash of the captured piece
    index = ((m->capture + 6) << 6) + ((m->toI << 3) * m->toJ);
    hash ^= mPieceHashes[index];

    // check for pawn promotion, place queen instead of pawn
    if (m->piece == W_PAWN && m->toI == 0)
    {
        index = ((W_QUEEN + 6) << 6) + ((m->toI << 3) * m->toJ);
        hash ^= mPieceHashes[index];
    }

    else if (m->piece == B_PAWN && m->toI == 7)
    {
        index = ((B_QUEEN + 6) << 6) + ((m->toI << 3) * m->toJ);
        hash ^= mPieceHashes[index];
    }

    // check for en passant, remove captured pawn hash, add empty hash, place pawn
    else if (abs(m->piece) == B_PAWN && m->fromJ != m->toJ && m->capture == EMPTY) // pawn moved diagonally to an empty square -> en passant capture
    {
        index = ((-(m->piece) + 6) << 6) + ((m->fromI << 3) * m->toJ);
        hash ^= mPieceHashes[index];

        index = ((EMPTY + 6) << 6) + ((m->fromI << 3) * m->toJ);
        hash ^= mPieceHashes[index];

        index = ((m->piece + 6) << 6) + ((m->toI << 3) * m->toJ);
        hash ^= mPieceHashes[index];
    }

    // check for castling
    else if (abs(m->piece) == B_KING && abs(len) == 2)
    {
        if (len > 0) // castle to the right
        {
            // remove castle hash
            index = (((m->piece > 0 ? B_ROOK : W_ROOK) + 6) << 6) + ((m->fromI << 3) * (m->toJ + 1));
            hash ^= mPieceHashes[index];

            // add empty hash
            index = ((EMPTY + 6) << 6) + ((m->fromI << 3) * (m->toJ + 1));
            hash ^= mPieceHashes[index];

            // remove empty hash of square castle moved to
            index = ((EMPTY + 6) << 6) + ((m->fromI << 3) * (m->toJ - 1));
            hash ^= mPieceHashes[index];

            // add castle hash at new location
            index = (((m->piece > 0 ? B_ROOK : W_ROOK) + 6) << 6) + ((m->fromI << 3) * (m->toJ - 1));
            hash ^= mPieceHashes[index];

            // add king hash
            index = ((m->piece + 6) << 6) + ((m->toI << 3) * m->toJ);
            hash ^= mPieceHashes[index];
        }

        else // castle to the left
        {
            // remove castle hash
            index = (((m->piece > 0 ? B_ROOK : W_ROOK) + 6) << 6) + ((m->fromI << 3) * (m->toJ - 2));
            hash ^= mPieceHashes[index];

            // add empty hash
            index = ((EMPTY + 6) << 6) + ((m->fromI << 3) * (m->toJ - 2));
            hash ^= mPieceHashes[index];

            // remove empty hash of square castle moved to
            index = ((EMPTY + 6) << 6) + ((m->fromI << 3) * (m->toJ + 1));
            hash ^= mPieceHashes[index];

            // add castle hash at new location
            index = (((m->piece > 0 ? B_ROOK : W_ROOK) + 6) << 6) + ((m->fromI << 3) * (m->toJ + 1));
            hash ^= mPieceHashes[index];

            // add king hash
            index = ((m->piece + 6) << 6) + ((m->toI << 3) * m->toJ);
            hash ^= mPieceHashes[index];
        }
    }

    else
    {
        // add hash of the piece at the new location
        index = ((m->piece + 6) << 6) + ((m->toI << 3) * m->toJ);
        hash ^= mPieceHashes[index];
    }
}

hashState *cHash::fFindState(unsigned long long &hash)
{
    unsigned long long key = hash % mHashTableSize;

    if (mHashTable.empty())
        return nullptr;

    std::unordered_map<unsigned long long, hashState>::iterator it = mHashTable.find(key);

    // not found
    if (it == mHashTable.end())
        return nullptr;

    else
        return &(it->second);
}

void cHash::fAddState(hashState &state)
{
    unsigned long long key = state.hash % mHashTableSize;

    //  std::cout << "Rank " << rank << "- Thread " << pthread_self() << ": waiting to add to map" << std::endl;
    sem_wait(&gHashTableAccess);

    mHashTable.emplace(key, state);

    sem_post(&gHashTableAccess);
    // std::cout << "Rank " << rank << "- Thread " << pthread_self() << ": added to map size = " << mHashTable.size() << std::endl;
}

unsigned long long cHash::fBoardToHash(int board[][8])
{
    int index, i, j;
    unsigned long long hash = 0;
    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 8; j++)
        {
            index = ((board[i][j] + 6) << 6) + ((i << 3) * j);
            hash ^= mPieceHashes[index];
        }
    }

    return hash;
}
