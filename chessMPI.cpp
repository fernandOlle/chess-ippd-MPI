#include <string.h>

#include <cstddef>

#include "cGame.h"
#include "pieceSquareTables.h"

int worldSize, rank;
MPI_Datatype customType;

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  // tipo espeficio para o MPI
  // cBoardState.h
  int blockLength = 75;
  MPI_Aint displacement = 0;
  MPI_Datatype type = MPI_INT;

  MPI_Type_create_struct(1, &blockLength, &displacement, &type, &customType);
  MPI_Type_commit(&customType);

  if (argc == 1) {
    if (rank == 0) std::cout << "Entrando modo Player vs Player" << std::endl;
    cGame::fGetGame()->fRun(PVP);
  } else if (argc == 2) {
    if (strcmp(argv[1], "pva") == 0) {
      if (rank == 0) std::cout << "Entrando modo Player vs AI" << std::endl;
      cGame::fGetGame()->fRun(PVA);
    } else if (strcmp(argv[1], "ava") == 0) {
      if (rank == 0) std::cout << "Entrando modo AI vs AI\n" << std::endl;
      cGame::fGetGame()->fRun(AVA);
    } else if (rank == 0)
      std::cout << "uso: " << argv[0]
                << "<argumento opicional>\n <argumento opicional> = pva , "
                   "ava.\n pva = "
                   "Player vs AI. ava = AI vs AI\n"
                << std::endl;
  } else if (rank == 0)
    std::cout
        << "usage: " << argv[0]
        << "<argumento opicional>\n <argumento opicional> = pva , ava.\n pva = "
           "Player vs AI. ava = AI vs AI\n"
        << std::endl;

  cGame::fEndGame();

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
  return 0;
}
