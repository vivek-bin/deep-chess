#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <Python.h>
#include <numpy/arrayobject.h>
#include <dirent.h>
#include <math.h>
#define NUM_SIMULATIONS 800
#define MAX_CHILDREN (PREDICTION_BATCH_SIZE-1)
#define PREDICTION_BATCH_SIZE 128
#define BACKPROP_DECAY 0.95
#define MC_EXPLORATION_CONST 0.5
#define STATE_HISTORY_LEN 10
#define BOARD_HISTORY 4
#define ACTION_SIZE 5 
#define BOARD_SIZE 8
#define STATE_SIZE (2*BOARD_SIZE*BOARD_SIZE + 4 + 2 + 1)
#define MAX_AVAILABLE_MOVES (4*(BOARD_SIZE-1)*2*BOARD_SIZE + 1)

#define WHITE_IDX 0
#define BLACK_IDX 1
#define LEFT_CASTLE 0
#define RIGHT_CASTLE 1

#define LEN(x) (sizeof(x)/sizeof(x[0]))

static int compareState(char state[], char state2[]);
static void copyState(char state[], char blankState[]);
static int getBoardBox(char state[], int player, int row, int col);
static int getEnPassant(char state[], int player);
static int getCastling(char state[], int player, int side);
static int getPlayer(char state[]);
static int actionIndex(char action[]);
static void actionFromIndex(int idx, char action[]);
static void stateIndex(char state[], char stateIdx[]);
static void stateFromIndex(char stateIdx[], char state[]);

static PyObject* stateToPy(char state[]);
static void stateFromPy(PyObject* pyState, char state[]);
static PyObject* actionsToPy(char positions[]);
static void actionFromPy(PyObject* pyAction, char action[]);

static void init(char state[], char actions[], int *endIdx, int *reward);
static void play(char state[], char action[], int duration, char actions[], int *endIdx, int *reward);


typedef struct NodeCommon NodeCommon;
typedef struct Node Node;
static void getStateHistory(Node *node, long limit, char **stateHistory);
static void prepareCModelInput(int batchSize, char *stateHistories[][BOARD_HISTORY], char *boardModelInput, char *otherModelInput);
static void setChildrenValuePolicy(Node *node, int setNodeValuePolicyAlso);
static double nodeValue(Node *node, int explore);
static Node* bestChild(Node *node);
static Node* getLeaf(Node *node);
static void backPropogate(Node *node);
static void runSimulations(Node *node);
static void saveNodeInfo(Node *node);
static void removeOtherChildren(Node *node, Node *best);
static void freeTree(Node *node);
static void freePyTree(PyObject *pyNode);
static int lastGameIndex(char *dataPath);
static void getStateHistoryFromPyHistory(PyObject *pyHistory, char stateHistory[][STATE_SIZE]);
static Node* initNodeChildren(Node *node, char actions[]);
static PyObject* __initTree(PyObject *self, PyObject *args);
static PyObject* __searchTree(PyObject *self, PyObject *args);



struct NodeCommon{
	long count;
	char training;
	PyObject *model;
	long gameIndex;
	char dataPath[2550];
	char rootStateHistory[STATE_HISTORY_LEN][STATE_SIZE];
	int rootStateHistoryLen;
	int pyCounter;
};

struct Node{
	NodeCommon *common;

	Node *parent;
	char isTop;
	char state[STATE_SIZE];
	int end;
	int reward;
	int numActions;
	int depth;

	long visits;
	double stateTotalValue;
	double stateValue;
	double exploreValue;


	char previousAction[2*ACTION_SIZE];
	float actionProbability;
	char repeat;
	Node *firstChild;
	Node *sibling;
};



