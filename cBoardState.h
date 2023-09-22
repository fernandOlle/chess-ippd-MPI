#ifndef BOARDSTATE_H_
#define BOARDSTATE_H_

#include <ctype.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "cHash.h"
#include "mpi.h"
#include "types.h"

extern int rank, worldSize;
extern MPI_Datatype customType;
extern int pawnSquare[64], knightSquare[64], bishopSquare[64];
extern int rookSquare[64], queenSquare[64], kingSquare[64], kingEndGame[64];

class cBoardState {
 private:
  int mTurn;   // turn counter
  int mState;  // gameState value from above
  int mWScore;
  int mBScore;  // total board score, see computeValidMoves()
  int mBoard[8][8];
  int mEnPassant[2];  // coordinates of piece that can be captured en passant
  int mWLostCastle;   // what turn did white lose castling ability?
  int mBLostCastle;
  int mEndGame;
  int mNumCheckMoves;  // number of moves the ai checked, for fun

  bool mWCheck;  // is white in check?
  bool mBCheck;

  static cHash *mHash;

  std::vector<gameMove> mMoveList;  // list of moves that have been played
  std::vector<gameMove>
      mValidList;  // list of legal moves that can be played this turn

  void fAddPieceMoves(int i, int j);  // add moves for the piece at the given coordinates
  void fAddPawnMoves(int i, int j);
  void fAddKnightMoves(int i, int j);
  void fAddHorVertMoves(int i, int j);  // add horizontal and vertical moves for piece
  void fAddDiagMoves(int i, int j);  // add diagonal moves
  void fAddKingMoves(int i, int j);
  void fAddMove(int i, int j, int i2, int j2, int list);  // add move to the given list
  void fPrintMoves(int list);
  std::string fPrintPiece(int piece);
  void fMove(gameMove *m);  // perform the given move on the board
  void fIsInCheck();     // update values if current player's king is in check
  void fRemoveChecks();  // remove moves that result in check
  bool fCanMove(int i, int j);
  bool fMoveIsValid(gameMove *m);
  bool fCheckMoves(gameMove *m);  // check if the given move is in the validList

  mpiBoardState *fToStruct();
  void fMPIGetBoardState();  // for MPI compute threads to get board from root thread

  // alpha beta pruning, used to find the "best" move the ai can make
  int fAlphaBeta(gameMove *m, int maxPlayer, int alpha, int beta, int depth, int &checkMoves);

  // driver function for alpha beta used by threads
  static void *fAlphaBetaDriver(void *args);

 public:
  cBoardState();
  cBoardState(const cBoardState &b2);
  cBoardState(mpiBoardState *m);

  void fComputeValidMoves();
  void fPrintBoard();
  void fProcessMove(gameMove *m);
  void fUndoMove();
  void fCleanup();

  void fMPISendBoardState();
  gameMove *fMPIGetBestMove();
  void fAiCalculateMove();

  int fGetState();
  int fGetTurn();
  int fGetCheckMoves();
  int fGetListCount(int list);
};

typedef struct threadArgs {
  cBoardState *bs;
  int start;
  int stop;
} threadArgs;

#endif /* BOARDSTATE_H_ */
