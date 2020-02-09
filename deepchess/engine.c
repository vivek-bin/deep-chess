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

void getBoardFromState(PyObject* state, int* board){
	board = PyDict_GetItemString(state, "BOARD");
	ffjydyrd

}

void getCastlingFromState(PyObject* state, int* castling){
	castling[0] = (int) PyInt_AsLong(PyDict_GetItem(PyDict_GetItem(PyDict_GetItemString(state, "CASTLING_AVAILABLE"), PyInt_FromInt(WHITE_IDX)), PyInt_FromInt(LEFT_CASTLE)));
	castling[1] = (int) PyInt_AsLong(PyDict_GetItem(PyDict_GetItem(PyDict_GetItemString(state, "CASTLING_AVAILABLE"), PyInt_FromInt(WHITE_IDX)), PyInt_FromInt(RIGHT_CASTLE)));
	castling[2] = (int) PyInt_AsLong(PyDict_GetItem(PyDict_GetItem(PyDict_GetItemString(state, "CASTLING_AVAILABLE"), PyInt_FromInt(BLACK_IDX)), PyInt_FromInt(LEFT_CASTLE)));
	castling[3] = (int) PyInt_AsLong(PyDict_GetItem(PyDict_GetItem(PyDict_GetItemString(state, "CASTLING_AVAILABLE"), PyInt_FromInt(BLACK_IDX)), PyInt_FromInt(RIGHT_CASTLE)));
}

void getEnPassantFromState(PyObject* state, int* enPassant){
	enPassant[0] = (int) PyInt_AsLong(PyDict_GetItem(PyDict_GetItemString(state, "EN_PASSANT"), PyInt_FromInt(WHITE_IDX)));
	enPassant[1] = (int) PyInt_AsLong(PyDict_GetItem(PyDict_GetItemString(state, "EN_PASSANT"), PyInt_FromInt(BLACK_IDX)));
}

void getPlayerFromState(PyObject* state, int* player){
	*player = (int) PyInt_AsLong(PyDict_GetItemString(state, "PLAYER"));
}

int getBoardBox(int* board, int player, int row, int col){
	int idx = player*BOARD_SIZE*BOARD_SIZE + row*BOARD_SIZE + col;
	return board[idx];
}

void setBoardBox(int* board, int value, int player, int row, int col){
	int idx = player*BOARD_SIZE*BOARD_SIZE + row*BOARD_SIZE + col;
	board[idx] = value;
}

int getCastling(int* castling, int player, int side){
	return castling[player*2 + side];
}

void setCastling(int* castling, int value, int player, int side){
	castling[player*2 + side] = value;
}

void initializeBoard(int* board){
	int i;
	for(i=0; i<BOARD_SIZE; i++){
		setBoardBox(board, TOP_LINE[i], WHITE_IDX, KING_LINE(WHITE_IDX) + PAWN_DIRECTION(WHITE_IDX), i);
		setBoardBox(board, TOP_LINE[i], BLACK_IDX, KING_LINE(BLACK_IDX) + PAWN_DIRECTION(BLACK_IDX), i);
		setBoardBox(board, PAWN, WHITE_IDX, KING_LINE(WHITE_IDX) + PAWN_DIRECTION(WHITE_IDX), i);
		setBoardBox(board, PAWN, BLACK_IDX, KING_LINE(BLACK_IDX) + PAWN_DIRECTION(BLACK_IDX), i);
	}
}

void initializeCastling(int* castling, int* board){
	if(getBoardBox(board, WHITE_IDX, KING_LINE(WHITE_IDX), BOARD_SIZE/2) == KING){
		if(getBoardBox(board, WHITE_IDX, KING_LINE(WHITE_IDX), 0) == ROOK){
			setCastling(castling, 1, WHITE_IDX, LEFT_CASTLE);
		}
		if(getBoardBox(board, WHITE_IDX, KING_LINE(WHITE_IDX), BOARD_SIZE - 1) == ROOK){
			setCastling(castling, 1, WHITE_IDX, RIGHT_CASTLE);
		}
	}
	if(getBoardBox(board, BLACK_IDX, KING_LINE(BLACK_IDX), BOARD_SIZE/2) == KING){
		if(getBoardBox(board, BLACK_IDX, KING_LINE(BLACK_IDX), 0) == ROOK){
			setCastling(castling, 1, BLACK_IDX, LEFT_CASTLE);
		}
		if(getBoardBox(board, BLACK_IDX, KING_LINE(BLACK_IDX), BOARD_SIZE - 1) == ROOK){
			setCastling(castling, 1, BLACK_IDX, RIGHT_CASTLE);
		}
	}
}

