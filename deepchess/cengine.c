#include <Python.h>

#define BOARD_SIZE 8
#define WHITE_IDX 0
#define BLACK_IDX 1

#define EMPTY 0
#define PAWN 1
#define BISHOP 2
#define KNIGHT 3
#define ROOK 4
#define QUEEN 5
#define KING 6

#define LEFT_CASTLE 0
#define RIGHT_CASTLE 1

#define MAX_PIECE_MOVES (4*(BOARD_SIZE-1) + 1)
#define MAX_AVAILABLE_MOVES (4*(BOARD_SIZE-1)*2*BOARD_SIZE + 1)
#define STATE_SIZE (2*BOARD_SIZE*BOARD_SIZE + 4 + 2 + 1)
#define MAX_GAME_STEPS 200

#define LEN(x) (sizeof(x)/sizeof(x[0]))
#define OPPONENT(x) (x==WHITE_IDX?BLACK_IDX:WHITE_IDX)
#define SCORING(x) (x==WHITE_IDX?1:-1)
#define KING_LINE(x) (x==WHITE_IDX?0:BOARD_SIZE - 1)
#define PAWN_DIRECTION(x) (x==WHITE_IDX?1:-1)
#define IS_VALID_IDX(x) ((x>=0 && x<BOARD_SIZE)?1:0)
#define IS_NOT_VALID_IDX(x) ((x>=0 && x<BOARD_SIZE)?0:1)
#define CASTLE_MOVES(p, s) (p==WHITE_IDX?(s==LEFT_CASTLE?CASTLE_WHITE_LEFT:CASTLE_WHITE_RIGHT):(s==LEFT_CASTLE?CASTLE_BLACK_LEFT:CASTLE_BLACK_RIGHT))
#define MAX_POSSIBLE_MOVES ((BOARD_SIZE*BOARD_SIZE + 2 * LEN(PROMOTIONS))*BOARD_SIZE*BOARD_SIZE + 1)
#define ACTION_SIZE 5

static void copyState(char state[], char blankState[]);
static int getBoardBox(char state[], int player, int row, int col);
static void setBoardBox(char state[], int value, int player, int row, int col);
static int getEnPassant(char state[], int player);
static void setEnPassant(char state[], int value, int player);
static int getCastling(char state[], int player, int side);
static void setCastling(char state[], int value, int player, int side);
static int getPlayer(char state[]);
static void setPlayer(char state[], int value);
static int actionIndex(char action[]);
static void actionFromIndex(int idx, char action[]);

static PyObject* stateToPy(char state[]);
static void stateFromPy(PyObject* pyState, char state[]);
static PyObject* actionsToPy(char positions[]);
static void actionFromPy(PyObject* pyAction, char action[]);

static void initializeBoard(char state[]);
static void initializeCastling(char state[]);
static void initializeGame(char state[]);

static int clipSlideMoves(char state[], int player, char position[], const int direction[]);
static int positionCheck(char state[], int player, int x, int y);
static void positionMoves(char positions[], char state[], char position[], int player, int onlyFetchAttackSquares);
static void positionAllowedMoves(char positions[], char state[], char position[]);
static int positionAttacked(char state[], char position[], int player);
static void kingPosition(char state[], int player, char position[]);
static int kingAttacked(char state[], int player);
static void allActions(char state[], char actions[]);
static void performAction(char state[], char move[]);
static int checkGameEnd(char state[], char actions[], int duration);
static PyObject* __kingAttacked(PyObject* self, PyObject *args);
static PyObject* __positionAllowedMoves(PyObject *self, PyObject *args);
static PyObject* __init(PyObject *self, PyObject *args);
static PyObject* __play(PyObject *self, PyObject *args);
PyMODINIT_FUNC PyInit_cengine(void);


const int PIECES[] = {PAWN, BISHOP, KNIGHT, ROOK, QUEEN, KING};
const int PROMOTIONS[] = {QUEEN, ROOK, KNIGHT, BISHOP};
const int TOP_LINE[] = {ROOK, KNIGHT, BISHOP, KING, QUEEN, BISHOP, KNIGHT, ROOK};

const int MOVE_DIRECTIONS_BISHOP[][2] = {{1, 1}, {1, -1}, {-1, -1}, {-1, 1}};
const int MOVE_DIRECTIONS_ROOK[][2] = {{1, 0}, {-1, 0}, {0, -1}, {0, 1}};
const int MOVE_DIRECTIONS_QUEEN[][2] = {{1, 0}, {-1, 0}, {0, -1}, {0, 1}, {1, 1}, {1, -1}, {-1, -1}, {-1, 1}};

const int KNIGHT_MOVES[][2] = {{1, 2}, {2, 1}, {-1, 2}, {2, -1}, {1, -2}, {-2, 1}, {-1, -2}, {-2, -1}};
const int KING_MOVES[][2] = {{1, 0}, {-1, 0}, {0, -1}, {0, 1}, {1, 1}, {1, -1}, {-1, -1}, {-1, 1}};

const int PAWN_CAPTURE_MOVES[][2] = {{1, 1}, {1, -1}};
const int PAWN_NORMAL_MOVE[2] = {1, 0};
const int PAWN_FIRST_MOVE[2] = {2, 0};

const int CASTLE_WHITE_LEFT[][2] = {{0, BOARD_SIZE/2 - 1}, {0, BOARD_SIZE/2 - 2}, {0, BOARD_SIZE/2 - 3}};
const int CASTLE_WHITE_RIGHT[][2] = {{0, BOARD_SIZE/2 - 1}, {0, BOARD_SIZE/2 + 0}, {0, BOARD_SIZE/2 + 1}};