static void getStateHistory(Node *node, long limit, char *stateHistory[]){
	long i, j;
	for(i=0; i<limit; i++){
		stateHistory[i] = NULL;
	}
	i = limit - 1;
	while(i>=0 && node!=NULL){
		stateHistory[i--] = node->state;
		if(node->parent == NULL && node->isTop){
			if(node->common->rootStateHistory != NULL){
				for(j = node->common->rootStateHistoryLen - 1; j>=0 && i>=0; j--){
					stateHistory[i--] = node->common->rootStateHistory[j];
				}
			}
			break;
		}
		node = node->parent;
	}
	for(i=0, j=0; i<limit; i++){
		if(stateHistory[i] == NULL){
			j++;
		}
		else{
			stateHistory[i-j] = stateHistory[i];
			if(j){
				stateHistory[i] = NULL;
			}
		}
	}
}

static void prepareCModelInput(int batchSize, char *stateHistories[][BOARD_HISTORY], char *boardModelInput, char *otherModelInput){
	int b, h, p, r, c, i, j;
	long long idx;

	// initialize used area = 0
	for(i=0; i<batchSize * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE; i++){
		boardModelInput[i] = 0;
	}
	// initialize unused area = -1
	for(; i<PREDICTION_BATCH_SIZE * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE; i++){
		boardModelInput[i] = -1;
	}
	for(b=0; b<batchSize; b++){
		for(j=0; j<STATE_HISTORY_LEN && stateHistories[b][j]!= NULL; j++);
		h = (j>BOARD_HISTORY?0:BOARD_HISTORY-j);
		for(; h<BOARD_HISTORY; h++){
			for(p=0; p<2; p++){
				for(r=0; r<BOARD_SIZE; r++){
					for(c=0; c<BOARD_SIZE; c++){
						idx = (((b*BOARD_HISTORY + h)*2 + p)*BOARD_SIZE + r)*BOARD_SIZE + c;
						boardModelInput[idx] = getBoardBox(stateHistories[b][h+(j>BOARD_HISTORY?j-BOARD_HISTORY:0)], p, r, c);
					}
				}
			}
		}
	}


	// initialize used area = 0
	for(i=0; i<batchSize * 3 * BOARD_SIZE*BOARD_SIZE; i++){
		otherModelInput[i] = 0;
	}
	// initialize unused area = -1
	for(; i<PREDICTION_BATCH_SIZE * 3 * BOARD_SIZE*BOARD_SIZE; i++){
		otherModelInput[i] = -1;
	}

	for(b=0; b<batchSize; b++){
		for(j=0; j<STATE_HISTORY_LEN && stateHistories[b][j]!= NULL; j++);
		if(j) h = j-1; else continue;

		// player info
		for(r=0; r<BOARD_SIZE; r++){
			for(c=0; c<BOARD_SIZE; c++){
				idx = ((b*3 + 0)*BOARD_SIZE + r)*BOARD_SIZE + c;
				otherModelInput[idx] = getPlayer(stateHistories[b][h]);
			}
		}

		// castling info
		if(getCastling(stateHistories[b][h], WHITE_IDX, LEFT_CASTLE)){
			for(c=0; c<BOARD_SIZE/2; c++){
				idx = ((b*3 + 1)*BOARD_SIZE + 0)*BOARD_SIZE + c;
				otherModelInput[idx] = 1;
			}
		}
		if(getCastling(stateHistories[b][h], WHITE_IDX, RIGHT_CASTLE)){
			for(c=BOARD_SIZE/2-1; c<BOARD_SIZE; c++){
				idx = ((b*3 + 1)*BOARD_SIZE + 0)*BOARD_SIZE + c;
				otherModelInput[idx] = 1;
			}
		}
		if(getCastling(stateHistories[b][h], BLACK_IDX, LEFT_CASTLE)){
			for(c=0; c<BOARD_SIZE/2; c++){
				idx = ((b*3 + 1)*BOARD_SIZE + BOARD_SIZE - 1)*BOARD_SIZE + c;
				otherModelInput[idx] = 1;
			}
		}
		if(getCastling(stateHistories[b][h], BLACK_IDX, RIGHT_CASTLE)){
			for(c=BOARD_SIZE/2-1; c<BOARD_SIZE; c++){
				idx = ((b*3 + 1)*BOARD_SIZE + BOARD_SIZE - 1)*BOARD_SIZE + c;
				otherModelInput[idx] = 1;
			}
		}

		// enpassant info
		if((c=getEnPassant(stateHistories[b][h], WHITE_IDX))>=0){
			for(r=0; r<3; r++){
				idx = ((b*3 + 2)*BOARD_SIZE + 1 + r)*BOARD_SIZE + c;
				otherModelInput[idx] = 1;
			}
		}
		if((c=getEnPassant(stateHistories[b][h], BLACK_IDX))>=0){
			for(r=0; r<3; r++){
				idx = ((b*3 + 2)*BOARD_SIZE + (BOARD_SIZE - 2) - r)*BOARD_SIZE + c;
				otherModelInput[idx] = 1;
			}
		}
	}
}

