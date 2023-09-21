#ifndef TYPES_H_
#define TYPES_H_

//values that are stored in the board
enum piece {W_KING = -6, W_QUEEN, W_ROOK, W_BISHOP, W_KNIGHT, W_PAWN,
            EMPTY = 0, B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING};

//Player v Player, Player v AI, AI v AI
enum gameMode {PVP = 0, PVA, AVA};

enum list {MOVELIST = 0, VALIDLIST};

enum gameState {PLAYING = 0, DRAW, WHITEWON, BLACKWON};

enum stateType {EXACT, ALPHA, BETA};

typedef struct hashState
{
  int ply;                 //how deep we searched
  int score;               //score of board, stored as Black - White
  unsigned long long hash; //hash of the zorbist key
  int type;
} hashState;

//same as cBoardState but to transfer over MPI
typedef struct mpiBoardState
{
  int mBoard[64];
  int mEnPassant[2];
  int mTurn;
  int mState;
  int mWScore;
  int mBScore;
  int mWLostCastle;
  int mBLostCastle;
  int mWCheck;
  int mBCheck;
  int mEndGame;
} mpiBoardState;

typedef struct gameMove
{
  int fromI, fromJ; //start and end squares
  int toI, toJ;
  int piece, capture; //piece that moved and piece that was captured
  int score;
}gameMove;

#endif