const int CASTLE_BLACK_LEFT[][2] = {{BOARD_SIZE - 1, BOARD_SIZE/2 - 1}, {BOARD_SIZE - 1, BOARD_SIZE/2 - 2}, {BOARD_SIZE - 1, BOARD_SIZE/2 - 3}};
const int CASTLE_BLACK_RIGHT[][2] = {{BOARD_SIZE - 1, BOARD_SIZE/2 - 1}, {BOARD_SIZE - 1, BOARD_SIZE/2 + 0}, {BOARD_SIZE - 1, BOARD_SIZE/2 + 1}};

const char END_MESSAGE[4][25] = {"draw,max_steps", "draw,only_kings", "draw,stalemate", "loss"};



static void copyState(char state[], char blankState[]){
	int i;
	for(i=0; i<STATE_SIZE; i++){
		blankState[i] = state[i];
	}
}

static int getBoardBox(char state[], int player, int row, int col){
	int idx;
	idx = player*BOARD_SIZE*BOARD_SIZE + row*BOARD_SIZE + col;
	return state[idx];
}

static void setBoardBox(char state[], int value, int player, int row, int col){
	int idx;
	idx = player*BOARD_SIZE*BOARD_SIZE + row*BOARD_SIZE + col;
	state[idx] = value;
}

static int getEnPassant(char state[], int player){
	int offset;
	offset = BOARD_SIZE*BOARD_SIZE*2;
	return state[offset + player];
}

static void setEnPassant(char state[], int value, int player){
	int offset;
	offset = BOARD_SIZE*BOARD_SIZE*2;
	state[offset + player] = value;
}

static int getCastling(char state[], int player, int side){
	int offset;
	offset = BOARD_SIZE*BOARD_SIZE*2 + 2;
	return state[offset + player*2 + side];
}

static void setCastling(char state[], int value, int player, int side){
	int offset;
	offset = BOARD_SIZE*BOARD_SIZE*2 + 2;
	state[offset + player*2 + side] = value;
}

static int getPlayer(char state[]){
	int offset;
	offset = BOARD_SIZE*BOARD_SIZE*2 + 2 + 4;
	return state[offset];
}

static void setPlayer(char state[], int value){
	int offset;
	offset = BOARD_SIZE*BOARD_SIZE*2 + 2 + 4;
	state[offset] = value;
}

static int actionIndex(char action[]){
	int idx=0, p;
	if(action[4]>0){
		for(p=0; p<LEN(PROMOTIONS) && PROMOTIONS[p]!=action[4]; p++);
		idx = (action[2]/(BOARD_SIZE-1))*BOARD_SIZE + action[3];
		idx = (idx*BOARD_SIZE + action[1])*LEN(PROMOTIONS) + p;

		idx += 1 + BOARD_SIZE*BOARD_SIZE*BOARD_SIZE*BOARD_SIZE;
	}
	else{
		idx = action[0]*BOARD_SIZE + action[1];
		idx += (action[2]*BOARD_SIZE + action[3])*BOARD_SIZE*BOARD_SIZE;
	}
	return idx;
}

static void actionFromIndex(int idx, char action[]){
	if(idx<BOARD_SIZE*BOARD_SIZE*BOARD_SIZE*BOARD_SIZE){
		action[1] = idx % BOARD_SIZE;	idx /= BOARD_SIZE;
		action[0] = idx % BOARD_SIZE;	idx /= BOARD_SIZE;
		action[3] = idx % BOARD_SIZE;	idx /= BOARD_SIZE;
		action[2] = idx % BOARD_SIZE;	idx /= BOARD_SIZE;
		action[4] = 0;
	}
	else{
		idx -= 1 + BOARD_SIZE*BOARD_SIZE*BOARD_SIZE*BOARD_SIZE;
		
		action[4] = PROMOTIONS[idx % LEN(PROMOTIONS)];	idx /= LEN(PROMOTIONS);
		action[1] = idx % BOARD_SIZE; 					idx /= BOARD_SIZE;
		action[3] = idx % BOARD_SIZE; 					idx /= BOARD_SIZE;
		action[2] = idx * (BOARD_SIZE - 1);
		action[0] = action[2] + (action[2]==0?1:-1);
	}
}