static void setChildrenValuePolicy(Node *node, int setNodeValuePolicyAlso){
	PyObject *predictionOutput, *predictionInput, *pyValues, *pyPolicies;
	PyObject *npBoard, *npOther;
	npy_intp boardDims[4], otherDims[4];
	char actions[ACTION_SIZE*MAX_AVAILABLE_MOVES];
	Node *child, *grandChild;

	char *repeatStateHistory[STATE_HISTORY_LEN];
	char **nodeStateHistory;
	char *stateHistories[PREDICTION_BATCH_SIZE][BOARD_HISTORY];
	char boardModelInput[PREDICTION_BATCH_SIZE * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE];
	char otherModelInput[PREDICTION_BATCH_SIZE * 3 * BOARD_SIZE*BOARD_SIZE];
	int end, reward;
	int batchSize, b, i, j;


	// find batch size
	for(batchSize=0; batchSize<PREDICTION_BATCH_SIZE && stateHistories[batchSize]!=NULL; batchSize++);
	
	getStateHistory(node, STATE_HISTORY_LEN, repeatStateHistory);
	for(i=0; i<STATE_HISTORY_LEN && repeatStateHistory[i]!=NULL; i++);
	nodeStateHistory = i>BOARD_HISTORY?&(repeatStateHistory[i-BOARD_HISTORY]):repeatStateHistory;

	b = 0;
	if(setNodeValuePolicyAlso){
		for(i=0; i<BOARD_HISTORY; i++){
			stateHistories[b][i] = nodeStateHistory[i];
		}
		b++;
	}
	for(child=(node->firstChild); child!=NULL; child=(child->sibling)){
		copyState(node->state, child->state);
		play(child->state, child->previousAction, node->depth, actions, &end, &reward);
		
		child->end = end;
		child->reward = reward?-1:0;
		for(j=0; j<MAX_AVAILABLE_MOVES && actions[j*ACTION_SIZE]>=0; j++);
		child->numActions = j;
		child->firstChild = initNodeChildren(child, actions);

		for(j=0; j<BOARD_HISTORY-1; j++){
			stateHistories[b][j] = nodeStateHistory[j+1];
		}
		stateHistories[b++][j] = child->state;

		for(j=0; j<STATE_HISTORY_LEN && repeatStateHistory[j]!=NULL; j++){
			if(compareState(child->state, repeatStateHistory[j])){
				child->repeat = 1;
				break;
			}
		}
	}



	prepareCModelInput(batchSize, stateHistories, boardModelInput, otherModelInput);

	boardDims[0] = batchSize;	boardDims[1] = BOARD_HISTORY*2;	boardDims[2] = BOARD_SIZE;	boardDims[3] = BOARD_SIZE;
	npBoard = PyArray_SimpleNewFromData(4, boardDims, NPY_INT8, boardModelInput);
	otherDims[0] = batchSize;	otherDims[1] = 3;	otherDims[2] = BOARD_SIZE;	otherDims[3] = BOARD_SIZE;
	npOther = PyArray_SimpleNewFromData(4, otherDims, NPY_INT8, otherModelInput);
	predictionInput = PyList_New(2);	PyList_SetItem(predictionInput, 0, npBoard);	PyList_SetItem(predictionInput, 1, npOther);

	predictionOutput = PyObject_CallMethod(node->common->model, "predict", "Oi", predictionInput, PREDICTION_BATCH_SIZE);
	
	pyValues = PyList_GetItem(predictionOutput, 0);
	pyPolicies = PyList_GetItem(predictionOutput, 1);


	b = 0;
	if(setNodeValuePolicyAlso){
		node->stateValue = *((float*)PyArray_GETPTR1(pyValues, b));
		for(child=(node->firstChild); child!=NULL; child=(child->sibling)){
			child->actionProbability = ((float*)PyArray_GETPTR1(pyPolicies, b))[actionIndex(child->previousAction)];
		}
		b++;
	}
	for(child=(node->firstChild); child!=NULL; child=(child->sibling)){
		child->stateValue = *((float*)PyArray_GETPTR1(pyValues, b));
		for(grandChild=(node->firstChild); grandChild!=NULL; grandChild=(grandChild->sibling)){
			grandChild->actionProbability = ((float*)PyArray_GETPTR1(pyPolicies, b))[actionIndex(grandChild->previousAction)];
		}
		b++;
	}

	Py_XDECREF(predictionInput);
	Py_XDECREF(predictionOutput);
}

