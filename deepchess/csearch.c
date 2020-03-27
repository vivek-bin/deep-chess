#include <Python.h>
#include <dirent.h> 

#define NUM_SIMULATIONS 800
#define PREDICTION_BATCH_SIZE 128
#define BACKPROP_DECAY 0.95
#define MC_EXPLORATION_CONST 0.5
#define STATE_HISTORY_LEN 10
#define ACTION_SIZE 5 
#define BOARD_SIZE 8
#define STATE_SIZE (2*BOARD_SIZE*BOARD_SIZE + 4 + 2 + 1)
#define MAX_AVAILABLE_MOVES (4*(BOARD_SIZE-1)*2*BOARD_SIZE + 1)

#define LEN(x) (sizeof(x)/sizeof(x[0]))

static int actionIndex(char action[]);
static void actionFromIndex(int idx, char action[]);

static PyObject* stateToPy(char state[]);
static void stateFromPy(PyObject* pyState, char state[]);
static PyObject* actionsToPy(char positions[]);
static void actionFromPy(PyObject* pyAction, char action[]);

static void init(char state[], char actions[], int *endIdx, int *reward);
static void play(char state[], char action[], int duration, char actions[], int *endIdx, int *reward);


typedef struct Node Node;
typedef struct Child Child;

struct Node{
	long *count;
	char training;
	PyObject *model;
	long gameIndex;
	char *dataPath;


	Node *parent;
	char isTop;
	char state[STATE_SIZE];
	int end;
	int reward;
	int numActions;

	long visits;
	double stateTotalValue;
	double stateValue;
	double exploreValue;


	Child *children;
	char **stateHistory;
};

struct Child{
	char action[2*ACTION_SIZE];
	float actionProbability;
	Node *node;
};



static void getStateHistory(Node *root, long limit, char **stateHistory){
	long i, j;
	Node *node;
	for(i=0; i<limit; i++){
		stateHistory[i] = NULL;
	}
	node = root; i = limit - 1;
	while(i>=0){
		stateHistory[i--] = node->state;
		if(node->parent == NULL){
			if(node->stateHistory != NULL){
				for(j = STATE_HISTORY_LEN - 1; j>=0 && i>=0; j--){
					if(node->stateHistory[j] != NULL){
						stateHistory[i--] = stateHistory[j];
					}
				}
			}
			break;
		}
		node = node->parent;
	}
}






static void freeTree(Node *root){
	int i;

	for(i=0; i<root->numActions; i++){
		if(root->children[i].node != NULL){
			freeTree(root->children[i].node);
		}
	}

	*(root->count)--;
	free(root->children);
	if(root->isTop){
		if(root->stateHistory != NULL){
			for(i=0; i<STATE_HISTORY_LEN; i++){
				if(root->stateHistory[i] != NULL){
					free(root->stateHistory[i]);
				}
			}
			free(root->stateHistory);
		}
		free(root->count);
		free(root->dataPath);
	}
	
	free(root);
}

static void freePyTree(PyObject *pyRoot){
	Node *root;
	root = (Node*)PyCapsule_GetPointer(pyRoot, NULL);
	if(root->isTop){
		Py_XDECREF(root->model);
	}
	freeTree(root);
}

static int lastGameIndex(char *dataPath){
	int gameIndex, gameIndexTemp;
	char *fileName;

	gameIndex = -1;
	struct dirent *de;
	DIR *dr = opendir(dataPath);
	while ((de = readdir(dr)) != NULL){
		if(strstr(de->d_name, ".pickle") != NULL) {
			fileName = strstr(de->d_name, "game_");
			gameIndexTemp = 0;
			gameIndexTemp *= 10;	gameIndexTemp += (fileName[5]-'0');
			gameIndexTemp *= 10;	gameIndexTemp += (fileName[6]-'0');
			gameIndexTemp *= 10;	gameIndexTemp += (fileName[7]-'0');
			gameIndexTemp *= 10;	gameIndexTemp += (fileName[8]-'0');
			gameIndexTemp *= 10;	gameIndexTemp += (fileName[9]-'0');

			if(gameIndex < gameIndexTemp){
				gameIndex = gameIndexTemp;
			}
		}
	}
	closedir(dr);

	return gameIndex;
}

static char** getStateHistoryFromPyHistory(PyObject *pyHistory){
	PyObject *pyState;
	int size, i;
	char **stateHistory;
	size = PyTuple_Size(pyHistory);
	if(size == 0){
		return NULL;
	}

	stateHistory = malloc(STATE_HISTORY_LEN * sizeof(*stateHistory));
	for(i=0; i<STATE_HISTORY_LEN; i++){
		stateHistory[i] = NULL;
	}

	for(i=0; i<size; i++){
		pyState = PyDict_GetItemString(PyTuple_GetItem(pyHistory, i), "STATE");
		stateHistory[i] = malloc(STATE_SIZE * sizeof(**stateHistory));
		stateFromPy(pyState, stateHistory[i]);
	}

	return stateHistory;
}

static Child* initNodeChildren(int actions[]){
	Child* children;
	int i, j, n;
	for(n=0; actions[n*5]>=0; n++);
	if(n==0){
		return NULL;
	}
	children = (Child*)malloc(n*sizeof(Child));
	for(i=0; i<n; i++){
		for(j=0; j<ACTION_SIZE; j++){
			children[i].action[j] = actions[i*ACTION_SIZE + j];
		}
		for(; j<2*ACTION_SIZE; j++){
			children[i].action[j] = -1;
		}
		children[i].actionProbability = 0;
		children[i].node = NULL;
	}

	return children;
}

