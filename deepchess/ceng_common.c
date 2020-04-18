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

#define MAX_AVAILABLE_MOVES (4*(BOARD_SIZE-1)*2*BOARD_SIZE + 1)
#define STATE_SIZE (2*BOARD_SIZE*BOARD_SIZE + 4 + 2 + 1)
#define ACTION_SIZE 5
#define MAX_PIECE_MOVES (4*(BOARD_SIZE-1) + 1)
#define MAX_GAME_STEPS 200
#define LEN(x) (sizeof(x)/sizeof(x[0]))
#define OPPONENT(x) (x==WHITE_IDX?BLACK_IDX:WHITE_IDX)
#define SCORING(x) (x==WHITE_IDX?1:-1)
#define KING_LINE(x) (x==WHITE_IDX?0:BOARD_SIZE - 1)
#define PAWN_DIRECTION(x) (x==WHITE_IDX?1:-1)
#define IS_VALID_IDX(x) ((x>=0 && x<BOARD_SIZE)?1:0)
#define IS_NOT_VALID_IDX(x) ((x>=0 && x<BOARD_SIZE)?0:1)
#define CASTLE_MOVES(p, s) (p==WHITE_IDX?(s==LEFT_CASTLE?CASTLE_WHITE_LEFT:CASTLE_WHITE_RIGHT):(s==LEFT_CASTLE?CASTLE_BLACK_LEFT:CASTLE_BLACK_RIGHT))
#define MAX_POSSIBLE_MOVES ((BOARD_SIZE*BOARD_SIZE + 2 * LEN(PROMOTIONS))*BOARD_SIZE*BOARD_SIZE)

void __displayState(char state[]);
void __copyState(char state[], char blankState[]);
int __getBoardBox(char state[], int player, int row, int col);
void __setBoardBox(char state[], int value, int player, int row, int col);
int __getEnPassant(char state[], int player);
void __setEnPassant(char state[], int value, int player);
int __getCastling(char state[], int player, int side);
void __setCastling(char state[], int value, int player, int side);
int __getPlayer(char state[]);
void __setPlayer(char state[], int value);
int __actionIndex(char action[]);
void __actionFromIndex(int idx, char action[]);
void __stateIndex(char state[], char stateIdx[]);
void __stateFromIndex(char stateIdx[], char state[]);

PyObject* __stateToPy(char state[]);
void __stateFromPy(PyObject* pyState, char state[]);
PyObject* __actionsToPy(char positions[]);
void __actionFromPy(PyObject* pyAction, char action[]);

void __play(char state[], char action[], int duration, char actions[], int *endIdx, int *reward);