PyObject* initializeGame(PyObject *self){
	PyObject* state;
	int* board;
	int* castling;
	int* enPassant;
	int* player;

	board = (int*) calloc(2*BOARD_SIZE*BOARD_SIZE, sizeof(int));
	initializeBoard(board);

	castling = (int*) calloc(2*2, sizeof(int));
	initializeCastling(castling, board);

	enPassant = (int*) calloc(2, sizeof(int));
	enPassant[WHITE_IDX] = -1;enPassant[BLACK_IDX] = -1;

	player = (int*) calloc(1, sizeof(int));
	*player = WHITE_IDX;

	state = PyDict_New();
	if(PyDict_SetItemString(state, "BOARD", board) == -1){
		free(board);free(castling);free(enPassant);free(player);
		return NULL;
	}
	if(PyDict_SetItemString(state, "CASTLING_AVAILABLE", castling) == -1){
		free(board);free(castling);free(enPassant);free(player);
		return NULL;
	}
	if(PyDict_SetItemString(state, "EN_PASSANT", enPassant) == -1){
		free(board);free(castling);free(enPassant);free(player);
		return NULL;
	}
	if(PyDict_SetItemString(state, "PLAYER", PyInt_FromInt(player)) == -1){
		free(board);free(castling);free(enPassant);free(player);
		return NULL;
	}

	return state;
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

void positionAllowedMoves(int* positions, int* board, int* castling, int* enPassant, int* position, int player){
	int i, j, gap;

	positionMoves(positions, board, castling, enPassant, position, player, 0);

	gap=0;
	for(i=0; i<MAX_PIECE_MOVES*3; i+=3){
		if(positions[i]==-1){
			break;
		}
		if(kingAttacked(board, castling, enPassant, player)){
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

int positionAttacked(int* board, int* castling, int* enPassant, int* position, int player){
	int originalPiece = getBoardBox(board, player, position[0], position[1]);
	int i, j, flag;
	int positions[(MAX_PIECE_MOVES+1)*3];
	for(i=0;i<MAX_PIECE_MOVES*3;i++){
		positions[i] = -1;
	}

	flag = 0;
	for(i=0; i<6; i++){
		setBoardBox(board, PIECES[i], player, position[0], position[1]);
		positionMoves(positions, board, castling, enPassant, position, player, 1);

		for(j=0; positions[j]!=-1; j+=3){
			if(getBoardBox(board, OPPONENT(player), positions[j+0], positions[j+1]) == PIECES[i]){
				flag=1;break;
			}
		}
		if(flag){
			break;
		}
	}
	setBoardBox(board, originalPiece, player, position[0], position[1]);
	return flag;
}

int kingAttacked(int* board, int* castling, int* enPassant, int player){
	int position[2];
	int i, j, flag;
	flag = 0;
	for(i=0; i<BOARD_SIZE; i++){
		for(j=0;j<BOARD_SIZE; j++){
			if(getBoardBox(board, player, i, j)==KING){
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

	return positionAttacked(board, castling, enPassant, position, player);
}

PyObject* __kingAttacked(PyObject* state){
	int board[2*BOARD_SIZE*BOARD_SIZE], castling[4], enPassant[2];
	int player;

	getBoardFromState(state, board);
	getCastlingFromState(state, castling);
	getEnPassantFromState(state, enPassant);
	getPlayerFromState(state, &player);

	return PyBool_FromLong(kingAttacked(board, castling, enPassant, player));
}

PyObject* __positionAllowedMoves(PyObject* state, PyObject* position){
	int board[2*BOARD_SIZE*BOARD_SIZE], castling[4], enPassant[2];
	int player;
	int positions[(MAX_PIECE_MOVES+1)*3], pos[2];
	PyObject *oldPos, *newPos, *move, *moves;
	int tot, i;

	getBoardFromState(state, board);
	getCastlingFromState(state, castling);
	getEnPassantFromState(state, enPassant);
	getPlayerFromState(state, &player);

	pos[0] = (int)PyInt_AsLong(PyTuple_GetItem(position, 0));
	pos[1] = (int)PyInt_AsLong(PyTuple_GetItem(position, 1));

	for(i=0;i<MAX_PIECE_MOVES*3;i++){
		positions[i]=-1;
	}
	positionAllowedMoves(positions, board, castling, enPassant, pos, player);
	
	tot=0;
	moves = PyTuple_New(MAX_PIECE_MOVES*2*BOARD_SIZE);
	for(i=0;positions[i]!=-1;i+=3){
		move = PyTuple_New(2);

		oldPos = PyTuple_New(2);
		PyTuple_SetItem(oldPos, 0, pos[0]);
		PyTuple_SetItem(oldPos, 1, pos[1]);
		PyTuple_SetItem(move, 0, oldPos);

		if(positions[i+2]){
			newPos = PyTuple_New(3);
			PyTuple_SetItem(newPos, 0, positions[i+0]);
			PyTuple_SetItem(newPos, 1, positions[i+1]);
			PyTuple_SetItem(newPos, 2, positions[i+2]);
		}else{
			newPos = PyTuple_New(2);
			PyTuple_SetItem(newPos, 0, positions[i+0]);
			PyTuple_SetItem(newPos, 1, positions[i+1]);
		}
		PyTuple_SetItem(move, 1, newPos);
		PyTuple_SetItem(moves, tot++, move);
	}
	_PyTuple_Resize(moves, tot);
	return moves;
}

PyObject* allActions(PyObject* state){
	int i,j,k;
	int posPositions[(MAX_PIECE_MOVES+1)*3];
	int position[2];
	int tot;
	PyObject *oldPos, *newPos, *move, *moves;

	int board[2*BOARD_SIZE*BOARD_SIZE], castling[4], enPassant[2];
	int player;

	getBoardFromState(state, board);
	getCastlingFromState(state, castling);
	getEnPassantFromState(state, enPassant);
	getPlayerFromState(state, &player);

	tot=0;
	moves = PyTuple_New(MAX_PIECE_MOVES*2*BOARD_SIZE);
	for(i=0; i<BOARD_SIZE; i++){
		for(j=0;j<BOARD_SIZE; j++){
			for(k=0;k<MAX_PIECE_MOVES*3;k++){
				posPositions[k] = -1;
			}
			position[0]=i;
			position[1]=j;
			positionAllowedMoves(posPositions, board, castling, enPassant, position, player);
			
			for(k=0;posPositions[k]!=-1;k+=3){
				move = PyTuple_New(2);

				oldPos = PyTuple_New(2);
				PyTuple_SetItem(oldPos, 0, i);
				PyTuple_SetItem(oldPos, 1, j);
				PyTuple_SetItem(move, 0, oldPos);

				if(posPositions[k+2]){
					newPos = PyTuple_New(3);
					PyTuple_SetItem(newPos, 0, posPositions[k+0]);
					PyTuple_SetItem(newPos, 1, posPositions[k+1]);
					PyTuple_SetItem(newPos, 2, posPositions[k+2]);
				}else{
					newPos = PyTuple_New(2);
					PyTuple_SetItem(newPos, 0, posPositions[k+0]);
					PyTuple_SetItem(newPos, 1, posPositions[k+1]);
				}
				PyTuple_SetItem(move, 1, newPos);
				PyTuple_SetItem(moves, tot++, move);
			}
		}
	}
	_PyTuple_Resize(moves, tot);
	return moves;
}

PyObject* performAction(PyObject* state, PyObject* move){
	PyObject *oldPos, *newPos;
	int oldX, oldY, newX, newY, newP;
	PyObject *oldXPy, *oldYPy, *newXPy, *newYPy, *newPPy;
	int piece, opponentPiece;
	int player;
	PyObject *boardPy, *boardOppPy, *castlingPy, *enPassantPy;
	PyObject *oldRow, *newRow, *piecePy;

	getPlayerFromState(state, &player);
	boardPy = PyDict_GetItem(PyDict_GetItemString(state, "BOARD"), PyInt_FromInt(player));
	boardOppPy = PyDict_GetItem(PyDict_GetItemString(state, "BOARD"), PyInt_FromInt(OPPONENT(player)));
	castlingPy = PyDict_GetItem(PyDict_GetItemString(state, "CASTLING_AVAILABLE"), PyInt_FromInt(player));
	enPassantPy = PyDict_GetItemString(state, "EN_PASSANT");


	oldXPy = PyTuple_GetItem(PyTuple_GetItem(move, 0), 0);
	oldYPy = PyTuple_GetItem(PyTuple_GetItem(move, 0), 1);
	newXPy = PyTuple_GetItem(PyTuple_GetItem(move, 1), 0);
	newYPy = PyTuple_GetItem(PyTuple_GetItem(move, 1), 1);
	oldX = (int) PyInt_AsLong(oldXPy);
	oldY = (int) PyInt_AsLong(oldYPy);
	newX = (int) PyInt_AsLong(newXPy);
	newY = (int) PyInt_AsLong(newYPy);
	if(PyTuple_Size(PyTuple_GetItem(move, 1))>2){
		newPPy = PyTuple_GetItem(PyTuple_GetItem(move, 1), 2);
		newP = (int) PyInt_AsLong(newPPy);
	}

	piecePy = PyList_GetItem(PyList_GetItem(boardPy, oldXPy), oldYPy);
	piece = (int) PyInt_AsLong(piecePy);
	opponentPiece = (int) PyInt_AsLong(PyList_GetItem(PyList_GetItem(boardOppPy, newXPy), newYPy));
	
	PyDict_SetItem(PyDict_GetItem(boardOppPy, newXPy), newYPy, PyInt_FromInt(EMPTY));
	PyDict_SetItem(PyDict_GetItem(boardPy, oldXPy), oldYPy, PyInt_FromInt(EMPTY));
	PyDict_SetItem(PyDict_GetItem(boardPy, newXPy), newYPy, piecePy);

	PyDict_SetItem(enPassantPy, PyInt_FromInt(player), PyInt_FromInt(-1));

	switch(piece){
		case KING:
			PyDict_SetItem(castlingPy, PyInt_FromInt(LEFT_CASTLE), PyInt_FromInt(0));
			PyDict_SetItem(castlingPy, PyInt_FromInt(RIGHT_CASTLE), PyInt_FromInt(0));
			
			if(oldY - newY == 2){
				PyDict_SetItem(PyDict_GetItem(boardPy, oldXPy), PyInt_FromInt(0), PyInt_FromInt(EMPTY));
				PyDict_SetItem(PyDict_GetItem(boardPy, oldXPy), PyInt_FromInt(oldY-1), PyInt_FromInt(ROOK));
			}else if(newY - oldY == 2){
				PyDict_SetItem(PyDict_GetItem(boardPy, oldXPy), PyInt_FromInt(BOARD_SIZE-1), PyInt_FromInt(EMPTY));
				PyDict_SetItem(PyDict_GetItem(boardPy, oldXPy), PyInt_FromInt(oldY+1), PyInt_FromInt(ROOK));
			}
			break;

		case PAWN:
			if(newX==KING_LINE(OPPONENT(player))){
				PyDict_SetItem(PyDict_GetItem(boardPy, newXPy), newYPy, newPPy);
			}else if(oldX==(KING_LINE(player)+PAWN_DIRECTION(player)) && (oldX-newX>1 || newX-oldX>1)){
				PyDict_SetItem(enPassantPy, PyInt_FromInt(player), oldYPy);
			}else if(opponentPiece == EMPTY){
				PyDict_SetItem(PyDict_GetItem(boardOppPy, oldXPy), newYPy, PyInt_FromInt(EMPTY));
			}
			break;

		case ROOK:
			if(oldX == 0 && CASTLE_MOVES(player, LEFT_CASTLE)){
				PyDict_SetItem(castlingPy, PyInt_FromInt(LEFT_CASTLE), PyInt_FromInt(0));
			}
			if(oldX == BOARD_SIZE-1 && CASTLE_MOVES(player, RIGHT_CASTLE)){
				PyDict_SetItem(castlingPy, PyInt_FromInt(RIGHT_CASTLE), PyInt_FromInt(0));
			}
			break;

		default:break;
	}









	PyDict_SetItemString(state, "PLAYER", PyInt_FromInt(OPPONENT(player)));
}