static PyObject* stateToPy(char state[]){
	PyObject *pyState, *board, *boardPlayer, *boardRow, *castling, *castlingPlayer, *enPassant, *player;
	PyObject *white, *black, *left, *right, *v;
	int i, j, k;

	board = PyList_New(2);
	for(i=0; i<2; i++){
		boardPlayer = PyList_New(BOARD_SIZE);
		for(j=0; j<BOARD_SIZE; j++){
			boardRow = PyList_New(BOARD_SIZE);
			for(k=0; k<BOARD_SIZE; k++){
				PyList_SetItem(boardRow, k, PyLong_FromLong(getBoardBox(state, i, j, k)));
			}
			PyList_SetItem(boardPlayer, j, boardRow);
		}
		PyList_SetItem(board, i, boardPlayer);
	}

	white = PyLong_FromLong(WHITE_IDX);
	black = PyLong_FromLong(BLACK_IDX);
	left = PyLong_FromLong(LEFT_CASTLE);
	right = PyLong_FromLong(RIGHT_CASTLE);

	enPassant = PyDict_New();
	v = PyLong_FromLong(getEnPassant(state, WHITE_IDX));	PyDict_SetItem(enPassant, white, v);	Py_XDECREF(v);
	v = PyLong_FromLong(getEnPassant(state, BLACK_IDX));	PyDict_SetItem(enPassant, black, v);	Py_XDECREF(v);

	castling = PyDict_New();
	castlingPlayer = PyDict_New();
	v = PyLong_FromLong(getCastling(state, WHITE_IDX, LEFT_CASTLE));	PyDict_SetItem(castlingPlayer, left, v);	Py_XDECREF(v);
	v = PyLong_FromLong(getCastling(state, WHITE_IDX, RIGHT_CASTLE));	PyDict_SetItem(castlingPlayer, right, v);	Py_XDECREF(v);
	PyDict_SetItem(castling, white, castlingPlayer);Py_XDECREF(castlingPlayer);
	castlingPlayer = PyDict_New();
	v = PyLong_FromLong(getCastling(state, BLACK_IDX, LEFT_CASTLE));	PyDict_SetItem(castlingPlayer, left, v);	Py_XDECREF(v);
	v = PyLong_FromLong(getCastling(state, BLACK_IDX, RIGHT_CASTLE));	PyDict_SetItem(castlingPlayer, right, v);	Py_XDECREF(v);
	PyDict_SetItem(castling, black, castlingPlayer);Py_XDECREF(castlingPlayer);
	
	player = PyLong_FromLong(getPlayer(state));
	
	pyState = PyDict_New();
	PyDict_SetItemString(pyState, "BOARD", board);
	PyDict_SetItemString(pyState, "CASTLING_AVAILABLE", castling);
	PyDict_SetItemString(pyState, "EN_PASSANT", enPassant);
	PyDict_SetItemString(pyState, "PLAYER", player);

	Py_XDECREF(board);
	Py_XDECREF(castling);
	Py_XDECREF(enPassant);
	Py_XDECREF(player);

	Py_XDECREF(white);
	Py_XDECREF(black);
	Py_XDECREF(left);
	Py_XDECREF(right);
	return pyState;
}

static void stateFromPy(PyObject* pyState, char state[]){
	PyObject *board=NULL, *castling=NULL, *enPassant=NULL, *player=NULL, *item=NULL;
	PyObject *white=NULL, *black=NULL, *leftCastle=NULL, *rightCastle=NULL;
	int i, j, k;

	board = PyDict_GetItemString(pyState, "BOARD");
	castling = PyDict_GetItemString(pyState, "CASTLING_AVAILABLE");
	enPassant = PyDict_GetItemString(pyState, "EN_PASSANT");
	player = PyDict_GetItemString(pyState, "PLAYER");

	for(i=0; i<2; i++){
		for(j=0; j<BOARD_SIZE; j++){
			for(k=0; k<BOARD_SIZE; k++){
				item = PyList_GetItem(PyList_GetItem(PyList_GetItem(board, i), j), k);
				setBoardBox(state, PyLong_AsLong(item), i, j, k);
			}
		}
	}

	white = PyLong_FromLong(WHITE_IDX);
	black = PyLong_FromLong(BLACK_IDX);
	leftCastle = PyLong_FromLong(LEFT_CASTLE);
	rightCastle = PyLong_FromLong(RIGHT_CASTLE);

	setEnPassant(state, PyLong_AsLong(PyDict_GetItem(enPassant, white)), WHITE_IDX);
	setEnPassant(state, PyLong_AsLong(PyDict_GetItem(enPassant, black)), BLACK_IDX);

	item = PyDict_GetItem(PyDict_GetItem(castling, white), leftCastle);		setCastling(state, PyLong_AsLong(item), WHITE_IDX, LEFT_CASTLE);
	item = PyDict_GetItem(PyDict_GetItem(castling, white), rightCastle);	setCastling(state, PyLong_AsLong(item), WHITE_IDX, RIGHT_CASTLE);
	item = PyDict_GetItem(PyDict_GetItem(castling, black), leftCastle);		setCastling(state, PyLong_AsLong(item), BLACK_IDX, LEFT_CASTLE);
	item = PyDict_GetItem(PyDict_GetItem(castling, black), rightCastle);	setCastling(state, PyLong_AsLong(item), BLACK_IDX, RIGHT_CASTLE);

	setPlayer(state, PyLong_AsLong(player));

	Py_XDECREF(white);Py_XDECREF(black);Py_XDECREF(leftCastle);Py_XDECREF(rightCastle);
}

static PyObject* actionsToPy(char positions[]){
	PyObject *pyActions, *move, *oldPos, *newPos;
	int i, tot;

	tot = 0;
	for(i=0; positions[i*ACTION_SIZE]!=-1; i++);
	pyActions = PyTuple_New(i);
	for(i=0; positions[i]!=-1; ){
		move = PyTuple_New(2);

		oldPos = PyTuple_New(2);
		PyTuple_SetItem(oldPos, 0, PyLong_FromLong(positions[i++]));
		PyTuple_SetItem(oldPos, 1, PyLong_FromLong(positions[i++]));
		PyTuple_SetItem(move, 0, oldPos);

		if(positions[i+2]>0){
			newPos = PyTuple_New(3);
			PyTuple_SetItem(newPos, 0, PyLong_FromLong(positions[i++]));
			PyTuple_SetItem(newPos, 1, PyLong_FromLong(positions[i++]));
			PyTuple_SetItem(newPos, 2, PyLong_FromLong(positions[i++]));
		}else{
			newPos = PyTuple_New(2);
			PyTuple_SetItem(newPos, 0, PyLong_FromLong(positions[i++]));
			PyTuple_SetItem(newPos, 1, PyLong_FromLong(positions[i++]));
			i++;
		}
		PyTuple_SetItem(move, 1, newPos);
		PyTuple_SetItem(pyActions, tot++, move);
	}

	return pyActions;
}

