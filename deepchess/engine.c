#include <Python.h>
#define LEN(x) (sizeof(x)/sizeof(x[0]))
#define OPPONENT(x) (x==WHITE_IDX?BLACK_IDX:WHITE_IDX)
#define SCORING(x) (x==WHITE_IDX?1:-1)
#define KING_LINE(x) (x==WHITE_IDX?0:BOARD_SIZE - 1)
#define PAWN_DIRECTION(x) (x==WHITE_IDX?1:-1)
#define IS_VALID_IDX(x) ((x>=0 && x<BOARD_SIZE)?1:0)
#define IS_NOT_VALID_IDX(x) ((x>=0 && x<BOARD_SIZE)?0:1)
#define CASTLE_MOVES(p, s) (p==WHITE_IDX?(s==LEFT_CASTLE?CASTLE_WHITE_LEFT:CASTLE_WHITE_RIGHT):(s==LEFT_CASTLE?CASTLE_BLACK_LEFT:CASTLE_BLACK_RIGHT))

const int BOARD_SIZE = 8;
const int WHITE_IDX = 0;
const int BLACK_IDX = 1;

const int EMPTY = 0;
const int PAWN = 1;
const int BISHOP = 2;
const int KNIGHT = 3;
const int ROOK = 4;
const int QUEEN = 5;
const int KING = 6;
const int PIECES[] = {PAWN, BISHOP, KNIGHT, ROOK, QUEEN, KING};
const int PROMOTIONS[] = {QUEEN, ROOK, KNIGHT, BISHOP};
const int TOP_LINE[] = {ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK};

const int MOVE_DIRECTIONS_BISHOP[][2] = {{1, 1}, {1, -1}, {-1, -1}, {-1, 1}};
const int MOVE_DIRECTIONS_ROOK[][2] = {{1, 0}, {-1, 0}, {0, -1}, {0, 1}};
const int MOVE_DIRECTIONS_QUEEN[][2] = {{1, 0}, {-1, 0}, {0, -1}, {0, 1}, {1, 1}, {1, -1}, {-1, -1}, {-1, 1}};

const int KNIGHT_MOVES[][2] = {{1, 2}, {2, 1}, {-1, 2}, {2, -1}, {1, -2}, {-2, 1}, {-1, -2}, {-2, -1}};
const int KING_MOVES[][2] = {{1, 0}, {-1, 0}, {0, -1}, {0, 1}, {1, 1}, {1, -1}, {-1, -1}, {-1, 1}};

const int PAWN_CAPTURE_MOVES[][2] = {{1, 1}, {1, -1}};
const int PAWN_NORMAL_MOVE[2] = {1, 0};
const int PAWN_FIRST_MOVE[2] = {2, 0};
const int LEFT_CASTLE = 0;
const int RIGHT_CASTLE = 1;

const int CASTLE_WHITE_LEFT[][2] = {{0, BOARD_SIZE/2}, {0, BOARD_SIZE/2 - 1}, {0, BOARD_SIZE/2 - 2}};
const int CASTLE_WHITE_RIGHT[][2] = {{0, BOARD_SIZE/2}, {0, BOARD_SIZE/2 + 1}, {0, BOARD_SIZE/2 + 2}};

const int CASTLE_BLACK_LEFT[][2] = {{BOARD_SIZE - 1, BOARD_SIZE/2}, {BOARD_SIZE - 1, BOARD_SIZE/2 - 1}, {BOARD_SIZE - 1, BOARD_SIZE/2 - 2}};
const int CASTLE_BLACK_RIGHT[][2] = {{BOARD_SIZE - 1, BOARD_SIZE/2}, {BOARD_SIZE - 1, BOARD_SIZE/2 + 1}, {BOARD_SIZE - 1, BOARD_SIZE/2 + 2}};

const int MAX_PIECE_MOVES = 4*(BOARD_SIZE-1);
const int MAX_MOVES = MAX_PIECE_MOVES*2*BOARD_SIZE;
const int STATE_SIZE = 2*BOARD_SIZE*BOARD_SIZE + 4 + 2 + 1;


