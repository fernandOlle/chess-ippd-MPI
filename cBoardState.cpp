#include "cBoardState.h"

int gNodeBest;      // melhor valor do nodo
int gIndex;         // index do melhor movimento
sem_t gBestAccess;  // semaforo para as variaveis acima
sem_t gMoveNumAccess;

cHash *cBoardState::mHash;

bool moveCompare(gameMove m, gameMove m2) { return (m.score < m2.score); }

cBoardState::cBoardState() {
  int board[8][8] = {
      {B_ROOK, B_KNIGHT, B_BISHOP, B_QUEEN, B_KING, B_BISHOP, B_KNIGHT, B_ROOK},
      {B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN},
      {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
      {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
      {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
      {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
      {W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN},
      {W_ROOK, W_KNIGHT, W_BISHOP, W_QUEEN, W_KING, W_BISHOP, W_KNIGHT,
       W_ROOK}};

  memcpy(mBoard, board, sizeof(board));

  mTurn = 0;
  mState = PLAYING;
  mWScore = 0;
  mBScore = 0;
  mEnPassant[0] = -1;
  mEnPassant[1] = -1;
  mBLostCastle = -1;
  mWLostCastle = -1;

  mBCheck = false;
  mWCheck = false;
  mEndGame = false;

  mHash = new cHash(board);
  mHash->fInitSemaphore();

  if (sem_init(&gBestAccess, 0, 1) == -1 ||
      sem_init(&gMoveNumAccess, 0, 1) == -1) {
    std::cout << "Rank " << rank << ": Nao conseguiu ciar semaforo" << std::endl;
    exit(-1);
  }
}

cBoardState::cBoardState(const cBoardState &b2) {
  memcpy(mBoard, b2.mBoard, sizeof(b2.mBoard));
  memcpy(mEnPassant, b2.mEnPassant, sizeof(b2.mEnPassant));

  mTurn = b2.mTurn;
  mState = b2.mState;
  mWScore = b2.mWScore;
  mBScore = b2.mBScore;
  mBCheck = b2.mBCheck;
  mWCheck = b2.mWCheck;
  mBLostCastle = b2.mBLostCastle;
  mWLostCastle = b2.mWLostCastle;
  mEndGame = b2.mEndGame;
}

cBoardState::cBoardState(mpiBoardState *m) {
  int i, j;

  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++) mBoard[i][j] = m->mBoard[((i << 3) + j)];

  mTurn = m->mTurn;
  mState = m->mState;
  mWScore = m->mWScore;
  mBScore = m->mBScore;
  mEnPassant[0] = m->mEnPassant[0];
  mEnPassant[1] = m->mEnPassant[1];
  mBCheck = m->mBCheck == 1 ? true : false;
  mWCheck = m->mWCheck == 1 ? true : false;
  mBLostCastle = m->mBLostCastle;
  mWLostCastle = m->mWLostCastle;
  mEndGame = m->mEndGame == 1 ? true : false;
}

void cBoardState::fComputeValidMoves() {
  int i, j, piece, val;

  mWScore = 0;
  mBScore = 0;

  mValidList.clear();

  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++) {
      piece = abs(mBoard[i][j]);
      switch (
          piece) {  // https://en.wikipedia.org/wiki/Chess_piece_relative_value
        case B_KING:
          val = 20000;
          val += mBoard[i][j] > 0 ? mEndGame ? kingEndGame[63 - ((i << 3) + j)]
                                             : kingSquare[63 - ((i << 3) + j)]
                 : mEndGame ? kingEndGame[((i << 3) + j)]
                                  : kingSquare[((i << 3) + j)];
          break;

        case B_QUEEN:
          val = 900;
          val += mBoard[i][j] > 0 ? queenSquare[63 - ((i << 3) + j)]
                                  : queenSquare[((i << 3) + j)];
          break;

        case B_ROOK:
          val = 500;
          val += mBoard[i][j] > 0 ? rookSquare[63 - ((i << 3) + j)]
                                  : rookSquare[((i << 3) + j)];
          break;

        case B_BISHOP:
          val = 330;
          val += mBoard[i][j] > 0 ? bishopSquare[63 - ((i << 3) + j)]
                                  : bishopSquare[((i << 3) + j)];
          break;

        case B_KNIGHT:
          val = 320;
          val += mBoard[i][j] > 0 ? knightSquare[63 - ((i << 3) + j)]
                                  : knightSquare[((i << 3) + j)];
          break;

        case B_PAWN:
          val = 100;
          val += mBoard[i][j] > 0 ? pawnSquare[63 - ((i << 3) + j)]
                                  : pawnSquare[((i << 3) + j)];
          break;

        case EMPTY:
        default:
          val = 0;
      }

      if (mBoard[i][j] > 0)
        mBScore += val;
      else
        mWScore += val;

      if ((mTurn % 2 == 0 && mBoard[i][j] < EMPTY) ||
          (mTurn % 2 == 1 && mBoard[i][j] > EMPTY))
        fAddPieceMoves(i, j);
    }
  }

  if (mWScore <= 21200 || mBScore <= 21200) mEndGame = true;
}

void cBoardState::fAddPieceMoves(int i, int j) {
  int piece = abs(mBoard[i][j]);

  switch (piece) {
    case B_KING:
      fAddKingMoves(i, j);
      break;

    case B_QUEEN:
      fAddDiagMoves(i, j);
      fAddHorVertMoves(i, j);
      break;

    case B_ROOK:
      fAddHorVertMoves(i, j);
      break;

    case B_BISHOP:
      fAddDiagMoves(i, j);
      break;

    case B_KNIGHT:
      fAddKnightMoves(i, j);
      break;

    case B_PAWN:
      fAddPawnMoves(i, j);
      break;
  }
}

void cBoardState::fAddPawnMoves(int i, int j) {
  if (mTurn % 2 == 0)  // brancas
  {
    // move frente 1
    if (mBoard[i - 1][j] == EMPTY && fCanMove(i - 1, j))
      fAddMove(i, j, i - 1, j, VALIDLIST);

    // move frente 2 para peao que nao se moveu
    if (i == 6 && mBoard[i - 1][j] == EMPTY && mBoard[i - 2][j] == EMPTY)
      fAddMove(i, j, i - 2, j, VALIDLIST);

    // ataque frente-direita
    if (mBoard[i - 1][j + 1] > EMPTY && fCanMove(i - 1, j + 1))
      fAddMove(i, j, i - 1, j + 1, VALIDLIST);

    // ataque frente-esquerda
    if (mBoard[i - 1][j - 1] > EMPTY && fCanMove(i - 1, j - 1))
      fAddMove(i, j, i - 1, j - 1, VALIDLIST);

    // en passant direita
    if (mBoard[i - 1][j + 1] == EMPTY && fCanMove(i - 1, j + 1) &&
        mEnPassant[0] == i && mEnPassant[1] == j + 1)
      fAddMove(i, j, i - 1, j + 1, VALIDLIST);

    // en passant esquerda
    if (mBoard[i - 1][j - 1] == EMPTY && fCanMove(i - 1, j - 1) &&
        mEnPassant[0] == i && mEnPassant[1] == j - 1)
      fAddMove(i, j, i - 1, j - 1, VALIDLIST);
  }

  if (mTurn % 2 == 1)  // pretas
  {
    // move frente 1
    if (mBoard[i + 1][j] == EMPTY && fCanMove(i + 1, j))
      fAddMove(i, j, i + 1, j, VALIDLIST);

    // move frente 2 para peao que nao se moveu
    if (i == 1 && mBoard[i + 1][j] == EMPTY && mBoard[i + 2][j] == EMPTY)
      fAddMove(i, j, i + 2, j, VALIDLIST);

    // ataque frente-direita
    if (mBoard[i + 1][j + 1] < EMPTY && fCanMove(i + 1, j + 1))
      fAddMove(i, j, i + 1, j + 1, VALIDLIST);

    // ataque frente-esquerda
    if (mBoard[i + 1][j - 1] < EMPTY && fCanMove(i + 1, j - 1))
      fAddMove(i, j, i + 1, j - 1, VALIDLIST);

    // en passant direita
    if (mBoard[i + 1][j + 1] == EMPTY && fCanMove(i + 1, j + 1) &&
        mEnPassant[0] == i && mEnPassant[1] == j + 1)
      fAddMove(i, j, i + 1, j + 1, VALIDLIST);

    // en passant esquerda
    if (mBoard[i + 1][j - 1] == EMPTY && fCanMove(i + 1, j - 1) &&
        mEnPassant[0] == i && mEnPassant[1] == j - 1)
      fAddMove(i, j, i + 1, j - 1, VALIDLIST);
  }
}

void cBoardState::fAddKnightMoves(int i, int j) {
  // move baixo2-direita1
  if (fCanMove(i + 2, j + 1)) fAddMove(i, j, i + 2, j + 1, VALIDLIST);

  // move baixo2-esquerda1
  if (fCanMove(i + 2, j - 1)) fAddMove(i, j, i + 2, j - 1, VALIDLIST);

  // move esquerda2-baixo1
  if (fCanMove(i + 1, j - 2)) fAddMove(i, j, i + 1, j - 2, VALIDLIST);

  // move esquerda2-cima1
  if (fCanMove(i - 1, j - 2)) fAddMove(i, j, i - 1, j - 2, VALIDLIST);

  // move cima2-esquerda1
  if (fCanMove(i - 2, j - 1)) fAddMove(i, j, i - 2, j - 1, VALIDLIST);

  // move cima2-direita1
  if (fCanMove(i - 2, j + 1)) fAddMove(i, j, i - 2, j + 1, VALIDLIST);

  // move direita2-cima1
  if (fCanMove(i - 1, j + 2)) fAddMove(i, j, i - 1, j + 2, VALIDLIST);

  // move direita2-baixo1
  if (fCanMove(i + 1, j + 2)) fAddMove(i, j, i + 1, j + 2, VALIDLIST);
}

void cBoardState::fAddHorVertMoves(int i, int j) {
  int i2, j2;

  // move cima
  i2 = i - 1;
  j2 = j;
  while (fCanMove(i2, j2)) {
    fAddMove(i, j, i2, j2, VALIDLIST);
    if (mBoard[i2][j2] != EMPTY)  // bloqueado
      break;
    i2--;
  }

  // move direita
  i2 = i;
  j2 = j + 1;
  while (fCanMove(i2, j2)) {
    fAddMove(i, j, i2, j2, VALIDLIST);
    if (mBoard[i2][j2] != EMPTY)  // bloqueado
      break;
    j2++;
  }

  // move baixo
  i2 = i + 1;
  j2 = j;
  while (fCanMove(i2, j2)) {
    fAddMove(i, j, i2, j2, VALIDLIST);
    if (mBoard[i2][j2] != EMPTY)  // bloqueado
      break;
    i2++;
  }

  // move esquerda
  i2 = i;
  j2 = j - 1;
  while (fCanMove(i2, j2)) {
    fAddMove(i, j, i2, j2, VALIDLIST);
    if (mBoard[i2][j2] != EMPTY)  // bloqueado
      break;
    j2--;
  }
}

void cBoardState::fAddDiagMoves(int i, int j) {
  int i2, j2;

  // move cima-direita
  i2 = i - 1;
  j2 = j + 1;
  while (fCanMove(i2, j2)) {
    fAddMove(i, j, i2, j2, VALIDLIST);
    if (mBoard[i2][j2] != EMPTY)  // bloqueado
      break;

    i2--;
    j2++;
  }

  // move baixo-direita
  i2 = i + 1;
  j2 = j + 1;
  while (fCanMove(i2, j2)) {
    fAddMove(i, j, i2, j2, VALIDLIST);
    if (mBoard[i2][j2] != EMPTY)  // bloqueado
      break;
    i2++;
    j2++;
  }

  // move baixo-esquerda
  i2 = i + 1;
  j2 = j - 1;
  while (fCanMove(i2, j2)) {
    fAddMove(i, j, i2, j2, VALIDLIST);
    if (mBoard[i2][j2] != EMPTY)  // bloqueado
      break;
    i2++;
    j2--;
  }

  // move cima-esquerda
  i2 = i - 1;
  j2 = j - 1;
  while (fCanMove(i2, j2)) {
    fAddMove(i, j, i2, j2, VALIDLIST);
    if (mBoard[i2][j2] != EMPTY)  // bloqueado
      break;
    i2--;
    j2--;
  }
}

void cBoardState::fAddKingMoves(int i, int j) {
  // move cima
  if (fCanMove(i - 1, j)) fAddMove(i, j, i - 1, j, VALIDLIST);

  // move cima-direita
  if (fCanMove(i - 1, j + 1)) fAddMove(i, j, i - 1, j + 1, VALIDLIST);

  // move direita
  if (fCanMove(i, j + 1)) fAddMove(i, j, i, j + 1, VALIDLIST);

  // move baixo-direita
  if (fCanMove(i + 1, j + 1)) fAddMove(i, j, i + 1, j + 1, VALIDLIST);

  // move baixo
  if (fCanMove(i + 1, j)) fAddMove(i, j, i + 1, j, VALIDLIST);

  // move baixo-esquerda
  if (fCanMove(i + 1, j - 1)) fAddMove(i, j, i + 1, j - 1, VALIDLIST);

  // move esquerda
  if (fCanMove(i, j - 1)) fAddMove(i, j, i, j - 1, VALIDLIST);

  // move cima-esquerda
  if (fCanMove(i - 1, j - 1)) fAddMove(i, j, i - 1, j - 1, VALIDLIST);

  // roque
  if ((mTurn % 2 == 0 && mWLostCastle == -1) ||
      (mTurn % 2 == 1 && mBLostCastle == -1)) {
    // roque esquerda
    if (mBoard[i][j - 1] == EMPTY && mBoard[i][j - 2] == EMPTY &&
        mBoard[i][j - 3] == EMPTY && abs(mBoard[i][j - 4]) == B_ROOK)
      fAddMove(i, j, i, j - 2, VALIDLIST);

    // roque direita
    if (mBoard[i][j + 1] == EMPTY && mBoard[i][j + 2] == EMPTY &&
        abs(mBoard[i][j + 3]) == B_ROOK)
      fAddMove(i, j, i, j + 2, VALIDLIST);
  }
}

void cBoardState::fAddMove(int i, int j, int i2, int j2, int list) {
  int oldB = mBScore, oldW = mWScore;
  gameMove newMove;
  newMove.fromI = i;
  newMove.fromJ = j;
  newMove.toI = i2;
  newMove.toJ = j2;
  newMove.piece = mBoard[i][j];
  newMove.capture = mBoard[i2][j2];

  mWScore = oldW;
  mBScore = oldB;

  if (list == MOVELIST)
    mMoveList.push_back(newMove);
  else
    mValidList.push_back(newMove);
}

void cBoardState::fPrintMoves(int list) {
  std::vector<gameMove>::iterator it, stop;

  if (list == MOVELIST) {
    it = mMoveList.begin();
    stop = mMoveList.end();
  } else {
    it = mValidList.begin();
    stop = mValidList.end();
  }

  std::cout << "Rank " << rank << " Movimento:" << std::endl;
  for (it; it != stop; it++) {
    std::cout << char(it->fromJ + 'a') << char('8' - it->fromI)
              << char(it->toJ + 'a') << char('8' - it->toI) << std::endl;
  }
}

std::string cBoardState::fPrintPiece(int piece) {
  // unicode
  switch (piece) {
    case W_KING:
      return "\u2654";
    case W_QUEEN:
      return "\u2655";
    case W_ROOK:
      return "\u2656";
    case W_BISHOP:
      return "\u2657";
    case W_KNIGHT:
      return "\u2658";
    case W_PAWN:
      return "\u2659";
    case B_KING:
      return "\u265a";
    case B_QUEEN:
      return "\u265b";
    case B_ROOK:
      return "\u265c";
    case B_BISHOP:
      return "\u265d";
    case B_KNIGHT:
      return "\u265e";
    case B_PAWN:
      return "\u265f";
    case EMPTY:
      return " ";
    default:
      return " ";
  }
}

void cBoardState::fPrintBoard() {
  int i, j;

  std::cout << "     a    b    c    d    e    f    g    h" << std::endl;
  for (i = 0; i < 8; i++) {
    std::cout << "   -----------------------------------------" << std::endl;
    std::cout << " " << (8 - i) << " ";

    for (j = 0; j < 8; j++)
      std::cout << "| " << fPrintPiece(mBoard[i][j]) << "  ";

    std::cout << "| " << (8 - i) << std::endl;
  }

  std::cout << "   -----------------------------------------" << std::endl;
  std::cout << "     a    b    c    d    e    f    g    h" << std::endl;
}

void cBoardState::fProcessMove(gameMove *m) {
  if (fMoveIsValid(m)) {
    fMove(m);      // move e muda turno
    fIsInCheck();  // verifica se player esta em cheque
    fComputeValidMoves();
    fRemoveChecks();
  } else {
    std::cout << "Movimento invalido!" << std::endl;
    sleep(2);
  }
}

void cBoardState::fMove(gameMove *m) {
  int piece = mBoard[m->fromI][m->fromJ];
  int capture = mBoard[m->toI][m->toJ];

  fAddMove(m->fromI, m->fromJ, m->toI, m->toJ,
           MOVELIST);  // acompanha todos os movimentos

  mBoard[m->fromI][m->fromJ] = EMPTY;  // move peca
  mBoard[m->toI][m->toJ] = piece;

  // verifica se peao pode ser promovido
  if (piece == W_PAWN && m->toI == 0) mBoard[m->toI][m->toJ] = W_QUEEN;

  if (piece == B_PAWN && m->toI == 7) mBoard[m->toI][m->toJ] = B_QUEEN;

  // verifica `en passant`
  if (abs(piece) == B_PAWN && abs(m->toI - m->fromI) == 2) {
    mEnPassant[0] = m->toI;
    mEnPassant[1] = m->toJ;
  } else {
    mEnPassant[0] = -1;
    mEnPassant[1] = -1;
  }

  // move peao `en passant`
  if (abs(piece) == B_PAWN && m->fromJ != m->toJ && capture == EMPTY) {
    int enPCapture = mBoard[m->fromI][m->toJ];
    mBoard[m->fromI][m->toJ] = EMPTY;
  }

  // verifica por roque
  int len = m->toJ - m->fromJ;
  if (abs(piece) == B_KING && abs(len) == 2) {
    if (len > 0) {
      int castle = mBoard[m->fromI][m->toJ + 1];
      mBoard[m->fromI][m->toJ + 1] = EMPTY;
      mBoard[m->fromI][m->toJ - 1] = castle;
    } else {
      int castle = mBoard[m->fromI][m->toJ - 2];
      mBoard[m->fromI][m->toJ - 2] = EMPTY;
      mBoard[m->fromI][m->toJ + 1] = castle;
    }

    if (piece < EMPTY)  // brancas
      mWLostCastle = mTurn;
    else if (piece > EMPTY)  // pretas
      mBLostCastle = mTurn;
  }

  // bloqueia roque
  else if (abs(piece) == B_KING) {
    if (piece < EMPTY && mWLostCastle == -1)  // brancas
      mWLostCastle = mTurn;
    else if (piece > EMPTY && mBLostCastle == -1)  // pretas
      mBLostCastle = mTurn;
  }

  // remove direito de roque
  else if (abs(piece) == B_ROOK) {
    if (piece < EMPTY && mWLostCastle == -1) {
      if (mBoard[7][0] != W_ROOK && mBoard[7][7] != W_ROOK)
        mWLostCastle = mTurn;
    }

    else if (piece > EMPTY && mBLostCastle == -1) {
      if (mBoard[0][0] != B_ROOK && mBoard[0][7] != B_ROOK)
        mWLostCastle = mTurn;
    }
  }

  mTurn++;
}

void cBoardState::fIsInCheck() {
  int i;
  gameMove m;
  cBoardState nextBoard = *this;

  nextBoard.mTurn++;

  // verifica movimentos que o oponente pode realziar
  nextBoard.fComputeValidMoves();

  int loop = nextBoard.mValidList.size();
  for (i = 0; i < loop; i++) {
    if (abs(nextBoard.mValidList[i].capture) == B_KING) {
      if (nextBoard.mTurn % 2 == 0)
        mBCheck = true;

      else if (nextBoard.mTurn % 2 == 1)
        mWCheck = true;

      break;
    } else {
      if (nextBoard.mTurn % 2 == 0)
        mBCheck = false;

      else if (nextBoard.mTurn % 2 == 1)
        mWCheck = false;
    }
  }

  nextBoard.mValidList.clear();
  nextBoard.mMoveList.clear();
}

void cBoardState::fRemoveChecks() {
  int i, j;
  gameMove m;

  cBoardState nextBoard = *this;

  std::vector<gameMove>::iterator it;
  // itera todos os movimentos validos
  for (it = mValidList.begin(); it != mValidList.end(); it++) {
    m.fromI = it->fromI;
    m.fromJ = it->fromJ;
    m.toI = it->toI;
    m.toJ = it->toJ;
    m.piece = it->piece;
    m.capture = it->capture;

    nextBoard.fMove(&m);  // realiza o movimento no board clonado
    nextBoard.fComputeValidMoves();

    std::vector<gameMove>::iterator it2;
    for (it2 = nextBoard.mValidList.begin(); it2 != nextBoard.mValidList.end();
         it2++) {
      // encontrou um cheque
      if (abs(it2->capture) == B_KING) {
        mValidList.erase(it);
        it--;
        break;
      }
    }

    nextBoard.fUndoMove();
  }

  if (mValidList.size() == 0) {
    if (mTurn % 2 == 0 && mWCheck)
      mState = BLACKWON;
    else if (mTurn % 2 == 1 && mBCheck)
      mState = WHITEWON;

    else if ((mTurn % 2 == 0 && !mWCheck) || (mTurn % 2 == 1 && !mBCheck))
      mState = DRAW;
  } else if (mValidList.size() > 0) {
    if (mTurn % 2 == 0)
      mWCheck = false;
    else
      mBCheck = false;
  }

  nextBoard.mValidList.clear();
  nextBoard.mMoveList.clear();
}

void cBoardState::fUndoMove() {
  mTurn--;
  mState = PLAYING;

  // `en passant`
  if (abs(mMoveList.back().piece) == B_PAWN &&
      mMoveList.back().fromJ != mMoveList.back().toJ &&
      mMoveList.back().capture == EMPTY) {
    mBoard[mMoveList.back().fromI][mMoveList.back().toJ] =
        -(mMoveList.back().piece);
    mEnPassant[0] = mMoveList.back().fromI;
    mEnPassant[1] = mMoveList.back().toJ;
  } else {
    mEnPassant[0] = -1;
    mEnPassant[1] = -1;
  }

  // roque
  if (mMoveList.back().piece == B_KING) {
    if (mMoveList.back().toJ - mMoveList.back().fromJ == -2) {
      mBoard[0][0] = B_ROOK;
      mBoard[0][3] = EMPTY;
    } else if (mMoveList.back().toJ - mMoveList.back().fromJ == 2) {
      mBoard[0][7] = B_ROOK;
      mBoard[0][5] = EMPTY;
    }
  } else if (mMoveList.back().piece == W_KING) {
    if (mMoveList.back().toJ - mMoveList.back().fromJ == -2) {
      mBoard[7][0] = W_ROOK;
      mBoard[7][3] = EMPTY;
    } else if (mMoveList.back().toJ - mMoveList.back().fromJ == 2) {
      mBoard[7][7] = W_ROOK;
      mBoard[7][5] = EMPTY;
    }
  }

  if (mBLostCastle == mTurn) mBLostCastle = -1;

  if (mWLostCastle == mTurn) mWLostCastle = -1;

  // defaz o movimento
  mBoard[mMoveList.back().fromI][mMoveList.back().fromJ] =
      mMoveList.back().piece;
  mBoard[mMoveList.back().toI][mMoveList.back().toJ] = mMoveList.back().capture;

  mMoveList.pop_back();
}

void cBoardState::fCleanup() {
  mMoveList.clear();
  mValidList.clear();
}

bool cBoardState::fCanMove(int i, int j) {
  if (i > 7 || j > 7 || i < 0 || j < 0) {
    return false;
  }

  int piece = mBoard[i][j];
  if ((piece > EMPTY && mTurn % 2 == 1) || (piece < EMPTY && mTurn % 2 == 0)) {
    return 0;
  } else if (piece == EMPTY) {
    return true;
  } else {
    return true;
  }
}

bool cBoardState::fMoveIsValid(gameMove *m) {
  m->piece = mBoard[m->fromI][m->fromJ];
  m->capture = mBoard[m->toI][m->toJ];

  if (m->piece == EMPTY)  // tenta mover um quadrado vazio
    return false;

  if (mTurn % 2 == 0 &&
      m->piece > EMPTY)  // branco tenta mover peca preta
    return false;

  if (mTurn % 2 == 1 &&
      m->piece < EMPTY)  // preto tenta mover peca preta
    return false;

  return fCheckMoves(m);
}

bool cBoardState::fCheckMoves(gameMove *m) {
  int i, loop = mValidList.size();
  for (i = 0; i < loop; i++) {
    if (m->fromJ == mValidList[i].fromJ && m->fromI == mValidList[i].fromI &&
        m->toJ == mValidList[i].toJ && m->toI == mValidList[i].toI)
    //  && m->piece == curr->move.piece && m->capture == curr->move.capture)
    //  //dont really need this but its there if you want it
    {
      return true;
    }
  }
  return false;
}

void cBoardState::fAiCalculateMove() {
  int i, score, index;
  int bestScore[2], globalBest[2], player;
  int testNum, remainder, offset, prevOffset;

  threadArgs *args = new threadArgs;
  bestScore[1] =
      rank;  // MPI_MAXLOC vai retornar o maior valor e o rank que enviou

  do {
    fMPIGetBoardState();

    int numMoves = mValidList.size();

    mNumCheckMoves = 0;

    bestScore[0] = -30000;  // IA tenta maximizar esse score. score da ia e
                            // (score da ia - score do oponente)

    // divide igualmente os movimentos entre os processos
    int neededRanks;
    if (numMoves <= 10)
      neededRanks = numMoves;
    else
      neededRanks = 10;

    // confirma que não teremos mais processos do que jogadas
    // processo 0 nunca chega aqui
    if (neededRanks >= rank) {
      int threadsNeeded, movesPerThread;
      testNum = numMoves /
                neededRanks;  // numeros de movimentos que o processo ira estar
      testNum += numMoves % neededRanks >= rank ? 1 : 0;

      if (testNum < 31) {
        threadsNeeded = testNum;
        movesPerThread = 1;
        remainder = 0;
      } else {
        threadsNeeded = 31;
        movesPerThread = testNum / 31;
        remainder = testNum % 31;
      }

      pthread_t threads[threadsNeeded];

      // numero de jogadas extras testadas pelo processo anterior
      prevOffset = 0;

      if (remainder > 0) {
        if (remainder >= rank) {
          offset = 1;
          prevOffset = rank == 1 ? 0 : 1;  // como processo não chega aqui, proc
                                           // 1 não tem prevOffSet
        }
      }

      args->bs = this;
      for (i = 1; i <= threadsNeeded; i++) {
        args->start = ((i - 1) * movesPerThread) + prevOffset;
        args->stop = (i * movesPerThread) + offset;

        int res = pthread_create(&threads[i - 1], NULL, fAlphaBetaDriver, args);

        if (res != 0) exit(-1);
      }

      for (i = 0; i < threadsNeeded; i++) {
        pthread_join(threads[i], NULL);
      }

      globalBest[0] = gNodeBest;

      // sincronizacao antes do reduce
      MPI_Barrier(MPI_COMM_WORLD);

      // distribui o melhor score para todos os processos
      MPI_Allreduce(&bestScore, &globalBest, 1, MPI_2INT, MPI_MAXLOC,
                    MPI_COMM_WORLD);

      // espera todos recebem o dado
      MPI_Barrier(MPI_COMM_WORLD);

      // soma todos os movimentos testasdo pela IA
      MPI_Reduce(&mNumCheckMoves, NULL, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

      // espera rank 0 receber o dado
      MPI_Barrier(MPI_COMM_WORLD);

      // se o processo tem o score mais alto, envia o index do movimento
      if (globalBest[1] == rank) {
        MPI_Send(&gIndex, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
      }
    } else {
      MPI_Barrier(MPI_COMM_WORLD);

      // MPI precisa que todos os processos chamem isso, eles vao ter -9999,
      // entao sem problemas
      MPI_Allreduce(&bestScore, &globalBest, 1, MPI_2INT, MPI_MAXLOC,
                    MPI_COMM_WORLD);

      MPI_Barrier(MPI_COMM_WORLD);

      // soma todos os valores checkados pelos processos de ia
      MPI_Reduce(&mNumCheckMoves, NULL, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

      // espera rank 0 receber o dado
      MPI_Barrier(MPI_COMM_WORLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);

  } while (mState == PLAYING);
}

void *cBoardState::fAlphaBetaDriver(void *args) {
  int score, i, player;
  std::vector<gameMove>::iterator it, bestMove;
  threadArgs *p = (threadArgs *)(args);

  cBoardState simuBoard = *(p->bs);

  if (p->bs->mTurn % 2 == 0)
    player = W_PAWN;
  else
    player = B_PAWN;

  for (i = p->start; i < p->stop; i++) {
    it = p->bs->mValidList.begin() + i;
    score = simuBoard.fAlphaBeta(&(*it), player, -30000, 30000, 0,
                                 p->bs->mNumCheckMoves);

    if (simuBoard.mMoveList.size() != 0) simuBoard.fUndoMove();

    sem_wait(&gBestAccess);
    // encontra a melhor jogada
    if (score >= gNodeBest) {
      gNodeBest = score;
      bestMove = it;
      gIndex = i;
    }
    sem_post(&gBestAccess);
  }

  simuBoard.fCleanup();
}

// wikipedia Negamax, https://en.wikipedia.org/wiki/Negamax
int cBoardState::fAlphaBeta(gameMove *m, int player, int alpha, int beta,
                            int depth, int &numMoves) {
  int i, bestVal, score;
  std::vector<gameMove>::iterator it;
  hashState *state;

  int maxDepth = 8;
  int originalAlpha = alpha;
  sem_wait(&gMoveNumAccess);
  numMoves++;  // total de movimentos
  sem_post(&gMoveNumAccess);

  unsigned long long boardHash = mHash->fBoardToHash(mBoard);
  state = mHash->fFindState(boardHash);

  if (state != nullptr) {
    int weightScore = (/*player * */ state->score);
    if (state->ply >= depth) {
      if (state->type == EXACT)
        return weightScore;

      else if (state->type == BETA)
        alpha = alpha > weightScore ? alpha : weightScore;

      else if (state->type == ALPHA)
        beta = beta > weightScore ? weightScore : beta;

      if (alpha >= beta) return weightScore;
    }
  }

  if (depth == maxDepth || mState > 0)
    return (/*jogador * */ (mBScore - mWScore));

  fMove(m);
  fComputeValidMoves();
  fIsInCheck();
  fRemoveChecks();

  cBoardState simuBoard = *this;

  bestVal = -30000;

  for (it = mValidList.begin(); it != mValidList.end(); it++) {
    simuBoard.fMove(&(*it));
    it->score = (player * (mBScore - mWScore));
    simuBoard.fUndoMove();
  }

  std::sort(mValidList.begin(), mValidList.end(), moveCompare);

  // começa a procura
  for (it = mValidList.begin(); it != mValidList.end(); it++) {
    score = -simuBoard.fAlphaBeta(&(*it), -player, -beta, -alpha, depth + 1,
                                  numMoves);

    if (depth < maxDepth - 1 && simuBoard.mMoveList.size() != 0) {
      simuBoard.fUndoMove();
    }

    if (score > bestVal)  // verifica se encontrou um score melhor
      bestVal = score;

    if (bestVal > alpha)  // verifica se o score é melhor que o alpha
      alpha = bestVal;

    if (alpha >= beta)  // verifica a ramificação deve ser podada
      break;
  }

  hashState newState;

  newState.score = bestVal;

  if (bestVal <= originalAlpha)
    newState.type = ALPHA;
  else if (bestVal >= originalAlpha)
    newState.type = BETA;
  else
    newState.type = EXACT;

  newState.ply = depth;
  newState.hash = boardHash;

  mHash->fAddState(newState);

  return alpha;
}

mpiBoardState *cBoardState::fToStruct() {
  int i, j;
  mpiBoardState *m = new mpiBoardState;

  // mapeia o board atual para o customtype MPI
  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)  // casas são mapeadas usando a formula = (8*i)+j
      m->mBoard[((i << 3) + j)] = mBoard[i][j];

  m->mEnPassant[0] = mEnPassant[0];
  m->mEnPassant[1] = mEnPassant[1];
  m->mTurn = mTurn;
  m->mState = mState;
  m->mWScore = mWScore;
  m->mBScore = mBScore;
  m->mWLostCastle = mWLostCastle;
  m->mBLostCastle = mBLostCastle;
  m->mWCheck = mWCheck ? 1 : 0;
  m->mBCheck = mBCheck ? 1 : 0;
  m->mEndGame = mEndGame ? 1 : 0;

  return m;
}

void cBoardState::fMPISendBoardState() {
  mpiBoardState *m = fToStruct();

  // sincroniza os processos
  MPI_Barrier(MPI_COMM_WORLD);

  // envia o boardstate por broadcast
  MPI_Bcast(m, 1, customType, 0, MPI_COMM_WORLD);

  // resinconiza os processos
  MPI_Barrier(MPI_COMM_WORLD);

  delete m;
}

void cBoardState::fMPIGetBoardState() {
  int i, j;
  mpiBoardState *m = new mpiBoardState;

  // sincroniza processos para receberem o boardstate
  MPI_Barrier(MPI_COMM_WORLD);

  // realiza o broadcast do boardstate
  MPI_Bcast(m, 1, customType, 0, MPI_COMM_WORLD);

  // espera que todos processos recebam o boardstate
  MPI_Barrier(MPI_COMM_WORLD);

  cBoardState *bs = new cBoardState(m);

  *this = *bs;  // atualiza o boardstate

  fComputeValidMoves();
  fIsInCheck();
  fRemoveChecks();

  delete m;
  delete bs;
}

gameMove *cBoardState::fMPIGetBestMove() {
  int bestScore[2] = {-99999, 0};  // garante que um movimento será escolhido
  int index, globalBest[2], dummy = 0;
  std::vector<gameMove>::iterator it;

  gameMove *m = new gameMove;

  // sincronização antes do reduce
  MPI_Barrier(MPI_COMM_WORLD);

  // distribui o score maximo encontrado
  MPI_Allreduce(&bestScore, &globalBest, 1, MPI_2INT, MPI_MAXLOC,
                MPI_COMM_WORLD);

  // aguarda todos receberem o score
  MPI_Barrier(MPI_COMM_WORLD);

  // soma os movimento checados pelos processos
  MPI_Reduce(&dummy, &mNumCheckMoves, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

  // aguarda o processo de rank 0 receber o dado
  MPI_Barrier(MPI_COMM_WORLD);

  // recebe o indice do melhor movimento
  MPI_Recv(&index, 1, MPI_INT, globalBest[1], 0, MPI_COMM_WORLD,
           MPI_STATUS_IGNORE);

  // resincroniza os processo para a proxima iteração do loop
  MPI_Barrier(MPI_COMM_WORLD);

  it = mValidList.begin() + index;

  m->fromI = it->fromI;
  m->fromJ = it->fromJ;
  m->toI = it->toI;
  m->toJ = it->toJ;
  m->piece = it->piece;
  m->capture = it->capture;

  return m;
}

int cBoardState::fGetState() { return mState; }

int cBoardState::fGetTurn() { return mTurn; }

int cBoardState::fGetCheckMoves() { return mNumCheckMoves; }

int cBoardState::fGetListCount(int list) {
  if (list == VALIDLIST)
    return mValidList.size();
  else
    return mMoveList.size();
}