static void actionFromPy(PyObject* pyAction, char action[]){
	PyObject *pos=NULL;

	pos = PyTuple_GetItem(pyAction, 0);
	action[0] = PyLong_AsLong(PyTuple_GetItem(pos, 0));
	action[1] = PyLong_AsLong(PyTuple_GetItem(pos, 1));

	pos = PyTuple_GetItem(pyAction, 1);
	action[2] = PyLong_AsLong(PyTuple_GetItem(pos, 0));
	action[3] = PyLong_AsLong(PyTuple_GetItem(pos, 1));
	action[4] = (PyTuple_Size(pos)>2)?PyLong_AsLong(PyTuple_GetItem(pos, 2)):-1;
}



static void initializeBoard(char state[]){
	int i, j, k;
	for(i=0; i<2; i++){
		for(j=0; j<BOARD_SIZE; j++){
			for(k=0; k<BOARD_SIZE; k++){
				setBoardBox(state, EMPTY, i, j, k);
			}
		}	
	}
	
	for(i=0; i<BOARD_SIZE; i++){
		setBoardBox(state, TOP_LINE[i], WHITE_IDX, KING_LINE(WHITE_IDX), i);
		setBoardBox(state, TOP_LINE[i], BLACK_IDX, KING_LINE(BLACK_IDX), i);
		setBoardBox(state, PAWN, WHITE_IDX, KING_LINE(WHITE_IDX) + PAWN_DIRECTION(WHITE_IDX), i);
		setBoardBox(state, PAWN, BLACK_IDX, KING_LINE(BLACK_IDX) + PAWN_DIRECTION(BLACK_IDX), i);
	}
}

static void initializeCastling(char state[]){
	setCastling(state, 0, WHITE_IDX, LEFT_CASTLE);
	setCastling(state, 0, WHITE_IDX, RIGHT_CASTLE);
	setCastling(state, 0, BLACK_IDX, LEFT_CASTLE);
	setCastling(state, 0, BLACK_IDX, RIGHT_CASTLE);
	if(getBoardBox(state, WHITE_IDX, KING_LINE(WHITE_IDX), CASTLE_MOVES(WHITE_IDX, LEFT_CASTLE)[0][1]) == KING){
		if(getBoardBox(state, WHITE_IDX, KING_LINE(WHITE_IDX), 0) == ROOK){
			setCastling(state, 1, WHITE_IDX, LEFT_CASTLE);
		}
		if(getBoardBox(state, WHITE_IDX, KING_LINE(WHITE_IDX), BOARD_SIZE - 1) == ROOK){
			setCastling(state, 1, WHITE_IDX, RIGHT_CASTLE);
		}
	}
	if(getBoardBox(state, BLACK_IDX, KING_LINE(BLACK_IDX), CASTLE_MOVES(BLACK_IDX, LEFT_CASTLE)[0][1]) == KING){
		if(getBoardBox(state, BLACK_IDX, KING_LINE(BLACK_IDX), 0) == ROOK){
			setCastling(state, 1, BLACK_IDX, LEFT_CASTLE);
		}
		if(getBoardBox(state, BLACK_IDX, KING_LINE(BLACK_IDX), BOARD_SIZE - 1) == ROOK){
			setCastling(state, 1, BLACK_IDX, RIGHT_CASTLE);
		}
	}
}

static void initializeGame(char state[]){
	initializeBoard(state);
	initializeCastling(state);
	setEnPassant(state, -1, WHITE_IDX);setEnPassant(state, -1, BLACK_IDX);
	setPlayer(state, WHITE_IDX);
}



static int clipSlideMoves(char state[], int player, char position[], const int direction[]){
	int x,y; 
	int move;
	move = 1;
	while(1){
		x = position[0] + move*direction[0];
		y = position[1] + move*direction[1];
		if(IS_NOT_VALID_IDX(x) || IS_NOT_VALID_IDX(y) || getBoardBox(state, player, x, y) != EMPTY){
			break;
		}
		++move;
		if(getBoardBox(state, OPPONENT(player), x, y) != EMPTY){
			break;
		}
	}
	return move - 1;
}

static int positionCheck(char state[], int player, int x, int y){
	if(IS_NOT_VALID_IDX(x) || IS_NOT_VALID_IDX(y) || getBoardBox(state, player, x, y) != EMPTY){
		return -1;
	}

	return getBoardBox(state, OPPONENT(player), x, y);
}

