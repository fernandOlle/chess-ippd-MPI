#include "cBoardState.h"

int gNodeBest;     // best move score for the node
int gIndex;        // index for the best move
sem_t gBestAccess; // semaphore to restrict access to above variables
sem_t gMoveNumAccess;

cHash *cBoardState::mHash;

bool moveCompare(gameMove m, gameMove m2) { return (m.score < m2.score); }

// constructor
cBoardState::cBoardState()
{
    int board[8][8] = {{B_ROOK, B_KNIGHT, B_BISHOP, B_QUEEN, B_KING, B_BISHOP, B_KNIGHT, B_ROOK},
                       {B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN},
                       {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                       {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                       {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                       {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                       {W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN},
                       {W_ROOK, W_KNIGHT, W_BISHOP, W_QUEEN, W_KING, W_BISHOP, W_KNIGHT, W_ROOK}};

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

    if (sem_init(&gBestAccess, 0, 1) == -1 || sem_init(&gMoveNumAccess, 0, 1) == -1)
    {
        std::cout << "Rank " << rank << ": Could not create semaphore" << std::endl;
        exit(-1);
    }
}

// copy constructor
cBoardState::cBoardState(const cBoardState &b2)
{
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
    //  mValidList = b2.mValidList;
    //  mMoveList = b2.mMoveList;
}

// construct from struct
cBoardState::cBoardState(mpiBoardState *m)
{
    int i, j;

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            mBoard[i][j] = m->mBoard[((i << 3) + j)];

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

void cBoardState::fComputeValidMoves()
{
    int i, j, piece, val;

    mWScore = 0;
    mBScore = 0;

    mValidList.clear();

    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 8; j++)
        {
            piece = abs(mBoard[i][j]); // absolute value
            switch (piece)             // https://en.wikipedia.org/wiki/Chess_piece_relative_value
            {
            case B_KING:
                val = 20000;
                val += mBoard[i][j] > 0 ? mEndGame ? kingEndGame[63 - ((i << 3) + j)] : kingSquare[63 - ((i << 3) + j)]
                       : mEndGame ? kingEndGame[((i << 3) + j)]
                                        : kingSquare[((i << 3) + j)];
                break;

            case B_QUEEN:
                val = 900;
                val += mBoard[i][j] > 0 ? queenSquare[63 - ((i << 3) + j)] : queenSquare[((i << 3) + j)];
                break;

            case B_ROOK:
                val = 500;
                val += mBoard[i][j] > 0 ? rookSquare[63 - ((i << 3) + j)] : rookSquare[((i << 3) + j)];
                break;

            case B_BISHOP:
                val = 330;
                val += mBoard[i][j] > 0 ? bishopSquare[63 - ((i << 3) + j)] : bishopSquare[((i << 3) + j)];
                break;

            case B_KNIGHT:
                val = 320;
                val += mBoard[i][j] > 0 ? knightSquare[63 - ((i << 3) + j)] : knightSquare[((i << 3) + j)];
                break;

            case B_PAWN:
                val = 100;
                val += mBoard[i][j] > 0 ? pawnSquare[63 - ((i << 3) + j)] : pawnSquare[((i << 3) + j)];
                break;

            case EMPTY: // fall through

            default:
                val = 0;
            }

            if (mBoard[i][j] > 0) // black piece
                mBScore += val;
            else // white piece
                mWScore += val;

            // add piece moves for the corresponding player
            if ((mTurn % 2 == 0 && mBoard[i][j] < EMPTY) || (mTurn % 2 == 1 && mBoard[i][j] > EMPTY))
                fAddPieceMoves(i, j);
        }
    }

    if (mWScore <= 21200 || mBScore <= 21200)
        mEndGame = true;
}

void cBoardState::fAddPieceMoves(int i, int j)
{
    int piece = abs(mBoard[i][j]);

    switch (piece)
    {
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

void cBoardState::fAddPawnMoves(int i, int j)
{
    if (mTurn % 2 == 0) // white
    {
        // move forward 1
        if (mBoard[i - 1][j] == EMPTY && fCanMove(i - 1, j))
            fAddMove(i, j, i - 1, j, VALIDLIST);

        // move forward 2 for pawns that haven't moved
        if (i == 6 && mBoard[i - 1][j] == EMPTY && mBoard[i - 2][j] == EMPTY)
            fAddMove(i, j, i - 2, j, VALIDLIST);

        // attack forward-right
        if (mBoard[i - 1][j + 1] > EMPTY && fCanMove(i - 1, j + 1))
            fAddMove(i, j, i - 1, j + 1, VALIDLIST);

        // attack forward-left
        if (mBoard[i - 1][j - 1] > EMPTY && fCanMove(i - 1, j - 1))
            fAddMove(i, j, i - 1, j - 1, VALIDLIST);

        // en passant right
        if (mBoard[i - 1][j + 1] == EMPTY && fCanMove(i - 1, j + 1) && mEnPassant[0] == i && mEnPassant[1] == j + 1)
            fAddMove(i, j, i - 1, j + 1, VALIDLIST);

        // en passant left
        if (mBoard[i - 1][j - 1] == EMPTY && fCanMove(i - 1, j - 1) && mEnPassant[0] == i && mEnPassant[1] == j - 1)
            fAddMove(i, j, i - 1, j - 1, VALIDLIST);
    }

    if (mTurn % 2 == 1) // black
    {
        // move forward 1
        if (mBoard[i + 1][j] == EMPTY && fCanMove(i + 1, j))
            fAddMove(i, j, i + 1, j, VALIDLIST);

        // move forward 2 for pawns that haven't moved
        if (i == 1 && mBoard[i + 1][j] == EMPTY && mBoard[i + 2][j] == EMPTY)
            fAddMove(i, j, i + 2, j, VALIDLIST);

        // attack forward-right
        if (mBoard[i + 1][j + 1] < EMPTY && fCanMove(i + 1, j + 1))
            fAddMove(i, j, i + 1, j + 1, VALIDLIST);

        // attack forward-left
        if (mBoard[i + 1][j - 1] < EMPTY && fCanMove(i + 1, j - 1))
            fAddMove(i, j, i + 1, j - 1, VALIDLIST);

        // en passant right
        if (mBoard[i + 1][j + 1] == EMPTY && fCanMove(i + 1, j + 1) && mEnPassant[0] == i && mEnPassant[1] == j + 1)
            fAddMove(i, j, i + 1, j + 1, VALIDLIST);

        // en passant left
        if (mBoard[i + 1][j - 1] == EMPTY && fCanMove(i + 1, j - 1) && mEnPassant[0] == i && mEnPassant[1] == j - 1)
            fAddMove(i, j, i + 1, j - 1, VALIDLIST);
    }
}

void cBoardState::fAddKnightMoves(int i, int j)
{
    // move down2-right1
    if (fCanMove(i + 2, j + 1))
        fAddMove(i, j, i + 2, j + 1, VALIDLIST);

    // move down2-left1
    if (fCanMove(i + 2, j - 1))
        fAddMove(i, j, i + 2, j - 1, VALIDLIST);

    // move left2-down1
    if (fCanMove(i + 1, j - 2))
        fAddMove(i, j, i + 1, j - 2, VALIDLIST);

    // move left2-up1
    if (fCanMove(i - 1, j - 2))
        fAddMove(i, j, i - 1, j - 2, VALIDLIST);

    // move up2-left1
    if (fCanMove(i - 2, j - 1))
        fAddMove(i, j, i - 2, j - 1, VALIDLIST);

    // move up2-right1
    if (fCanMove(i - 2, j + 1))
        fAddMove(i, j, i - 2, j + 1, VALIDLIST);

    // move right2-up1
    if (fCanMove(i - 1, j + 2))
        fAddMove(i, j, i - 1, j + 2, VALIDLIST);

    // move right2-down1
    if (fCanMove(i + 1, j + 2))
        fAddMove(i, j, i + 1, j + 2, VALIDLIST);
}

void cBoardState::fAddHorVertMoves(int i, int j)
{
    int i2, j2;

    // move up
    i2 = i - 1;
    j2 = j;
    while (fCanMove(i2, j2))
    {
        fAddMove(i, j, i2, j2, VALIDLIST);
        if (mBoard[i2][j2] != EMPTY) // blocked by enemy;
            break;
        i2--;
    }

    // move right
    i2 = i;
    j2 = j + 1;
    while (fCanMove(i2, j2))
    {
        fAddMove(i, j, i2, j2, VALIDLIST);
        if (mBoard[i2][j2] != EMPTY) // blocked by enemy;
            break;
        j2++;
    }

    // move down
    i2 = i + 1;
    j2 = j;
    while (fCanMove(i2, j2))
    {
        fAddMove(i, j, i2, j2, VALIDLIST);
        if (mBoard[i2][j2] != EMPTY) // blocked by enemy;
            break;
        i2++;
    }

    // move left
    i2 = i;
    j2 = j - 1;
    while (fCanMove(i2, j2))
    {
        fAddMove(i, j, i2, j2, VALIDLIST);
        if (mBoard[i2][j2] != EMPTY) // blocked by enemy;
            break;
        j2--;
    }
}

void cBoardState::fAddDiagMoves(int i, int j)
{
    int i2, j2;

    // move up-right
    i2 = i - 1;
    j2 = j + 1;
    while (fCanMove(i2, j2))
    {
        fAddMove(i, j, i2, j2, VALIDLIST);
        if (mBoard[i2][j2] != EMPTY) // blocked by enemy;
            break;

        i2--;
        j2++;
    }

    // move down-right
    i2 = i + 1;
    j2 = j + 1;
    while (fCanMove(i2, j2))
    {
        fAddMove(i, j, i2, j2, VALIDLIST);
        if (mBoard[i2][j2] != EMPTY) // blocked by enemy;
            break;
        i2++;
        j2++;
    }

    // move down-left
    i2 = i + 1;
    j2 = j - 1;
    while (fCanMove(i2, j2))
    {
        fAddMove(i, j, i2, j2, VALIDLIST);
        if (mBoard[i2][j2] != EMPTY) // blocked by enemy;
            break;
        i2++;
        j2--;
    }

    // move up-left
    i2 = i - 1;
    j2 = j - 1;
    while (fCanMove(i2, j2))
    {
        fAddMove(i, j, i2, j2, VALIDLIST);
        if (mBoard[i2][j2] != EMPTY) // blocked by enemy;
            break;
        i2--;
        j2--;
    }
}

void cBoardState::fAddKingMoves(int i, int j)
{
    // move up
    if (fCanMove(i - 1, j))
        fAddMove(i, j, i - 1, j, VALIDLIST);

    // move up-right
    if (fCanMove(i - 1, j + 1))
        fAddMove(i, j, i - 1, j + 1, VALIDLIST);

    // move right
    if (fCanMove(i, j + 1))
        fAddMove(i, j, i, j + 1, VALIDLIST);

    // move down-right
    if (fCanMove(i + 1, j + 1))
        fAddMove(i, j, i + 1, j + 1, VALIDLIST);

    // move down
    if (fCanMove(i + 1, j))
        fAddMove(i, j, i + 1, j, VALIDLIST);

    // move down-left
    if (fCanMove(i + 1, j - 1))
        fAddMove(i, j, i + 1, j - 1, VALIDLIST);

    // move left
    if (fCanMove(i, j - 1))
        fAddMove(i, j, i, j - 1, VALIDLIST);

    // move up-left
    if (fCanMove(i - 1, j - 1))
        fAddMove(i, j, i - 1, j - 1, VALIDLIST);

    // castling
    if ((mTurn % 2 == 0 && mWLostCastle == -1) || (mTurn % 2 == 1 && mBLostCastle == -1))
    {
        // castle left
        if (mBoard[i][j - 1] == EMPTY && mBoard[i][j - 2] == EMPTY && mBoard[i][j - 3] == EMPTY && abs(mBoard[i][j - 4]) == B_ROOK)
            fAddMove(i, j, i, j - 2, VALIDLIST);

        // castle right
        if (mBoard[i][j + 1] == EMPTY && mBoard[i][j + 2] == EMPTY && abs(mBoard[i][j + 3]) == B_ROOK)
            fAddMove(i, j, i, j + 2, VALIDLIST);
    }
}

void cBoardState::fAddMove(int i, int j, int i2, int j2, int list)
{
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

void cBoardState::fPrintMoves(int list)
{
    std::vector<gameMove>::iterator it, stop;

    if (list == MOVELIST)
    {
        it = mMoveList.begin();
        stop = mMoveList.end();
    }
    else
    {
        it = mValidList.begin();
        stop = mValidList.end();
    }

    std::cout << "Rank " << rank << " Moves:" << std::endl;
    for (it; it != stop; it++)
    {
        std::cout << char(it->fromJ + 'a') << char('8' - it->fromI) << char(it->toJ + 'a') << char('8' - it->toI) << std::endl;
    }
}

std::string cBoardState::fPrintPiece(int piece)
{
    // unicode
    switch (piece)
    {
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

void cBoardState::fPrintBoard()
{
    int i, j;

    std::cout << "     a    b    c    d    e    f    g    h" << std::endl;
    for (i = 0; i < 8; i++)
    {
        std::cout << "   -----------------------------------------" << std::endl;
        std::cout << " " << (8 - i) << " ";

        for (j = 0; j < 8; j++)
            std::cout << "| " << fPrintPiece(mBoard[i][j]) << "  ";

        std::cout << "| " << (8 - i) << std::endl;
    }

    std::cout << "   -----------------------------------------" << std::endl;
    std::cout << "     a    b    c    d    e    f    g    h" << std::endl;
}

void cBoardState::fProcessMove(gameMove *m)
{
    if (fMoveIsValid(m))
    {
        fMove(m);     // move and update turn
        fIsInCheck(); // is the current player in check?
        fComputeValidMoves();
        fRemoveChecks();
    }
    else
    {
        std::cout << "Invalid Move!" << std::endl;
        sleep(2);
    }
}

void cBoardState::fMove(gameMove *m)
{
    int piece = mBoard[m->fromI][m->fromJ];
    int capture = mBoard[m->toI][m->toJ];

    fAddMove(m->fromI, m->fromJ, m->toI, m->toJ, MOVELIST); // keep track of all moves

    mBoard[m->fromI][m->fromJ] = EMPTY; // move piece
    mBoard[m->toI][m->toJ] = piece;

    // check for pawn promotion
    if (piece == W_PAWN && m->toI == 0)
        mBoard[m->toI][m->toJ] = W_QUEEN;

    if (piece == B_PAWN && m->toI == 7)
        mBoard[m->toI][m->toJ] = B_QUEEN;

    // if a pawn moved forward two spaces it is vulnerable to en passant capture
    if (abs(piece) == B_PAWN && abs(m->toI - m->fromI) == 2)
    {
        mEnPassant[0] = m->toI;
        mEnPassant[1] = m->toJ;
    }
    else // en passant capture can only be performed the turn immediately after the pawn moves forward two spaces
    {
        mEnPassant[0] = -1;
        mEnPassant[1] = -1;
    }

    // check for en passant captures
    if (abs(piece) == B_PAWN && m->fromJ != m->toJ && capture == EMPTY) // pawn moved diagonally to an empty square -> en passant capture
    {
        int enPCapture = mBoard[m->fromI][m->toJ];
        mBoard[m->fromI][m->toJ] = EMPTY;
    }

    // check for castling and move rook
    int len = m->toJ - m->fromJ; // how far did the piece move horizontally
    if (abs(piece) == B_KING && abs(len) == 2)
    {
        if (len > 0) // castled to the right
        {
            int castle = mBoard[m->fromI][m->toJ + 1];
            // fAddMove(m->fromI, m->toJ + 1, m->fromI, m->toJ - 1, MOVELIST);
            mBoard[m->fromI][m->toJ + 1] = EMPTY;
            mBoard[m->fromI][m->toJ - 1] = castle;
        }
        else // castled to the left
        {
            int castle = mBoard[m->fromI][m->toJ - 2];
            // fAddMove(m->fromI, m->toJ-2 , m->fromI, m->toJ + 1, MOVELIST);
            mBoard[m->fromI][m->toJ - 2] = EMPTY;
            mBoard[m->fromI][m->toJ + 1] = castle;
        }

        if (piece < EMPTY) // white
            mWLostCastle = mTurn;

        else if (piece > EMPTY) // black
            mBLostCastle = mTurn;
    }

    else if (abs(piece) == B_KING) // if king moved but not to castle, don't allow castling
    {
        if (piece < EMPTY && mWLostCastle == -1) // white
            mWLostCastle = mTurn;

        else if (piece > EMPTY && mBLostCastle == -1) // black
            mBLostCastle = mTurn;
    }

    else if (abs(piece) == B_ROOK) // if rook moved
    {
        if (piece < EMPTY && mWLostCastle == -1) // white and has not lost ability to castle
        {
            if (mBoard[7][0] != W_ROOK && mBoard[7][7] != W_ROOK) // if both rooks have moved, no castling
                mWLostCastle = mTurn;
        }

        else if (piece > EMPTY && mBLostCastle == -1) // black
        {
            if (mBoard[0][0] != B_ROOK && mBoard[0][7] != B_ROOK)
                mWLostCastle = mTurn;
        }
    }

    mTurn++; // next turn
}

void cBoardState::fIsInCheck()
{
    int i;
    gameMove m;
    cBoardState nextBoard = *this;

    nextBoard.mTurn++;

    // check the possible moves the opponent can make
    nextBoard.fComputeValidMoves();

    int loop = nextBoard.mValidList.size();
    for (i = 0; i < loop; i++)
    {
        // if they can capture my king then i am in check
        if (abs(nextBoard.mValidList[i].capture) == B_KING)
        {
            if (nextBoard.mTurn % 2 == 0)
                mBCheck = true;

            else if (nextBoard.mTurn % 2 == 1)
                mWCheck = true;

            break;
        }
        else
        {
            if (nextBoard.mTurn % 2 == 0)
                mBCheck = false;

            else if (nextBoard.mTurn % 2 == 1)
                mWCheck = false;
        }
    }

    nextBoard.mValidList.clear();
    nextBoard.mMoveList.clear();
}

void cBoardState::fRemoveChecks()
{
    int i, j;
    gameMove m;

    cBoardState nextBoard = *this;

    std::vector<gameMove>::iterator it;
    for (it = mValidList.begin(); it != mValidList.end(); it++) // loop through all valid moves
    {
        m.fromI = it->fromI;
        m.fromJ = it->fromJ;
        m.toI = it->toI;
        m.toJ = it->toJ;
        m.piece = it->piece;
        m.capture = it->capture;

        nextBoard.fMove(&m); // do the move on the board copy
        nextBoard.fComputeValidMoves();

        std::vector<gameMove>::iterator it2;
        for (it2 = nextBoard.mValidList.begin(); it2 != nextBoard.mValidList.end(); it2++) // loop through all the valid moves in the simulated board
        {
            if (abs(it2->capture) == B_KING) // if the king can be captured, the current move results in a check
            {
                mValidList.erase(it); // so we remove it from the list of valid moves
                it--;
                break; // stop looking in this scenario
            }
        }

        nextBoard.fUndoMove();
    }

    if (mValidList.size() == 0) // no legal moves
    {
        if (mTurn % 2 == 0 && mWCheck) // white turn and in check
            mState = BLACKWON;
        else if (mTurn % 2 == 1 && mBCheck) // black turn and in check
            mState = WHITEWON;

        // white turn and not in check or black turn and not in check
        else if ((mTurn % 2 == 0 && !mWCheck) || (mTurn % 2 == 1 && !mBCheck))
            mState = DRAW;
    }
    else if (mValidList.size() > 0)
    {
        if (mTurn % 2 == 0)
            mWCheck = false;
        else
            mBCheck = false;
    }

    nextBoard.mValidList.clear();
    nextBoard.mMoveList.clear();
}

void cBoardState::fUndoMove()
{
    mTurn--;
    mState = PLAYING;

    // check if move was an en passant capture, if so J values will not be the same and the capture will be empty
    if (abs(mMoveList.back().piece) == B_PAWN && mMoveList.back().fromJ != mMoveList.back().toJ && mMoveList.back().capture == EMPTY)
    {
        mBoard[mMoveList.back().fromI][mMoveList.back().toJ] = -(mMoveList.back().piece); // captured piece will always be the opposite colored pawn
        mEnPassant[0] = mMoveList.back().fromI;
        mEnPassant[1] = mMoveList.back().toJ;
    }
    else
    {
        mEnPassant[0] = -1;
        mEnPassant[1] = -1;
    }

    // check for castling and fix rook
    if (mMoveList.back().piece == B_KING)
    {
        if (mMoveList.back().toJ - mMoveList.back().fromJ == -2) // castled left
        {
            mBoard[0][0] = B_ROOK;
            mBoard[0][3] = EMPTY;
        }
        else if (mMoveList.back().toJ - mMoveList.back().fromJ == 2) // castled right
        {
            mBoard[0][7] = B_ROOK;
            mBoard[0][5] = EMPTY;
        }
    }
    else if (mMoveList.back().piece == W_KING)
    {
        if (mMoveList.back().toJ - mMoveList.back().fromJ == -2) // castled left
        {
            mBoard[7][0] = W_ROOK;
            mBoard[7][3] = EMPTY;
        }
        else if (mMoveList.back().toJ - mMoveList.back().fromJ == 2) // castled right
        {
            mBoard[7][7] = W_ROOK;
            mBoard[7][5] = EMPTY;
        }
    }

    // reallow castling
    if (mBLostCastle == mTurn)
        mBLostCastle = -1;

    if (mWLostCastle == mTurn)
        mWLostCastle = -1;

    // undo move
    mBoard[mMoveList.back().fromI][mMoveList.back().fromJ] = mMoveList.back().piece;
    mBoard[mMoveList.back().toI][mMoveList.back().toJ] = mMoveList.back().capture;

    mMoveList.pop_back(); // remove move from movelist
}

void cBoardState::fCleanup()
{
    mMoveList.clear();
    mValidList.clear();
}

bool cBoardState::fCanMove(int i, int j)
{
    if (i > 7 || j > 7 || i < 0 || j < 0) // out of bounds
    {
        return false;
    }

    int piece = mBoard[i][j];
    // trying to take your own piece or they're in the way
    //       black                              white
    if ((piece > EMPTY && mTurn % 2 == 1) || (piece < EMPTY && mTurn % 2 == 0))
    {
        return 0;
    }

    else if (piece == EMPTY) // can move to empty space
    {
        return true;
    }

    else // enemy piece
    {
        return true;
    }
}

bool cBoardState::fMoveIsValid(gameMove *m)
{
    m->piece = mBoard[m->fromI][m->fromJ];
    m->capture = mBoard[m->toI][m->toJ];

    if (m->piece == EMPTY) // player tries to move an empty square
        return false;

    if (mTurn % 2 == 0 && m->piece > EMPTY) // white player tries to move black piece
        return false;

    if (mTurn % 2 == 1 && m->piece < EMPTY) // black player tries to move white piece
        return false;

    return fCheckMoves(m);
}

bool cBoardState::fCheckMoves(gameMove *m)
{
    int i, loop = mValidList.size();
    for (i = 0; i < loop; i++)
    {
        if (m->fromJ == mValidList[i].fromJ && m->fromI == mValidList[i].fromI && m->toJ == mValidList[i].toJ && m->toI == mValidList[i].toI)
        //  && m->piece == curr->move.piece && m->capture == curr->move.capture) //dont really need this but its there if you want it
        {
            return true;
        }
    }
    return false;
}

void cBoardState::fAiCalculateMove()
{
    int i, score, index;
    int bestScore[2], globalBest[2], player;
    int testNum, remainder, offset, prevOffset;

    threadArgs *args = new threadArgs;
    bestScore[1] = rank; // MPI_MAXLOC will return the highest value and the rank that sent it

    do
    {
        fMPIGetBoardState();

        int numMoves = mValidList.size();

        mNumCheckMoves = 0;

        bestScore[0] = -30000; // Ai wants to maximize this score. Score is (ai's score - opponent's score)

        // split moves evenly
        int neededRanks;
        if (numMoves <= 10)
            neededRanks = numMoves;
        else
            neededRanks = 10;

        // only allows enough processes as needed, ie if theres 60 moves and 300 procs then only 60 will compute
        // rank 0 will never get here
        if (neededRanks >= rank)
        {
            int threadsNeeded, movesPerThread;
            // cBoardState simuBoard = *this;
            testNum = numMoves / neededRanks; // number ofmoves this rank will test
            testNum += numMoves % neededRanks >= rank ? 1 : 0;

            if (testNum < 31)
            {
                threadsNeeded = testNum;
                movesPerThread = 1;
                remainder = 0;
            }
            else
            {
                threadsNeeded = 31;
                movesPerThread = testNum / 31;
                remainder = testNum % 31;
            }

            pthread_t threads[threadsNeeded];

            // number of extra tests process with rank = rank - 1 did.
            prevOffset = 0;

            if (remainder > 0)
            {
                if (remainder >= rank)
                {
                    offset = 1;
                    prevOffset = rank == 1 ? 0 : 1; // rank 1 has no prevOffset
                }
            }

            args->bs = this;
            for (i = 1; i <= threadsNeeded; i++)
            {
                //        std::cout << "Rank " << rank << ": Creating thread " << i << std::endl;
                // check my math if you want :)
                args->start = (((i - 1) * movesPerThread) + prevOffset);
                args->stop = ((i * movesPerThread) + offset);

                int res = pthread_create(&threads[i - 1], NULL, fAlphaBetaDriver, args);

                if (res != 0)
                {
                    exit(-1);
                }
            }

            for (i = 0; i < threadsNeeded; i++)
            {
                pthread_join(threads[i], NULL);
                //      std::cout << "Rank " << rank << ": Joined thread " << i << std::endl;
            }
            globalBest[0] = gNodeBest;

            // synchronize before mpi reduce
            MPI_Barrier(MPI_COMM_WORLD);

            // distribute the global maximum score
            MPI_Allreduce(&bestScore, &globalBest, 1, MPI_2INT, MPI_MAXLOC, MPI_COMM_WORLD);

            // wait for everyone to get the data
            MPI_Barrier(MPI_COMM_WORLD);

            // sum all moves checked by the ai processes
            MPI_Reduce(&mNumCheckMoves, NULL, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

            // wait for rank 0 to get the data
            MPI_Barrier(MPI_COMM_WORLD);

            // if this process had the highest score send the index of the move
            if (globalBest[1] == rank)
            {
                MPI_Send(&gIndex, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
            }

            //      simuBoard.fCleanup();
        }

        else
        {
            // barriers to keep process that are not computing in sync with those that are
            MPI_Barrier(MPI_COMM_WORLD);

            // apparently MPI needs ALL processes to call this so thats why this is here, these will have -9999 as the score so its fine
            MPI_Allreduce(&bestScore, &globalBest, 1, MPI_2INT, MPI_MAXLOC, MPI_COMM_WORLD);

            MPI_Barrier(MPI_COMM_WORLD);

            // sum all moves checked by the ai processes
            MPI_Reduce(&mNumCheckMoves, NULL, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

            // wait for rank 0 to get the data
            MPI_Barrier(MPI_COMM_WORLD);
        }

        // wait for all processes to reach this point before going on to next iteration
        MPI_Barrier(MPI_COMM_WORLD);

    } while (mState == PLAYING); // idk if this actually ever exits
}

void *cBoardState::fAlphaBetaDriver(void *args)
{
    int score, i, player;
    std::vector<gameMove>::iterator it, bestMove;
    threadArgs *p = (threadArgs *)(args);

    cBoardState simuBoard = *(p->bs);

    if (p->bs->mTurn % 2 == 0)
        player = W_PAWN;
    else
        player = B_PAWN;

    for (i = p->start; i < p->stop; i++)
    {
        it = p->bs->mValidList.begin() + i;
        score = simuBoard.fAlphaBeta(&(*it), player, -30000, 30000, 0, p->bs->mNumCheckMoves);

        if (simuBoard.mMoveList.size() != 0)
            simuBoard.fUndoMove();

        //   std::cout << "Rank " << rank << " - Thread " << pthread_self() << ": Waiting to update best" << std::endl;
        sem_wait(&gBestAccess);
        if (score >= gNodeBest) // find a better move
        {
            gNodeBest = score;
            bestMove = it;
            gIndex = i;
        }
        sem_post(&gBestAccess);
        //    std::cout << "Rank " << rank << " - Thread " << pthread_self() << ": updated best" << std::endl;
    }

    simuBoard.fCleanup();
}

// using negaMax, https://en.wikipedia.org/wiki/Negamax
int cBoardState::fAlphaBeta(gameMove *m, int player, int alpha, int beta, int depth, int &numMoves)
{
    int i, bestVal, score;
    std::vector<gameMove>::iterator it;
    hashState *state;

    int maxDepth = 8;
    int originalAlpha = alpha;

    //     std::cout << "Rank " << rank << " - Thread " << pthread_self() << ": waiting to update moves" << std::endl;
    sem_wait(&gMoveNumAccess);
    numMoves++; // number of moves checked in total
    sem_post(&gMoveNumAccess);
    //   std::cout << "Rank " << rank << " - Thread " << pthread_self() << ": updated moves" << std::endl;

    unsigned long long boardHash = mHash->fBoardToHash(mBoard);
    state = mHash->fFindState(boardHash);

    if (state != nullptr)
    {
        int weightScore = (/*player * */ state->score);
        if (state->ply >= depth)
        {
            if (state->type == EXACT)
                return weightScore;

            else if (state->type == BETA)
                alpha = alpha > weightScore ? alpha : weightScore;

            else if (state->type == ALPHA)
                beta = beta > weightScore ? weightScore : beta;

            if (alpha >= beta)
                return weightScore;
        }
    }

    if (depth == maxDepth || mState > 0)
    {
        return (/*player * */ (mBScore - mWScore));
    }

    fMove(m);
    fComputeValidMoves();
    fIsInCheck();
    fRemoveChecks();

    cBoardState simuBoard = *this;

    bestVal = -30000; // player tries to increase this

    for (it = mValidList.begin(); it != mValidList.end(); it++)
    {
        simuBoard.fMove(&(*it));
        it->score = (player * (mBScore - mWScore));
        simuBoard.fUndoMove();
    }

    std::sort(mValidList.begin(), mValidList.end(), moveCompare); // sort on a depth level 1 basis.

    // start search
    for (it = mValidList.begin(); it != mValidList.end(); it++)
    {
        score = -simuBoard.fAlphaBeta(&(*it), -player, -beta, -alpha, depth + 1, numMoves);

        if (depth < maxDepth - 1 && simuBoard.mMoveList.size() != 0)
        {
            //      std::cout << "Rank " << rank << " - Thread " << pthread_self() << ": before undo depth = " << depth << std::endl;

            simuBoard.fUndoMove();
            //    std::cout << "Rank " << rank << " - Thread " << pthread_self() << ": after undo depth = " <<  depth << std::endl;
        }

        if (score > bestVal) // is this score better than the current best?
            bestVal = score;

        if (bestVal > alpha) // is the current best score better than the alpha?
            alpha = bestVal;

        if (alpha >= beta) // is the minimizing player's score better? if so stop looking
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

mpiBoardState *cBoardState::fToStruct()
{
    int i, j;
    mpiBoardState *m = new mpiBoardState;

    // copy current boardstate to a structure that can be handled by MPI
    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++) // i,j coordinates are mapped 0-63 using the formula (8*i)+j
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

void cBoardState::fMPISendBoardState()
{
    mpiBoardState *m = fToStruct();

    // synchronize processes so that they are ready to recv the boardstate
    MPI_Barrier(MPI_COMM_WORLD);

    // send boardstate
    MPI_Bcast(m, 1, customType, 0, MPI_COMM_WORLD);

    // wait until all processes get the boardstate
    MPI_Barrier(MPI_COMM_WORLD);

    delete m;
}

void cBoardState::fMPIGetBoardState()
{
    int i, j;
    mpiBoardState *m = new mpiBoardState;

    // synchronize processes so that they are ready to recv the boardstate
    MPI_Barrier(MPI_COMM_WORLD);

    // get boardstate
    MPI_Bcast(m, 1, customType, 0, MPI_COMM_WORLD);

    // wait until all processes get the boardstate
    MPI_Barrier(MPI_COMM_WORLD);

    cBoardState *bs = new cBoardState(m);

    *this = *bs; // lmao

    fComputeValidMoves();
    fIsInCheck();
    fRemoveChecks();

    delete m;
    delete bs;
}

gameMove *cBoardState::fMPIGetBestMove()
{
    int bestScore[2] = {-99999, 0}; // make sure this is never the max score
    int index, globalBest[2], dummy = 0;
    std::vector<gameMove>::iterator it;

    gameMove *m = new gameMove;

    // synchronize before mpi reduce
    MPI_Barrier(MPI_COMM_WORLD);

    // distribute the global maximum score
    MPI_Allreduce(&bestScore, &globalBest, 1, MPI_2INT, MPI_MAXLOC, MPI_COMM_WORLD);

    // wait for everyone to get the data
    MPI_Barrier(MPI_COMM_WORLD);

    // sum all moves checked by the ai processes
    MPI_Reduce(&dummy, &mNumCheckMoves, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    // wait for rank 0 to get the data
    MPI_Barrier(MPI_COMM_WORLD);

    // get the index of of the best move from the corresponding process
    MPI_Recv(&index, 1, MPI_INT, globalBest[1], 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // synchronize again before next iteration
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

int cBoardState::fGetListCount(int list)
{
    if (list == VALIDLIST)
        return mValidList.size();
    else
        return mMoveList.size();
}