static PyObject* __initTree(PyObject *self, PyObject *args){
	PyObject *pyState, *pyActions, *pyHistory, *pyModel;
	Node *root;
	int end, reward, i;
	char actions[MAX_AVAILABLE_MOVES * ACTION_SIZE];
	char *dataPath;

	PyArg_ParseTuple(args, "OOiiOOs", pyState, pyActions, end, reward, pyHistory, pyModel, dataPath);
	Py_XINCREF(pyModel);
	for(i=0; i<MAX_AVAILABLE_MOVES * ACTION_SIZE; i++){
		actions[i] = -1;
	}
	for (i=0; i<PyTuple_Size(pyActions); i++){
		actionFromPy(PyTuple_GetItem(pyActions, i), actions[i*ACTION_SIZE]);
	}


	root = (Node*)malloc(sizeof(Node));
	stateFromPy(pyState, root->state);
	root->stateHistory = getStateHistoryFromPyHistory(pyHistory);
	root->model = pyModel;
	root->gameIndex = lastGameIndex(dataPath) + 1;
	root->count = malloc(sizeof(long)); *(root->count) = 1;
	root->training = 1;
	root->end = end;
	root->reward = reward?-1:0;
	root->dataPath = malloc(255*sizeof(char)); strcpy(root->dataPath, dataPath);
	root->numActions = PyTuple_Size(pyActions);
	root->children = initNodeChildren(actions);
	root->parent = NULL;

	root->visits = 0;
	root->stateTotalValue = 0;
	root->stateValue = 0;
	root->exploreValue = 1;


	return PyCapsule_New((void*)root, NULL, freePyTree);
}

static PyObject* __searchTree(PyObject *self, PyObject *args){
	PyObject *pyRoot, *pyAction, *output;
	PyObject *pyBestActions, *pyBestAction;
	Node *root;
	Child *bestChild;
	char bestAction[2*ACTION_SIZE];

	pyRoot = PyTuple_GetItem(args, 0);
	pyAction = PyTuple_GetItem(args, 1);
	root = (Node*)PyCapsule_GetPointer(pyRoot, NULL);



	runSimulations(root);
	saveRootInfo(root);
	bestChild = getBestChild(root);
	removeOtherChildren(root, bestChild);



	pyBestActions = actionsToPy(bestChild->action);
	pyBestAction = PyTuple_GetItem(pyBestActions, 0);
	Py_XINCREF(pyBestAction);
	Py_XDECREF(pyBestActions);

	output = PyTuple_New(2);
	PyTuple_SetItem(output, 0, PyCapsule_New((void*)bestChild->node, NULL, freePyTree));
	PyTuple_SetItem(output, 1, pyBestAction);
	return output;
}





/*
def prepareModelInput(stateHistories):
	batchSize = len(stateHistories)
	boardInput = np.ones((batchSize, CONST.BOARD_HISTORY, 2, EG.BOARD_SIZE, EG.BOARD_SIZE), dtype=np.int8) * EG.EMPTY
	playerInput = np.ones((batchSize, 1, EG.BOARD_SIZE, EG.BOARD_SIZE), dtype=np.int8)
	castlingStateInput = np.zeros((batchSize, 1, EG.BOARD_SIZE, EG.BOARD_SIZE), dtype=np.int8)
	enPassantStateInput = np.zeros((batchSize, 1, EG.BOARD_SIZE, EG.BOARD_SIZE), dtype=np.int8)
	for i, stateHistory in enumerate(stateHistories):
		for j in range(min(CONST.BOARD_HISTORY, len(stateHistory))):
			boardInput[i, j, :, :, :] = np.array(stateHistory[-1-j]["BOARD"], dtype=np.int8)
		
		state = stateHistory[-1]

		playerInput[i, 0] *= state["PLAYER"]
		
		for player in [EG.WHITE_IDX, EG.BLACK_IDX]:
			if state["CASTLING_AVAILABLE"][player][EG.LEFT_CASTLE]:
				castlingStateInput[i, 0, EG.KING_LINE[player], :EG.BOARD_SIZE//2] = 1
			if state["CASTLING_AVAILABLE"][player][EG.RIGHT_CASTLE]:
				castlingStateInput[i, 0, EG.KING_LINE[player], (EG.BOARD_SIZE//2 - 1):] = 1
		
		for player in [EG.WHITE_IDX, EG.BLACK_IDX]:
			if state["EN_PASSANT"][player] >= 0:
				movement = EG.PAWN_DIRECTION[player][0] * EG.PAWN_NORMAL_MOVE[0]
				pawnPos = EG.KING_LINE[player]
				for _ in range(3):
					pawnPos = pawnPos + movement
					enPassantStateInput[i, 0, pawnPos, state["EN_PASSANT"][player]] = 1

	boardInput = boardInput.reshape((batchSize, -1, EG.BOARD_SIZE, EG.BOARD_SIZE))
	stateInput = np.concatenate([playerInput, castlingStateInput, enPassantStateInput], axis=1)

	return (boardInput, stateInput)
*/



static PyMethodDef csearchMethods[] = {
    {"initTree",  __initTree, METH_VARARGS, "create new tree"},
    {"searchTree",  __searchTree, METH_VARARGS, "search for next move."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef csearchModule = {
   PyModuleDef_HEAD_INIT,
   "csearch",   /* name of module */
   NULL,       /* module documentation, may be NULL */
   -1,         /* size of per-interpreter state of the module, or -1 if the module keeps state in global variables. */
   csearchMethods
};

PyMODINIT_FUNC PyInit_csearch(void){
	PyObject *module;
	module = PyModule_Create(&csearchModule);

    return module;
}