static void positionMoves(char positions[], char state[], char position[], int player, int onlyFetchAttackSquares){
	int box;
	int i, j, x, y;
	int posI;
	const int *direction;
	int clipLim, flag, temp;
	char pos[2];

	posI=-1;
	box = getBoardBox(state, player, position[0], position[1]);
	for(i=0; i<MAX_PIECE_MOVES*3; i++){
		positions[i] = -1;
	}

	switch(box){
		case EMPTY:break;
		case BISHOP:
			for(i=0;i<4;i++){
				direction = MOVE_DIRECTIONS_BISHOP[i];
				clipLim = clipSlideMoves(state, player, position, direction);
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
				clipLim = clipSlideMoves(state, player, position, direction);
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
				clipLim = clipSlideMoves(state, player, position, direction);
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
				if(positionCheck(state, player, x, y)>=0){
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
				if(positionCheck(state, player, x, y)>=0){
					positions[++posI] = x;
					positions[++posI] = y;
					positions[++posI] = 0;
				}
			}
			if(onlyFetchAttackSquares == 0){
				if(getCastling(state, player, LEFT_CASTLE)){
					flag = 1;
					for(i=1;i<3;i++){
						x = CASTLE_MOVES(player, LEFT_CASTLE)[i][0];
						y = CASTLE_MOVES(player, LEFT_CASTLE)[i][1];
						if(getBoardBox(state, player, x, y) != EMPTY || getBoardBox(state, OPPONENT(player), x, y) != EMPTY){
							flag = 0;break;
						}
					}
					for(i=0;flag && i<3;i++){
						pos[0] = CASTLE_MOVES(player, LEFT_CASTLE)[i][0];
						pos[1] = CASTLE_MOVES(player, LEFT_CASTLE)[i][1];
						if(positionAttacked(state, pos, player)){
							flag = 0;break;
						}
					}
					if(flag){
						positions[++posI] = CASTLE_MOVES(player, LEFT_CASTLE)[2][0];
						positions[++posI] = CASTLE_MOVES(player, LEFT_CASTLE)[2][1];
						positions[++posI] = 0;
					}
				}
				if(getCastling(state, player, RIGHT_CASTLE)){
					flag = 1;
					for(i=1;i<3;i++){
						x = CASTLE_MOVES(player, RIGHT_CASTLE)[i][0];
						y = CASTLE_MOVES(player, RIGHT_CASTLE)[i][1];
						if(getBoardBox(state, player, x, y) != EMPTY || getBoardBox(state, OPPONENT(player), x, y) != EMPTY){
							flag = 0;break;
						}
					}
					for(i=0;flag && i<3;i++){
						pos[0] = CASTLE_MOVES(player, RIGHT_CASTLE)[i][0];
						pos[1] = CASTLE_MOVES(player, RIGHT_CASTLE)[i][1];
						if(positionAttacked(state, pos, player)){
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
				x = position[0] + PAWN_CAPTURE_MOVES[i][0]*PAWN_DIRECTION(player);
				y = position[1] + PAWN_CAPTURE_MOVES[i][1];
				temp = positionCheck(state, player, x, y);
				if(temp>0){
					if(onlyFetchAttackSquares==0 && x==KING_LINE(OPPONENT(player))){
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
				}else if(temp==0 && getEnPassant(state, OPPONENT(player))==y && getBoardBox(state, OPPONENT(player), position[0], y)==PAWN){
					if(KING_LINE(OPPONENT(player)) + ((1+2)*PAWN_DIRECTION(OPPONENT(player))) == position[0]){
						positions[++posI] = x;
						positions[++posI] = y;
						positions[++posI] = 0;
					}
				}
			}
			if(onlyFetchAttackSquares==0){
				x = position[0] + PAWN_NORMAL_MOVE[0]*PAWN_DIRECTION(player);
				y = position[1] + PAWN_NORMAL_MOVE[1];
				if(positionCheck(state, player, x, y)==0){
					if(x==KING_LINE(OPPONENT(player))){
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

						if(position[0]==KING_LINE(player) + PAWN_DIRECTION(player)){
							x = position[0] + PAWN_FIRST_MOVE[0]*PAWN_DIRECTION(player);
							y = position[1] + PAWN_FIRST_MOVE[1];
							if(positionCheck(state, player, x, y)==0){
								positions[++posI] = x;
								positions[++posI] = y;
								positions[++posI] = 0;
							}
						}
					}
				}
			}
			break;
		default: printf("%i unknown piece! %i %i %i \n", box, player, position[0], position[1]);
			break;
	}
}

static void positionAllowedMoves(char positions[], char state[], char position[]){
	int i, j, gap;
	char tempState[STATE_SIZE], move[ACTION_SIZE];

	positionMoves(positions, state, position, getPlayer(state), 0);

	gap=0;
	for(i=0; i<MAX_PIECE_MOVES*3; i+=3){
		if(positions[i]==-1){
			break;
		}
		move[0] = position[0]; move[1] = position[1];
		move[2] = positions[i+0]; move[3] = positions[i+1]; move[4] = positions[i+2];
		copyState(state, tempState);
		performAction(tempState, move);
		if(kingAttacked(tempState, getPlayer(state))){
			gap+=3;
			continue;
		}
		if(gap){
			positions[i-gap+0] = positions[i+0];
			positions[i-gap+1] = positions[i+1];
			positions[i-gap+2] = positions[i+2];
		}
	}
	for(j=3; j<=gap; j+=3){
		positions[i-j+0] = -1;
		positions[i-j+1] = -1;
		positions[i-j+2] = -1;
	}
}

static int positionAttacked(char state[], char position[], int player){
	int originalPiece;
	int i, j, flag;
	char positions[MAX_PIECE_MOVES*3];
	
	originalPiece = getBoardBox(state, player, position[0], position[1]);

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

static void kingPosition(char state[], int player, char position[]){
	int i, j, flag;
	flag = 0;
	for(i=0; i<BOARD_SIZE; i++){
		for(j=0;j<BOARD_SIZE; j++){
			if(getBoardBox(state, player, i, j)==KING){
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
}

static int kingAttacked(char state[], int player){
	char position[2];
	kingPosition(state, player, position);

	return positionAttacked(state, position, player);
}

static void allActions(char state[], char actions[]){
	int i, j , k, total;
	char position[2], positions[MAX_PIECE_MOVES*3];
	
	for(i=0; i<MAX_AVAILABLE_MOVES*ACTION_SIZE; i++){
		actions[i] = -1;
	}

	total = 0;
	for(i=0; i<BOARD_SIZE; i++){
		for(j=0;j<BOARD_SIZE; j++){
			position[0]=i;
			position[1]=j;
			positionAllowedMoves(positions, state, position);
			
			for(k=0;positions[k]!=-1;k+=3){
				actions[total++] = i;
				actions[total++] = j;
				actions[total++] = positions[k+0];
				actions[total++] = positions[k+1];
				actions[total++] = positions[k+2];
			}
		}
	}
}

static void performAction(char state[], char move[]){
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
			}else if(move[0]==(KING_LINE(player)+PAWN_DIRECTION(player)) && (move[0]-move[2]==2 || move[2]-move[0]==2)){
				setEnPassant(state, move[1], player);
			}else if(opponentPiece == EMPTY && move[3]==getEnPassant(state, OPPONENT(player))){
				setBoardBox(state, EMPTY, OPPONENT(player), move[0], move[3]);
			}
			break;

		case ROOK:
			if(move[1] == 0 && getCastling(state, player, LEFT_CASTLE)){
				setCastling(state, 0, player, LEFT_CASTLE);
			}
			if(move[1] == BOARD_SIZE-1 && getCastling(state, player, RIGHT_CASTLE)){
				setCastling(state, 0, player, RIGHT_CASTLE);
			}
			break;

		default:break;
	}

	setPlayer(state, OPPONENT(player));
}

static int checkGameEnd(char state[], char actions[], int duration){
	int endIdx;
	int i,j,k,flag;
	endIdx = -1;
	
	if(duration>MAX_GAME_STEPS){
		endIdx = 0;
	}
	else{
		flag = 1;
		for(i=0; i<2; i++)
			for(j=0; j<BOARD_SIZE; j++)
				for(k=0; k<BOARD_SIZE; k++)
					if(getBoardBox(state, i, j, k) != EMPTY && getBoardBox(state, i, j, k) != KING)
						flag = 0;
		if(flag){
			endIdx = 1;
		}
	}
	if(actions[0]==-1){
		if(kingAttacked(state, getPlayer(state))){
			endIdx = 3;
		}
		else{
			endIdx = 2;
		}
	}
	return endIdx;
}

static void init(char state[], char actions[], int *endIdx, int *reward){
	initializeGame(state);
	allActions(state, actions);
	*endIdx = checkGameEnd(state, actions, 0);
	*reward = *endIdx==3? SCORING(OPPONENT(getPlayer(state))): 0;
}

static void play(char state[], char action[], int duration, char actions[], int *endIdx, int *reward){
	performAction(state, action);
	allActions(state, actions);
	*endIdx = checkGameEnd(state, actions, duration);
	*reward = *endIdx==3? SCORING(OPPONENT(getPlayer(state))): 0;
}

static PyObject* __actionIndex(PyObject *self, PyObject *args){
	char action[ACTION_SIZE];
	PyObject *pyAction;

	pyAction = PyTuple_GetItem(args, 0);
	actionFromPy(pyAction, action);
	
	return PyLong_FromLong(actionIndex(action));
}

static PyObject* __actionFromIndex(PyObject *self, PyObject *args){
	char action[10]={-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
	int idx;
	PyObject *pyActionIdx, *pyActions, *pyAction;

	pyActionIdx = PyTuple_GetItem(args, 0);
	idx = PyLong_AsLong(pyActionIdx);
	actionFromIndex(idx, action);

	pyActions = actionsToPy(action);
	pyAction = PyTuple_GetItem(pyActions, 0);
	Py_XINCREF(pyAction);
	Py_XDECREF(pyActions);	
	return pyAction;
}

static PyObject* __stateIndex(PyObject *self, PyObject *args){
	char state[STATE_SIZE+1];
	int i;
	PyObject *pyState;

	pyState = PyTuple_GetItem(args, 0);
	stateFromPy(pyState, state);
	for(i=0; i<STATE_SIZE; i++){
		state[i] = state[i] + '0';
	}
	state[STATE_SIZE] = '\0';
	
	return PyUnicode_FromString(state);
}

static PyObject* __stateFromIndex(PyObject *self, PyObject *args){
	char *state;
	int i;
	PyObject *pyStateIdx;

	pyStateIdx = PyTuple_GetItem(args, 0);
	state = PyBytes_AsString(PyUnicode_AsUTF8String(pyStateIdx));
	for(i=0; i<STATE_SIZE; i++){
		state[i] = state[i] - '0';
	}

	return stateToPy(state);
}

static PyObject* __kingAttacked(PyObject* self, PyObject *args){
	char state[STATE_SIZE], position[2];
	int flag;
	PyObject *pyState, *pyOutput;

	pyState = PyTuple_GetItem(args, 0);
	stateFromPy(pyState, state);

	flag = kingAttacked(state, getPlayer(state));
	if(flag){
		kingPosition(state, getPlayer(state), position);
		pyOutput = PyTuple_New(2);
		PyTuple_SetItem(pyOutput, 0, PyLong_FromLong(position[0]));
		PyTuple_SetItem(pyOutput, 1, PyLong_FromLong(position[1]));
	}
	else{
		pyOutput = PyBool_FromLong(flag);
	}
	return pyOutput;
}

static PyObject* __positionAllowedMoves(PyObject *self, PyObject *args){
	char state[STATE_SIZE], position[2], positions[MAX_PIECE_MOVES*3], actions[MAX_AVAILABLE_MOVES*ACTION_SIZE];
	int i, j;
	PyObject *pyState=NULL, *pyPosition=NULL, *x=NULL, *y=NULL;
	pyState = PyTuple_GetItem(args, 0);
	pyPosition = PyTuple_GetItem(args, 1);

	stateFromPy(pyState, state);
	x = PyTuple_GetItem(pyPosition, 0);y = PyTuple_GetItem(pyPosition, 1);
	position[0] = PyLong_AsLong(x);	position[1] = PyLong_AsLong(y);

	for(i=0;i<MAX_PIECE_MOVES*3;i++){
		positions[i]=-1;
	}

	positionAllowedMoves(positions, state, position);
	for(i=0;i<MAX_PIECE_MOVES*ACTION_SIZE;i++){
		actions[i]=-1;
	}
	for(i=0, j=0; i<MAX_PIECE_MOVES*ACTION_SIZE && positions[j]!=-1;){
		actions[i++]=position[0];
		actions[i++]=position[1];
		actions[i++]=positions[j++];
		actions[i++]=positions[j++];
		actions[i++]=positions[j++];
	}

	return actionsToPy(actions);
}

static PyObject* __init(PyObject *self, PyObject *args){
	char state[STATE_SIZE], actions[MAX_AVAILABLE_MOVES*ACTION_SIZE];
	int endIdx, reward;
	PyObject *output;

	init(state, actions, &endIdx, &reward);

	output = PyTuple_New(4);
	PyTuple_SetItem(output, 0, stateToPy(state));
	PyTuple_SetItem(output, 1, actionsToPy(actions));
	PyTuple_SetItem(output, 2, (endIdx>=0)?PyUnicode_FromString(END_MESSAGE[endIdx]):PyBool_FromLong(0));
	PyTuple_SetItem(output, 3, PyLong_FromLong(reward));
	return output;
}

static PyObject* __play(PyObject *self, PyObject *args){
	char state[STATE_SIZE], actions[MAX_AVAILABLE_MOVES*ACTION_SIZE], action[ACTION_SIZE];
	int endIdx, reward, duration;
	PyObject *output;
	PyObject *pyState, *pyAction, *pyDuration;
	pyState = PyTuple_GetItem(args, 0);
	pyAction = PyTuple_GetItem(args, 1);
	pyDuration = PyTuple_GetItem(args, 2);

	stateFromPy(pyState, state);
	actionFromPy(pyAction, action);
	duration = PyLong_AsLong(pyDuration);
	
	play(state, action, duration, actions, &endIdx, &reward);

	output = PyTuple_New(4);
	PyTuple_SetItem(output, 0, stateToPy(state));
	PyTuple_SetItem(output, 1, actionsToPy(actions));
	PyTuple_SetItem(output, 2, (endIdx>=0)?PyUnicode_FromString(END_MESSAGE[endIdx]):PyBool_FromLong(0));
	PyTuple_SetItem(output, 3, PyLong_FromLong(reward));
	return output;
}


static PyObject* __playRandomTillEnd(PyObject *self, PyObject *args){
	char state[STATE_SIZE], actions[MAX_AVAILABLE_MOVES*ACTION_SIZE], *action;
	int endIdx, reward, i;
	PyObject *output=NULL, *pyState=NULL;
	pyState = PyTuple_GetItem(args, 0);

	stateFromPy(pyState, state);
	allActions(state, actions);
	endIdx = checkGameEnd(state, actions, 0);
	
	while(endIdx==-1){
		for(i=0; actions[i*ACTION_SIZE]!=-1; i++);
		action = &actions[(rand()%i)*ACTION_SIZE];
		
		performAction(state, action);
		allActions(state, actions);
		endIdx = checkGameEnd(state, actions, 0);
	}
	reward = endIdx==3? SCORING(OPPONENT(getPlayer(state))): 0;

	output = PyTuple_New(4);
	PyTuple_SetItem(output, 0, stateToPy(state));
	PyTuple_SetItem(output, 1, actionsToPy(actions));
	PyTuple_SetItem(output, 2, (endIdx>=0)?PyUnicode_FromString(END_MESSAGE[endIdx]):PyBool_FromLong(0));
	PyTuple_SetItem(output, 3, PyLong_FromLong(reward));
	return output;
}

static PyMethodDef cengineMethods[] = {
    {"actionIndex",  __actionIndex, METH_VARARGS, "Convert action to index"},
    {"actionFromIndex",  __actionFromIndex, METH_VARARGS, "Get action from index"},
    {"stateIndex",  __stateIndex, METH_VARARGS, "Convert state to index"},
    {"stateFromIndex",  __stateFromIndex, METH_VARARGS, "Get state to index"},
    {"kingAttacked",  __kingAttacked, METH_VARARGS, "Check if the current players king is under check"},
    {"positionAllowedMoves",  __positionAllowedMoves, METH_VARARGS, "Get all possible positions from the given box."},
    {"init",  __init, METH_VARARGS, "Initialize the game."},
    {"play",  __play, METH_VARARGS, "Play a move in the game."},
    {"playRandomTillEnd",  __playRandomTillEnd, METH_VARARGS, "Play a given state till game end with random;y selected moves."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef cengineModule = {
   PyModuleDef_HEAD_INIT,
   "cengine",   /* name of module */
   NULL,       /* module documentation, may be NULL */
   -1,         /* size of per-interpreter state of the module, or -1 if the module keeps state in global variables. */
   cengineMethods
};

PyMODINIT_FUNC PyInit_cengine(void){
	int i;
	PyObject *module;
	PyObject *promotions, *kingLine, *pawnDirection, *pawnDirection2, *pawnNormalMove, *opponent, *scoring;
	PyObject *white, *black, *val;
	module = PyModule_Create(&cengineModule);
	PyModule_AddIntConstant(module, "BOARD_SIZE", BOARD_SIZE);
	PyModule_AddIntConstant(module, "MAX_GAME_STEPS", MAX_GAME_STEPS);
	PyModule_AddIntConstant(module, "WHITE_IDX", WHITE_IDX);
	PyModule_AddIntConstant(module, "BLACK_IDX", BLACK_IDX);
	PyModule_AddIntConstant(module, "EMPTY", EMPTY);
	PyModule_AddIntConstant(module, "PAWN", PAWN);
	PyModule_AddIntConstant(module, "BISHOP", BISHOP);
	PyModule_AddIntConstant(module, "KNIGHT", KNIGHT);
	PyModule_AddIntConstant(module, "ROOK", ROOK);
	PyModule_AddIntConstant(module, "QUEEN", QUEEN);
	PyModule_AddIntConstant(module, "KING", KING);
	PyModule_AddIntConstant(module, "LEFT_CASTLE", LEFT_CASTLE);
	PyModule_AddIntConstant(module, "RIGHT_CASTLE", RIGHT_CASTLE);
	PyModule_AddIntConstant(module, "MAX_POSSIBLE_MOVES", MAX_POSSIBLE_MOVES);

	white = PyLong_FromLong(WHITE_IDX);
	black = PyLong_FromLong(BLACK_IDX);

	promotions = PyTuple_New(LEN(PROMOTIONS));
	for(i=0; i<LEN(PROMOTIONS); i++){
		PyTuple_SetItem(promotions, i, PyLong_FromLong(PROMOTIONS[i]));
	}
	PyModule_AddObject(module, "PROMOTIONS", promotions);


	pawnNormalMove = PyTuple_New(LEN(PAWN_NORMAL_MOVE));
	for(i=0; i<LEN(PAWN_NORMAL_MOVE); i++){
		PyTuple_SetItem(pawnNormalMove, i, PyLong_FromLong(PAWN_NORMAL_MOVE[i]));
	}
	PyModule_AddObject(module, "PAWN_NORMAL_MOVE", pawnNormalMove);
	

	kingLine = PyDict_New();
	val = PyLong_FromLong(0);	PyDict_SetItem(kingLine, white, PyLong_FromLong(0));			Py_XDECREF(val);
	val = PyLong_FromLong(0);	PyDict_SetItem(kingLine, black, PyLong_FromLong(BOARD_SIZE-1));	Py_XDECREF(val);
	PyModule_AddObject(module, "KING_LINE", kingLine);
	

	pawnDirection = PyDict_New();
	pawnDirection2 = PyTuple_New(LEN(PAWN_NORMAL_MOVE));
	PyTuple_SetItem(pawnDirection2, 0, PyLong_FromLong(PAWN_DIRECTION(WHITE_IDX)));
	for(i=1; i<LEN(PAWN_NORMAL_MOVE); i++){
		PyTuple_SetItem(pawnDirection2, i, PyLong_FromLong(PAWN_NORMAL_MOVE[i]));
	}
	PyDict_SetItem(pawnDirection, white, pawnDirection2);	Py_XDECREF(pawnDirection2);
	pawnDirection2 = PyTuple_New(LEN(PAWN_NORMAL_MOVE));
	PyTuple_SetItem(pawnDirection2, 0, PyLong_FromLong(PAWN_DIRECTION(BLACK_IDX)));
	for(i=1; i<LEN(PAWN_NORMAL_MOVE); i++){
		PyTuple_SetItem(pawnDirection2, i, PyLong_FromLong(PAWN_NORMAL_MOVE[i]));
	}
	PyDict_SetItem(pawnDirection, black, pawnDirection2);	Py_XDECREF(pawnDirection2);
	PyModule_AddObject(module, "PAWN_DIRECTION", pawnDirection);
	

	opponent = PyDict_New();
	val = PyLong_FromLong(OPPONENT(WHITE_IDX));	PyDict_SetItem(opponent, white, PyLong_FromLong(OPPONENT(WHITE_IDX)));	Py_XDECREF(val);
	val = PyLong_FromLong(OPPONENT(BLACK_IDX));	PyDict_SetItem(opponent, black, PyLong_FromLong(OPPONENT(BLACK_IDX)));	Py_XDECREF(val);
	PyModule_AddObject(module, "OPPONENT", opponent);
	

	scoring = PyDict_New();
	val = PyLong_FromLong(SCORING(WHITE_IDX));	PyDict_SetItem(scoring, white, PyLong_FromLong(SCORING(WHITE_IDX)));	Py_XDECREF(val);
	val = PyLong_FromLong(SCORING(BLACK_IDX));	PyDict_SetItem(scoring, black, PyLong_FromLong(SCORING(BLACK_IDX)));	Py_XDECREF(val);
	PyModule_AddObject(module, "SCORING", scoring);

	Py_XDECREF(white);
	Py_XDECREF(black);
    return module;
}