static double nodeValue(Node *node, int explore){
	double value;
	value = (node->stateValue + node->stateTotalValue)/((double)(node->visits + 1));
	if(explore && node->common->training){
		value += MC_EXPLORATION_CONST * (node->actionProbability) * (node->exploreValue);
	}
	return value;
}

static Node* bestChild(Node *node){
	double totalValue, minimumValue;
	Node *child;
	long long r;
	double doubleR;

	double values[MAX_AVAILABLE_MOVES];
	Node *children[MAX_AVAILABLE_MOVES];
	int count, i, bestIdx;

	for(count=0, child=(node->firstChild); count<MAX_AVAILABLE_MOVES && child!=NULL; count++, child=(child->sibling)){
		values[count] = nodeValue(child, 1);
		children[count] = child;
	}

	
	if(node->common->training){
		for(minimumValue=999999999, i=0; i<count; i++){
			if(values[i]<minimumValue){
				minimumValue = values[i];
			}
		}

		for(totalValue=0, i=0; i<count; i++){
			values[i] = pow(values[i]-minimumValue, 2);
			totalValue += values[i];
		}
		doubleR = totalValue * 0.5;//(((double)(rand()))/((double)RAND_MAX));

		for(bestIdx=-1, totalValue=0, i=0; i<count; i++){
			totalValue += values[i];
			if(totalValue>doubleR){
				bestIdx = i;
				break;
			}
		}
	}
	else{
		for(bestIdx = 0, i=0; i<count; i++){
			if(values[i]>values[bestIdx]){
				bestIdx = i;
			}
		}
	}

	return children[bestIdx];
}

static Node* getLeaf(Node *node){
	while(node->firstChild != NULL){
		node = bestChild(node);
	}

	return node;
}

static void backPropogate(Node *node){
	double decay, leafValue, flip;

	leafValue = node->end?node->reward:node->stateValue;

	for(flip=1, decay=1; node!=NULL; flip*=-1, decay*=BACKPROP_DECAY, node=(node->parent)){
		(node->visits)++;
		(node->stateTotalValue) += leafValue * decay * flip;
		(node->exploreValue) = (node->parent!=NULL && node->common->training)?(sqrt(node->parent->visits)/((double)(1+node->visits))):1;
	}
}

static void runSimulations(Node *node){
	int i;
	Node *leaf;

	for(i=0; i<NUM_SIMULATIONS; i++){
		leaf = getLeaf(node);
		if(leaf->end==0){
			setChildrenValuePolicy(leaf, 0);
			leaf = bestChild(leaf);
		}
		backPropogate(leaf);
	}

}