PyObject* stateToPy(int* state){
	PyObject* pyState, *board, *boardPlayer, *boardRow, *castling, *castlingPlayer, *enPassant, *player;
	int i, j, k;

	board = PyList_New();
	for(i=0; i<2; i++){
		boardPlayer = PyList_New();
		for(j=0; j<BOARD_SIZE; j++){
			boardRow = PyList_New();
			for(k=0; k<BOARD_SIZE; k++){
				PyList_Append(boardRow, PyInt_FromInt(getBoardBox(state, i, j, k)));
			}
			PyList_Append(boardPlayer, boardRow);
		}
		PyList_Append(board, boardPlayer);
	}

	enPassant = PyDict_New();
	PyDict_SetItem(enPassant, PyInt_FromInt(WHITE_IDX), PyInt_FromInt(getEnPassant(state, WHITE_IDX)));
	PyDict_SetItem(enPassant, PyInt_FromInt(BLACK_IDX), PyInt_FromInt(getEnPassant(state, BLACK_IDX)));

	castling = PyDict_New();
	castlingPlayer = PyDict_New();
	PyDict_SetItem(castlingPlayer, PyInt_FromInt(LEFT_CASTLE), PyInt_FromInt(getCastling(state, WHITE_IDX, LEFT_CASTLE)));
	PyDict_SetItem(castlingPlayer, PyInt_FromInt(RIGHT_CASTLE), PyInt_FromInt(getCastling(state, WHITE_IDX, RIGHT_CASTLE)));
	PyDict_SetItem(castling, PyInt_FromInt(WHITE_IDX), castlingPlayer);
	castlingPlayer = PyDict_New();
	PyDict_SetItem(castlingPlayer, PyInt_FromInt(LEFT_CASTLE), PyInt_FromInt(getCastling(state, BLACK_IDX, LEFT_CASTLE)));
	PyDict_SetItem(castlingPlayer, PyInt_FromInt(RIGHT_CASTLE), PyInt_FromInt(getCastling(state, BLACK_IDX, RIGHT_CASTLE)));
	PyDict_SetItem(castling, PyInt_FromInt(BLACK_IDX), castlingPlayer);
	
	
	pyState = PyDict_New();
	PyDict_SetItemString(pyState, "BOARD", board);
	PyDict_SetItemString(pyState, "CASTLING_AVAILABLE", castling);
	PyDict_SetItemString(pyState, "EN_PASSANT", enPassant);
	PyDict_SetItemString(pyState, "PLAYER", PyInt_FromInt(getPlayer(state)));
	return pyState;
}

void stateFromPy(PyObject* pyState, int* state){
	PyObject* *board, *castling, *enPassant;
	int i, j, k;

	board = PyDict_GetItemString(pyState, "BOARD");
	castling = PyDict_GetItemString(pyState, "CASTLING_AVAILABLE");
	enPassant = PyDict_GetItemString(pyState, "EN_PASSANT");
	setPlayer(state, PyInt_AsLong(PyDict_GetItemString(pyState, "PLAYER")));

	for(i=0; i<2; i++){
		for(j=0; j<BOARD_SIZE; j++){
			for(k=0; k<BOARD_SIZE; k++){
				setBoardBox(state, PyInt_AsLong(PyList_GetItem(PyList_GetItem(PyList_GetItem(board, PyInt_FromInt(i)), PyInt_FromInt(j)), PyInt_FromInt(k))), i, j, k);
			}
		}
	}

	setEnPassant(state, PyInt_AsLong(PyDict_GetItem(enPassant, PyInt_FromInt(WHITE_IDX))), WHITE_IDX);
	setEnPassant(state, PyInt_AsLong(PyDict_GetItem(enPassant, PyInt_FromInt(BLACK_IDX))), BLACK_IDX);

	setCastling(state, PyInt_AsLong(PyDict_GetItem(PyDict_GetItem(castling, PyInt_FromInt(WHITE_IDX)), PyInt_FromInt(LEFT_CASTLE))), WHITE_IDX, LEFT_CASTLE);
	setCastling(state, PyInt_AsLong(PyDict_GetItem(PyDict_GetItem(castling, PyInt_FromInt(WHITE_IDX)), PyInt_FromInt(RIGHT_CASTLE))), WHITE_IDX, RIGHT_CASTLE);
	setCastling(state, PyInt_AsLong(PyDict_GetItem(PyDict_GetItem(castling, PyInt_FromInt(BLACK_IDX)), PyInt_FromInt(LEFT_CASTLE))), BLACK_IDX, LEFT_CASTLE);
	setCastling(state, PyInt_AsLong(PyDict_GetItem(PyDict_GetItem(castling, PyInt_FromInt(BLACK_IDX)), PyInt_FromInt(RIGHT_CASTLE))), BLACK_IDX, RIGHT_CASTLE);
}

