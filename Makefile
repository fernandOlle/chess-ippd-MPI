chessMPI: cGame.cpp cBoardState.cpp chessMPI.cpp cHash.cpp
	mpic++ -g -o chessMPI *.cpp 

pvp:
	mpirun -np 4 chessMPI 

pva:
	mpirun -np 4 chessMPI pva

ava:
	mpirun -np 4 chessMPI ava