static void saveNodeInfo(Node *node){
	FILE * oFile;
	Node *child;
	char stateIdx[STATE_SIZE+1];
	char fileName[2650];
	char *stateHistory[STATE_HISTORY_LEN];
	int i;

	sprintf(fileName, "%sgame_%05ld_move_%03d.json", node->common->dataPath, node->common->gameIndex, node->depth);
	oFile = fopen(fileName,"w");

	fprintf(oFile, "{");
	stateIndex(node->state, stateIdx);
	fprintf(oFile, "  \"STATE\" : \"%s\", \n", stateIdx);
	
	fprintf(oFile, "  \"STATE_HISTORY\" : [ \n");
	getStateHistory(node, STATE_HISTORY_LEN, stateHistory);
	for(i=0; i<STATE_HISTORY_LEN && stateHistory[i]!=NULL; i++){
		stateIndex(stateHistory[i], stateIdx);
		fprintf(oFile, "    \"%s\" ", stateIdx);
		if(i+1<STATE_HISTORY_LEN && stateHistory[i+1]!=NULL){fprintf(oFile, ",");}
		fprintf(oFile, "\n");
	}
	fprintf(oFile, "                    ], \n");

	fprintf(oFile, "  \"ACTIONS_POLICY\" : { \n");
	for(child=(node->firstChild); child!=NULL; child=(child->sibling)){
		fprintf(oFile, "    \"%i\" : %f ", actionIndex(child->previousAction), child->actionProbability);
		if(child->sibling != NULL){	fprintf(oFile, ",");}
		fprintf(oFile, "\n");
	}
	fprintf(oFile, "                     } \n");
	fprintf(oFile, "  \"SEARCHED_POLICY\" : { \n");
	for(child=(node->firstChild); child!=NULL; child=(child->sibling)){
		fprintf(oFile, "    \"%i\" : %f ", actionIndex(child->previousAction), ((float)(child->visits)/(float)(node->visits)));
		if(child->sibling != NULL){	fprintf(oFile, ",");}
		fprintf(oFile, "\n");
	}
	fprintf(oFile, "                     }, \n");

	fprintf(oFile, "  \"END\" : %i, \n", node->end);
	fprintf(oFile, "  \"REWARD\" : %i, \n", node->reward);
	fprintf(oFile, "  \"VALUE\" : %f, \n", nodeValue(node, 0));
	fprintf(oFile, "  \"EXPLORATORY_VALUE\" : %f, \n", nodeValue(node, 1));
	fprintf(oFile, "  \"TRAINING\" : %i, \n", node->common->training);

	fprintf(oFile, "  \"GAME_NUMBER\" : %li, \n", node->common->gameIndex);
	fprintf(oFile, "  \"MOVE_NUMBER\" : %i, \n", node->depth);
	fprintf(oFile, "}");
	
	fclose (oFile);
}

static void removeOtherChildren(Node *node, Node *best){
	Node *child;

	for(child=(node->firstChild); child!=NULL; child=(child->sibling)){
		if(child!=best){
			freeTree(child);
		}
	}
	best->sibling = NULL;
	node->firstChild = best;
}


static void freeTree(Node *node){
	Node *child, *nextChild;

	for(child=(node->firstChild); child!=NULL; child=nextChild){
		nextChild = child->sibling;
		freeTree(child);
	}

	node->common->count--;
	if(node->isTop){
		free(node->common);
	}
	
	free(node);
}

