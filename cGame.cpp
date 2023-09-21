#include "cGame.h"
#include <stddef.h>

// make sure only one cGame class can be created
cGame *cGame::mGame = NULL;

cGame::cGame() {}

cGame *cGame::fGetGame()
{
    return mGame ? mGame : (mGame = new cGame());
}

void cGame::fEndGame()
{
    delete mGame;
    mGame = 0;
}

void cGame::fRun(int mode)
{
    bs = new cBoardState();

    sleep(2);
    if (rank == 0)
        system("clear");

    // entering different game loops
    if (mode == PVP && rank == 0)
    {
        bs->fComputeValidMoves();
        fGameLoopPVP();
    }
    else if (mode == PVA)
    {
        if (rank == 0)
        {
            bs->fComputeValidMoves();
            fGameLoopPVA();
        }

        else
            bs->fAiCalculateMove();
    }
    else if (mode == AVA)
    {
        if (rank == 0)
        {
            bs->fComputeValidMoves();
            fGameLoopAVA();
        }

        else
            bs->fAiCalculateMove();
    }
    bs->fCleanup();
    delete bs; // heh
}

void cGame::fGameLoopPVP()
{
    gameMove m;

    while (bs->fGetState() == PLAYING)
    {
        bs->fPrintBoard();

        if (bs->fGetTurn() % 2 == 0)
            std::cout << "White: ";
        else
            std::cout << "Black: ";

        if (fGetCmd(&m))
        {
            bs->fProcessMove(&m); // check if valid coordinates were entered
        }
        system("clear");
    }

    bs->fPrintBoard();
    switch (bs->fGetState())
    {
    case DRAW:
        std::cout << "Draw!" << std::endl;
        break;

    case WHITEWON:
        std::cout << "White Won!" << std::endl;
        break;

    case BLACKWON:
        std::cout << "Black Won!" << std::endl;
        break;
    }
}

void cGame::fGameLoopPVA()
{
    gameMove *m;
    struct timespec t1, t2;

    while (bs->fGetState() == PLAYING)
    {
        bs->fPrintBoard();

        if (bs->fGetTurn() % 2 == 0)
        {
            std::cout << "White: ";

            m = new gameMove;
            int check = fGetCmd(m);
            if (check == 1)
                bs->fProcessMove(m); // check if valid coordinates were entered

            else if (check == -1) // quit
            {
                delete m;
                bs->fCleanup();
                exit(0);
            }
        }
        else
        {
            delete m;

            bs->fMPISendBoardState();

            clock_gettime(CLOCK_MONOTONIC, &t1);
            m = bs->fMPIGetBestMove();
            clock_gettime(CLOCK_MONOTONIC, &t2);

            bs->fProcessMove(m);
            delete m;
        }

        system("clear");

        if (bs->fGetTurn() % 2 == 0)
        {
            double deltaT = (t2.tv_sec - t1.tv_sec) + (double)(t2.tv_nsec - t1.tv_nsec) / 1000000000L;
            std::cout << std::setprecision(3) << std::fixed << "Checked " << bs->fGetCheckMoves() << " moves in " << deltaT << " seconds" << std::endl;
        }
    }

    bs->fPrintBoard();
    switch (bs->fGetState())
    {
    case DRAW:
        std::cout << "Draw!" << std::endl;
        break;

    case WHITEWON:
        std::cout << "White Won!" << std::endl;
        break;

    case BLACKWON:
        std::cout << "Black Won!" << std::endl;
        break;
    }

    delete m;
}

void cGame::fGameLoopAVA()
{
    gameMove *m;
    struct timespec t1, t2;

    while (bs->fGetState() == PLAYING)
    {
        bs->fPrintBoard();

        bs->fMPISendBoardState();

        clock_gettime(CLOCK_MONOTONIC, &t1);
        m = bs->fMPIGetBestMove();
        clock_gettime(CLOCK_MONOTONIC, &t2);

        bs->fProcessMove(m);
        delete m;

        system("clear");

        double deltaT = (t2.tv_sec - t1.tv_sec) + (double)(t2.tv_nsec - t1.tv_nsec) / 1000000000L;

        if (bs->fGetTurn() % 2 == 1)
            std::cout << "White ";
        else
            std::cout << "Black ";

        std::cout << std::setprecision(3) << std::fixed << "Checked " << bs->fGetCheckMoves() << " moves in " << deltaT << " seconds" << std::endl;
    }

    bs->fPrintBoard();
    switch (bs->fGetState())
    {
    case DRAW:
        std::cout << "Draw!" << std::endl;
        break;

    case WHITEWON:
        std::cout << "White Won!" << std::endl;
        break;

    case BLACKWON:
        std::cout << "Black Won!" << std::endl;
        break;
    }

    delete m;
}

int cGame::fGetCmd(gameMove *m)
{ 
    int i;
    std::string coords;
                            
    std::cin >> coords;

    if (coords.length() != 4)
        return 0;

    for (i = 0; i < 4; i++)
        coords[i] = tolower(coords[i]);

    if (strcmp("quit", coords.c_str()) == 0)
    {
        return -1;
    }

    // convert to number coordinates
    if (fCheckCoords(coords))
    {
        m->fromJ = coords[0] - 'a';
        m->fromI = '8' - coords[1];
        m->toJ = coords[2] - 'a';
        m->toI = '8' - coords[3];
        return 1;
    }

    return 0;
}

int cGame::fCheckCoords(std::string coords)
{
    int i;
    for (i = 0; i < 4; i++)
    {
        if (i % 2 == 0)
        {
            if (!isalpha(coords[i])) // indices 0 and 2 must be letters a-h
                return 0;

            else if (coords[i] > 'h') // alpha coords are out of bounds
                return 0;
        }

        else
        {
            if (!isdigit(coords[i])) // indices 1 and 3 must be digits 1-8
                return 0;

            else if (coords[i] > '8')
                return 0;
        }
    }

    return 1;
}