PyObject* actionsToPy(int* positions, int single=0){
	PyObject* pyActions, move, oldPos, newPos;
	int i, tot;

	tot = 0;
	pyActions = PyTuple_New(MAX_MOVES);
	for(i=0; positions[i]!=-1 && (i<single || !single);){
		move = PyTuple_New(2);

		oldPos = PyTuple_New(2);
		PyTuple_SetItem(oldPos, 0, positions[i++]);
		PyTuple_SetItem(oldPos, 1, positions[i++]);
		PyTuple_SetItem(move, 0, oldPos);

		if(positions[i+2]>0){
			newPos = PyTuple_New(3);
			PyTuple_SetItem(newPos, 0, positions[i++]);
			PyTuple_SetItem(newPos, 1, positions[i++]);
			PyTuple_SetItem(newPos, 2, positions[i++]);
		}else{
			newPos = PyTuple_New(2);
			PyTuple_SetItem(newPos, 0, positions[i++]);
			PyTuple_SetItem(newPos, 1, positions[i++]);
			i++;
		}
		PyTuple_SetItem(move, 1, newPos);
		PyTuple_SetItem(pyActions, tot++, move);
	}
	_PyTuple_Resize(pyActions, tot);

	return pyActions;
}

void actionFromPy(PyObject* pyAction, int* action){
	action[0] = PyTuple_GetItem(PyTuple_GetItem(pyAction, PyInt_FromInt(0)), PyInt_FromInt(0));
	action[1] = PyTuple_GetItem(PyTuple_GetItem(pyAction, PyInt_FromInt(0)), PyInt_FromInt(1));
	action[2] = PyTuple_GetItem(PyTuple_GetItem(pyAction, PyInt_FromInt(1)), PyInt_FromInt(0));
	action[3] = PyTuple_GetItem(PyTuple_GetItem(pyAction, PyInt_FromInt(1)), PyInt_FromInt(1));
	action[4] = PyTuple_GetItem(PyTuple_GetItem(pyAction, PyInt_FromInt(1)), PyInt_FromInt(2));
	if(PyInt_FromInt(PyTuple_Size(PyTuple_GetItem(pyAction, PyInt_FromInt(1)))>2){
		action[4] = PyTuple_GetItem(PyTuple_GetItem(pyAction, PyInt_FromInt(1)), PyInt_FromInt(2));
	}
	else{
		action[4] = -1;
	}
}



int getBoardBox(int* state, int player, int row, int col){
	int idx = player*BOARD_SIZE*BOARD_SIZE + row*BOARD_SIZE + col;
	return state[idx];
}

void setBoardBox(int* state, int value, int player, int row, int col){
	int idx = player*BOARD_SIZE*BOARD_SIZE + row*BOARD_SIZE + col;
	state[idx] = value;
}

int getEnPassant(int* state, int player){
	const int offset = BOARD_SIZE*BOARD_SIZE*2;
	return state[offset + player];
}

void setEnPassant(int* state, int value, int player){
	const int offset = BOARD_SIZE*BOARD_SIZE*2;
	state[offset + player] = value;
}

int getCastling(int* state, int player, int side){
	const int offset = BOARD_SIZE*BOARD_SIZE*2 + 2;
	return state[offset + player*2 + side];
}

void setCastling(int* state, int value, int player, int side){
	const int offset = BOARD_SIZE*BOARD_SIZE*2 + 2;
	state[offset + player*2 + side] = value;
}

int getPlayer(int* state){
	const int offset = BOARD_SIZE*BOARD_SIZE*2 + 2 + 4;
	return state[offset];
}

void setPlayer(int* state, int value){
	const int offset = BOARD_SIZE*BOARD_SIZE*2 + 2 + 4;
	state[offset] = value;
}



void initializeBoard(int* state){
	int i;
	for(i=0; i<BOARD_SIZE; i++){
		setBoardBox(state, TOP_LINE[i], WHITE_IDX, KING_LINE(WHITE_IDX) + PAWN_DIRECTION(WHITE_IDX), i);
		setBoardBox(state, TOP_LINE[i], BLACK_IDX, KING_LINE(BLACK_IDX) + PAWN_DIRECTION(BLACK_IDX), i);
		setBoardBox(state, PAWN, WHITE_IDX, KING_LINE(WHITE_IDX) + PAWN_DIRECTION(WHITE_IDX), i);
		setBoardBox(state, PAWN, BLACK_IDX, KING_LINE(BLACK_IDX) + PAWN_DIRECTION(BLACK_IDX), i);
	}
}

void initializeCastling(int* state){
	if(getBoardBox(state, WHITE_IDX, KING_LINE(WHITE_IDX), BOARD_SIZE/2) == KING){
		if(getBoardBox(state, WHITE_IDX, KING_LINE(WHITE_IDX), 0) == ROOK){
			setCastling(state, 1, WHITE_IDX, LEFT_CASTLE);
		}
		if(getBoardBox(state, WHITE_IDX, KING_LINE(WHITE_IDX), BOARD_SIZE - 1) == ROOK){
			setCastling(state, 1, WHITE_IDX, RIGHT_CASTLE);
		}
	}
	if(getBoardBox(state, BLACK_IDX, KING_LINE(BLACK_IDX), BOARD_SIZE/2) == KING){
		if(getBoardBox(state, BLACK_IDX, KING_LINE(BLACK_IDX), 0) == ROOK){
			setCastling(state, 1, BLACK_IDX, LEFT_CASTLE);
		}
		if(getBoardBox(state, BLACK_IDX, KING_LINE(BLACK_IDX), BOARD_SIZE - 1) == ROOK){
			setCastling(state, 1, BLACK_IDX, RIGHT_CASTLE);
		}
	}
}

int* initializeGame(){
	int* state;
	state = (int*) calloc(STATE_SIZE, sizeof(int));
	initializeBoard(state);
	initializeCastling(state);
	setEnPassant(state, -1, WHITE_IDX);setEnPassant(state, -1, BLACK_IDX);
	setPlayer(state, WHITE_IDX);

	return state;
}

void initializeGame(int* state){
	initializeBoard(state);
	initializeCastling(state);
	setEnPassant(state, -1, WHITE_IDX);setEnPassant(state, -1, BLACK_IDX);
	setPlayer(state, WHITE_IDX);
}


int clipSlideMoves(int* board, int player, int* position, int* direction){
	int x,y; 
	int move;
	move = 1;
	while(1){
		x = position[0] + move*direction[0];
		y = position[1] + move*direction[1];
		if(IS_NOT_VALID_IDX(x) || IS_NOT_VALID_IDX(y) || getBoardBox(board, player, x, y) != EMPTY){
			break;
		}
		++move;
		if(getBoardBox(board, OPPONENT(player), x, y) != EMPTY){
			break;
		}
	}
	return move - 1;
}

int positionCheck(int* board, int player, int x, int y){
	if(IS_NOT_VALID_IDX(x) || IS_NOT_VALID_IDX(y) || getBoardBox(board, player, x, y) != EMPTY){
		return -1;
	}

	return getBoardBox(board, OPPONENT(player), x, y);
}

void positionMoves(int* positions, int* board, int* castling, int* enPassant, int* position, int player, int onlyFetchAttackSquares){
	int box = getBoardBox(board, player, position[0], position[1]);
	int i, j;
	int x, y;
	int posI;
	int* direction;
	int clipLim, movement, flag, temp;
	int pos[2];
	posI=-1;

	switch(box){
		case EMPTY:break;
		case BISHOP:
			for(i=0;i<4;i++){
				direction = MOVE_DIRECTIONS_BISHOP[i];
				clipLim = clipSlideMoves(board, player, position, direction);
				for(j=1; j<=clipLim; j++){
					positions[++posI] = position[0] + j*direction[0];
					positions[++posI] = position[1] + j*direction[1];
					positions[++posI] = 0;
				}
			}
			break;
		case ROOK:
			for(i=0;i<4;i++){
				direction = MOVE_DIRECTIONS_ROOK[i];
				clipLim = clipSlideMoves(board, player, position, direction);
				for(j=1; j<=clipLim; j++){
					positions[++posI] = position[0] + j*direction[0];
					positions[++posI] = position[1] + j*direction[1];
					positions[++posI] = 0;
				}
			}
			break;
		case QUEEN:
			for(i=0;i<8;i++){
				direction = MOVE_DIRECTIONS_QUEEN[i];
				clipLim = clipSlideMoves(board, player, position, direction);
				for(j=1; j<=clipLim; j++){
					positions[++posI] = position[0] + j*direction[0];
					positions[++posI] = position[1] + j*direction[1];
					positions[++posI] = 0;
				}
			}
			break;
		case KNIGHT:
			for(i=0;i<8;i++){
				x = position[0] + KNIGHT_MOVES[i][0];
				y = position[1] + KNIGHT_MOVES[i][1];
				if(positionCheck(board, player, x, y)>=0){
					positions[++posI] = x;
					positions[++posI] = y;
					positions[++posI] = 0;
				}
			}
			break;
		case KING:
			for(i=0;i<8;i++){
				x = position[0] + KING_MOVES[i][0];
				y = position[1] + KING_MOVES[i][1];
				if(positionCheck(board, player, x, y)>=0){
					positions[++posI] = x;
					positions[++posI] = y;
					positions[++posI] = 0;
				}
			}
			if(onlyFetchAttackSquares == 0){
				if(getCastling(castling, player, LEFT_CASTLE)){
					flag = 1;
					for(i=1;i<3;i++){
						x = CASTLE_MOVES(player, LEFT_CASTLE)[i][0];
						y = CASTLE_MOVES(player, LEFT_CASTLE)[i][0];
						if(getBoardBox(board, player, x, y) != EMPTY || getBoardBox(board, OPPONENT(player), x, y) != EMPTY){
							flag = 0;break;
						}
					}
					for(i=0;flag && i<3;i++){
						pos[0]=x;pos[1]=y;
						if(positionAttacked(board, castling, enPassant, pos, player)){
							flag = 0;break;
						}
					}
					if(flag){
						positions[++posI] = CASTLE_MOVES(player, LEFT_CASTLE)[2][0];
						positions[++posI] = CASTLE_MOVES(player, LEFT_CASTLE)[2][1];
						positions[++posI] = 0;
					}
				}
				if(getCastling(castling, player, RIGHT_CASTLE)){
					flag = 1;
					for(i=1;i<3;i++){
						x = CASTLE_MOVES(player, RIGHT_CASTLE)[i][0];
						y = CASTLE_MOVES(player, RIGHT_CASTLE)[i][0];
						if(getBoardBox(board, player, x, y) != EMPTY || getBoardBox(board, OPPONENT(player), x, y) != EMPTY){
							flag = 0;break;
						}
					}
					for(i=0;flag && i<3;i++){
						pos[0]=x;pos[1]=y;
						if(positionAttacked(board, castling, enPassant, pos, player)){
							flag = 0;break;
						}
					}
					if(flag){
						positions[++posI] = CASTLE_MOVES(player, RIGHT_CASTLE)[2][0];
						positions[++posI] = CASTLE_MOVES(player, RIGHT_CASTLE)[2][1];
						positions[++posI] = 0;
					}
				}
			}
			break;
		case PAWN:
			for(i=0;i<2;i++){
				x = position[0] + PAWN_CAPTURE_MOVES[i][0];
				y = position[1] + PAWN_CAPTURE_MOVES[i][1]*PAWN_DIRECTION(player);
				temp = positionCheck(board, player, x, y);
				if(temp>0){
					if(onlyFetchAttackSquares==0 && x==KING_LINE(player)){
						positions[++posI] = x;
						positions[++posI] = y;
						positions[++posI] = ROOK;
						positions[++posI] = x;
						positions[++posI] = y;
						positions[++posI] = BISHOP;
						positions[++posI] = x;
						positions[++posI] = y;
						positions[++posI] = KNIGHT;
						positions[++posI] = x;
						positions[++posI] = y;
						positions[++posI] = QUEEN;
					}else{
						positions[++posI] = x;
						positions[++posI] = y;
						positions[++posI] = 0;
					}
				}else if(temp==0 && enPassant[player]==y && getBoardBox(board, OPPONENT(player), position[0], y)==PAWN){
					if(KING_LINE(OPPONENT(player)) + ((1+2)*PAWN_DIRECTION(OPPONENT(player))) == x){
						positions[++posI] = x;
						positions[++posI] = y;
						positions[++posI] = 0;
					}
				}
			}
			if(onlyFetchAttackSquares==0){
				x = position[0] + PAWN_NORMAL_MOVE[0];
				y = position[1] + PAWN_NORMAL_MOVE[1]*PAWN_DIRECTION(player);
				if(positionCheck(board, player, x, y)==0){
					if(y==KING_LINE(OPPONENT(player))){
						positions[++posI] = x;
						positions[++posI] = y;
						positions[++posI] = ROOK;
						positions[++posI] = x;
						positions[++posI] = y;
						positions[++posI] = BISHOP;
						positions[++posI] = x;
						positions[++posI] = y;
						positions[++posI] = KNIGHT;
						positions[++posI] = x;
						positions[++posI] = y;
						positions[++posI] = QUEEN;
					}else{
						positions[++posI] = x;
						positions[++posI] = y;
						positions[++posI] = 0;

						x = position[0] + PAWN_FIRST_MOVE[0];
						y = position[1] + PAWN_FIRST_MOVE[1]*PAWN_DIRECTION(player);
						if(positionCheck(board, player, x, y)==0){
							positions[++posI] = x;
							positions[++posI] = y;
							positions[++posI] = 0;
						}
					}
				}
			}
			break;
		default: printf("unknown piece!");break;
	}

}

void positionAllowedMoves(int* positions, int* state, int* position, int player){
	int i, j, gap;

	positionMoves(positions, state, position, player, 0);

	gap=0;
	for(i=0; i<MAX_PIECE_MOVES*3; i+=3){
		if(positions[i]==-1){
			break;
		}
		if(kingAttacked(state)){	tfiytfurdrd // updated board to be checked!
			gap+=3;
			continue;
		}
		if(gap){
			positions[i-gap+0] = positions[i+0];
			positions[i-gap+1] = positions[i+1];
			positions[i-gap+2] = positions[i+2];
		}
	}
	for(j=3;j<=gap;j+=3){
		positions[i-j+0] = -1;
		positions[i-j+1] = -1;
		positions[i-j+2] = -1;
	}
}

int positionAttacked(int* state, int* position, int player){
	int originalPiece;
	int i, j, flag;
	int positions[(MAX_PIECE_MOVES+1)*3];
	
	originalPiece = getBoardBox(state, player, position[0], position[1]);
	for(i=0;i<(MAX_PIECE_MOVES+1)*3;i++){
		positions[i] = -1;
	}

	flag = 0;
	for(i=0; i<6; i++){
		setBoardBox(state, PIECES[i], player, position[0], position[1]);
		positionMoves(positions, state, position, player, 1);

		for(j=0; positions[j]!=-1; j+=3){
			if(getBoardBox(state, OPPONENT(player), positions[j+0], positions[j+1]) == PIECES[i]){
				flag=1;break;
			}
		}
		if(flag){
			break;
		}
	}
	setBoardBox(state, originalPiece, player, position[0], position[1]);
	return flag;
}

int kingAttacked(int* state){
	int position[2];
	int i, j, flag;
	flag = 0;
	for(i=0; i<BOARD_SIZE; i++){
		for(j=0;j<BOARD_SIZE; j++){
			if(getBoardBox(state, getPlayer(state), i, j)==KING){
				position[0]=i;
				position[1]=j;
				flag=1;
				break;
			}
		}
		if(flag){
			break;
		}
	}
	if(!flag){
		printf("no king found!");
	}

	return positionAttacked(state, position, getPlayer(state));
}

void allActions(int* state, int *moves){
	int i, j , k, total;
	int pos[2], positions[(MAX_PIECE_MOVES+1)*3];

	total = 0;
	for(i=0; i<BOARD_SIZE; i++){
		for(j=0;j<BOARD_SIZE; j++){
			for(k=0;k<(MAX_PIECE_MOVES+1)*3;k++){
				positions[k] = -1;
			}
			pos[0]=i;
			pos[1]=j;
			positionAllowedMoves(positions, state, pos);
			
			for(k=0;positions[k]!=-1;k+=3){
				moves[total++] = i;
				moves[total++] = j;
				moves[total++] = positions[k+0];
				moves[total++] = positions[k+1];
				moves[total++] = positions[k+2];
			}
		}
	}
}

void performAction(int* state, int* move){
	int player, piece, opponentPiece;

	player = getPlayer(state);
	piece = getBoardBox(state, player, move[0], move[1]);
	opponentPiece = getBoardBox(state, OPPONENT(player), move[2], move[3]);

	setBoardBox(state, EMPTY, OPPONENT(player), move[2], move[3]);
	setBoardBox(state, EMPTY, player, move[0], move[1]);
	setBoardBox(state, piece, player, move[2], move[3]);
	
	setEnPassant(state, -1, player);

	switch(piece){
		case KING:
			setCastling(state, 0, player, LEFT_CASTLE);
			setCastling(state, 0, player, RIGHT_CASTLE);
			if(move[1] - move[3] == 2){
				setBoardBox(state, EMPTY, player, move[0], 0);
				setBoardBox(state, ROOK, player, move[0], move[1]-1);
			}else if(move[3] - move[1] == 2){
				setBoardBox(state, EMPTY, player, move[0], BOARD_SIZE-1);
				setBoardBox(state, ROOK, player, move[0], move[1]+1);
			}
			break;

		case PAWN:
			if(move[2]==KING_LINE(OPPONENT(player))){
				setBoardBox(state, move[4], player, move[2], move[3]);
			}else if(move[0]==(KING_LINE(player)+PAWN_DIRECTION(player)) && (move[0]-move[2]>1 || move[2]-move[0]>1)){
				setEnPassant(state, move[1], player);
			}else if(opponentPiece == EMPTY){
				setBoardBox(state, EMPTY, player, move[0], move[4]);
			}
			break;

		case ROOK:
			if(move[0] == 0 && getCastling(state, player, LEFT_CASTLE)){
				setCastling(state, 0, player, LEFT_CASTLE);
			}
			if(move[0] == BOARD_SIZE-1 && getCastling(state, player, RIGHT_CASTLE)){
				setCastling(state, 0, player, RIGHT_CASTLE);
			}
			break;

		default:break;
	}

	setPlayer(state, OPPONENT(player));
}

PyObject* __initializeGame(){
	int state[STATE_SIZE];
	initializeGame(state);
	return stateToPy(state);
}

PyObject* __performAction(PyObject* pyState, PyObject* pyMove){
	int state[STATE_SIZE], move[5];
	PyObject *oldPos, *newPos;

	stateFromPy(pyState, state);
	actionFromPy(pyMove, move);
	
	performAction(state, move);
	return stateToPy(state);
}

PyObject* __allActions(PyObject* pyState){
	int state[STATE_SIZE], positions[MAX_MOVES*5];
	stateFromPy(pyState, state);

	allActions(state, positions);
	return actionsToPy(positions);
}

PyObject* __kingAttacked(PyObject* pyState){
	int state[STATE_SIZE];
	stateFromPy(pyState, state);
	return PyBool_FromLong(kingAttacked(state));
}

PyObject* __positionAllowedMoves(PyObject* pyState, PyObject* position){
	int state[STATE_SIZE], pos[2], positions[(MAX_PIECE_MOVES+1)*3];
	int i;
	PyObject *actions;

	stateFromPy(pyState, state);
	pos[0] = (int)PyInt_AsLong(PyTuple_GetItem(position, PyInt_FromInt(0)));
	pos[1] = (int)PyInt_AsLong(PyTuple_GetItem(position, PyInt_FromInt(1)));
	for(i=0;i<MAX_PIECE_MOVES*3;i++){
		positions[i]=-1;
	}

	positionAllowedMoves(positions, state, pos);

	return actionsToPy(positions);
}