static void freePyTree(PyObject *pyNode){
	Node *node;
	node = (Node*)PyCapsule_GetPointer(pyNode, NULL);
	node->common->pyCounter--;
	if(node->common->pyCounter==0){
		while(node->parent != NULL){
			node = node->parent;
		}
		Py_XDECREF(node->common->model);
		freeTree(node);
	}
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

static void getStateHistoryFromPyHistory(PyObject *pyHistory, char stateHistory[][STATE_SIZE]){
	PyObject *pyState;
	int size, i;
	
	size = PyTuple_Size(pyHistory);
	for(i=0; i<size; i++){
		pyState = PyDict_GetItemString(PyTuple_GetItem(pyHistory, i), "STATE");
		stateFromPy(pyState, stateHistory[i]);
	}
}

static Node* initNodeChildren(Node *node, char actions[]){
	Node *child, *previousChild, *firstChild;
	int i, j, n;
	for(n=0; n<MAX_CHILDREN && actions[n*5]>=0; n++);
	if(n==0){
		return NULL;
	}
	previousChild = NULL;
	firstChild = NULL;
	for(i=0; i<n; i++){
		child = malloc(sizeof(Node));

		child->parent = node;
		child->common = child->parent->common;
		child->common->count++;
		child->depth = child->parent->depth + 1;
		child->isTop = 0;

		child->visits = 0;
		child->stateTotalValue = 0;
		child->stateValue = 0;
		child->exploreValue = sqrt(child->parent->visits);
		child->firstChild = NULL;


		for(j=0; j<ACTION_SIZE; j++){
			child->previousAction[j] = actions[i*ACTION_SIZE + j];
		}
		for(; j<2*ACTION_SIZE; j++){
			child->previousAction[j] = -1;
		}
		child->actionProbability = 0;
		child->repeat = 0;

		if(previousChild!=NULL){
			previousChild->sibling = child;
		}
		else{
			firstChild = child;
		}
		previousChild = child;
	}
	if(previousChild!=NULL){
		previousChild->sibling = NULL;
	}

	return firstChild;
}

static PyObject* __initTree(PyObject *self, PyObject *args){
	PyObject *pyState, *pyActions, *pyHistory, *pyModel;
	Node *root;
	NodeCommon *common;
	int end, reward, i;
	char actions[MAX_AVAILABLE_MOVES * ACTION_SIZE];
	char *dataPath;

	PyArg_ParseTuple(args, "OOiiOOs", pyState, pyActions, end, reward, pyHistory, pyModel, dataPath);
	
	for(i=0; i<MAX_AVAILABLE_MOVES * ACTION_SIZE; i++){
		actions[i] = -1;
	}
	for (i=0; i<PyTuple_Size(pyActions); i++){
		actionFromPy(PyTuple_GetItem(pyActions, i), &(actions[i*ACTION_SIZE]));
	}


	root = malloc(sizeof(Node));
	common = malloc(sizeof(NodeCommon));
	common->count = 1;
	common->rootStateHistoryLen = PyTuple_Size(pyHistory);
	getStateHistoryFromPyHistory(pyHistory, common->rootStateHistory);
	common->model = pyModel;	Py_XINCREF(pyModel);
	common->gameIndex = lastGameIndex(dataPath) + 1;
	common->training = 1;
	common->pyCounter = 1;
	strcpy(common->dataPath, dataPath);

	root->common = common;
	stateFromPy(pyState, root->state);
	root->end = end;
	root->reward = reward?-1:0;
	root->numActions = PyTuple_Size(pyActions);
	root->firstChild = initNodeChildren(root, actions);
	root->parent = NULL;
	root->depth = PyTuple_Size(pyHistory) + 1;
	root->isTop = 1;
	root->firstChild = NULL;

	root->visits = 0;
	root->stateTotalValue = 0;
	root->stateValue = 0;
	root->exploreValue = 1;
	root->actionProbability = 1;

	setChildrenValuePolicy(root, 1);

	return PyCapsule_New((void*)root, NULL, freePyTree);
}

static PyObject* __searchTree(PyObject *self, PyObject *args){
	PyObject *pyRoot, *output;
	PyObject *pyBestActions, *pyBestAction;
	Node *root, *best;

	pyRoot = PyTuple_GetItem(args, 0);
	root = (Node*)PyCapsule_GetPointer(pyRoot, NULL);


	runSimulations(root);
	saveNodeInfo(root);
	best = bestChild(root);
	removeOtherChildren(root, best);
	root->common->pyCounter++;


	pyBestActions = actionsToPy(best->previousAction);
	pyBestAction = PyTuple_GetItem(pyBestActions, 0);
	Py_XINCREF(pyBestAction);
	Py_XDECREF(pyBestActions);

	output = PyTuple_New(2);
	PyTuple_SetItem(output, 0, PyCapsule_New((void*)best, NULL, freePyTree));
	PyTuple_SetItem(output, 1, pyBestAction);
	return output;
}





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