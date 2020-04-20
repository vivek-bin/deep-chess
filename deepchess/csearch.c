#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "ceng_common.c"
#include <numpy/arrayobject.h>
#include <dirent.h>
#include <math.h>
#define MEMORY_BLOCK_SIZE (1<<19)
#define NUM_SIMULATIONS 600
#define BACKPROP_DECAY 0.98
#define MC_EXPLORATION_CONST 0.5
#define STATE_HISTORY_LEN 10
#define BOARD_HISTORY 4
#define MAX_CONCURRENT_GAMES 5
#define NUM_GENERATE_GAMES (MAX_CONCURRENT_GAMES * 20)


typedef struct NodeCommon NodeCommon;
typedef struct Node Node;
static Node* __nodeMalloc(NodeCommon *common);
static void __nodeFree(Node *node);
static int __compareState(char state[], char state2[]);
static void __getStateHistory(Node *node, long limit, char **stateHistory);
static PyObject* __prepareModelInput(int batchSize, char *stateHistories[][BOARD_HISTORY], char *boardModelInput, char *otherModelInput);
static void __expandChildren(Node *node);
static void __setChildrenHistories(Node *node, char *stateHistories[][BOARD_HISTORY]);
static double __nodeValue(Node *node, int explore);
static Node* __bestChild(Node *node);
static Node* __getLeaf(Node *node);
static void __backPropogate(Node *node);
static void __runSimulations(Node *roots[], int numConc);
static void __saveNodeInfo(Node *node);
static void __removeOtherChildren(Node *node, Node *best);
static void __freeTree(Node *node);
static void __freePyTree(PyObject *pyNode);
static int __lastGameIndex(char *dataPath);
static void __getStateHistoryFromPyHistory(PyObject *pyHistory, char stateHistory[][STATE_SIZE]);
static void __setChildrenValuePolicy(Node *node, PyObject *predictionOutput, int batchOffset);
static Node* __initNodeChildren(Node *node, char actions[]);
static void __test(PyObject *pyModel);
static void __freeMem(PyObject *capsule);
static PyObject* initTree(PyObject *self, PyObject *args);
static PyObject* searchTree(PyObject *self, PyObject *args);
static PyObject* generateGames(PyObject *self, PyObject *args);
static PyObject* test(PyObject *self, PyObject *args);



struct NodeCommon{
	long count;
	char training;
	PyObject *predictor;
	long gameIndex;
	char dataPath[2550];
	char rootStateHistory[STATE_HISTORY_LEN][STATE_SIZE];
	int rootStateHistoryLen;
	int pyCounter;
	Node *nodeBank;
};

struct Node{
	NodeCommon *common;

	Node *parent;
	char isTop;
	char state[STATE_SIZE];
	char end;
	int reward;
	int depth;
	char stateSetFlag;

	long visits;
	double stateTotalValue;
	double stateValue;

	char previousAction[ACTION_SIZE];
	double actionProbability;
	Node *firstChild;
	Node *sibling;
};


static Node* __nodeMalloc(NodeCommon *common){
	Node *node;
	long i;

	// allocate more if bank is empty
	if(common->nodeBank == NULL){
		common->nodeBank = malloc(sizeof(Node));
		common->nodeBank->sibling = NULL;
		for(i=0; i<MEMORY_BLOCK_SIZE; i++){
			node = malloc(sizeof(Node));
			node->sibling = common->nodeBank;
			common->nodeBank = node;
		}
	}

	// get new node
	node = common->nodeBank;
	common->nodeBank = node->sibling;
	common->count++;

	// initialize
	node->common = common;
	node->parent = NULL;
	node->isTop = 0;
	node->end = 0;
	node->reward = 0;
	node->depth = 0;
	node->stateSetFlag = 0;

	node->visits = 0;
	node->stateTotalValue = 0;
	node->stateValue = 0;

	for(i=0; i<ACTION_SIZE; i++){node->previousAction[i] = -1;}
	node->actionProbability = 0;
	node->sibling = NULL;
	node->firstChild = NULL;

	return node;
}

static void __nodeFree(Node *node){
	node->sibling = node->common->nodeBank;
	node->common->nodeBank = node;
	node->common->count--;
}

static int __compareState(char state[], char state2[]){
	int i;
	for(i=0; i<STATE_SIZE; i++){
		if(state2[i] != state[i]){
			return 0;
		}
	}
	return 1;
}

static void __getStateHistory(Node *node, long limit, char *stateHistory[]){
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

static PyObject* __prepareModelInput(int batchSize, char *stateHistories[][BOARD_HISTORY], char *boardModelInput, char *otherModelInput){
	int b, h, p, r, c, i, j, historyOffset;
	long long idx;
	PyObject *predictionInput;
	PyObject *npBoard, *npOther;
	npy_intp dims[4];

	// initialize used area = 0
	for(i=0; i<batchSize * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE; i++){
		boardModelInput[i] = 0;
	}
	// initialize unused area = -1
	for(; i<MAX_AVAILABLE_MOVES * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE; i++){
		boardModelInput[i] = -1;
	}
	for(b=0; b<batchSize; b++){
		for(j=0; j<BOARD_HISTORY && stateHistories[b][j]!= NULL; j++);
		historyOffset = j-1;
		for(h=0; h<BOARD_HISTORY && h<=historyOffset; h++){
			for(p=0; p<2; p++){
				for(r=0; r<BOARD_SIZE; r++){
					for(c=0; c<BOARD_SIZE; c++){
						idx = (((b*BOARD_HISTORY + h)*2 + p)*BOARD_SIZE + r)*BOARD_SIZE + c;
						boardModelInput[idx] = __getBoardBox(stateHistories[b][historyOffset-h], p, r, c);
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
	for(; i<MAX_AVAILABLE_MOVES * 3 * BOARD_SIZE*BOARD_SIZE; i++){
		otherModelInput[i] = -1;
	}

	for(b=0; b<batchSize; b++){
		for(j=0; j<BOARD_HISTORY && stateHistories[b][j]!= NULL; j++);
		if(j) h = j-1; else continue;

		// player info
		for(r=0; r<BOARD_SIZE; r++){
			for(c=0; c<BOARD_SIZE; c++){
				idx = ((b*3 + 0)*BOARD_SIZE + r)*BOARD_SIZE + c;
				otherModelInput[idx] = __getPlayer(stateHistories[b][h]);
			}
		}

		// castling info
		if(__getCastling(stateHistories[b][h], WHITE_IDX, LEFT_CASTLE)){
			for(c=0; c<BOARD_SIZE/2; c++){
				idx = ((b*3 + 1)*BOARD_SIZE + 0)*BOARD_SIZE + c;
				otherModelInput[idx] = 1;
			}
		}
		if(__getCastling(stateHistories[b][h], WHITE_IDX, RIGHT_CASTLE)){
			for(c=BOARD_SIZE/2-1; c<BOARD_SIZE; c++){
				idx = ((b*3 + 1)*BOARD_SIZE + 0)*BOARD_SIZE + c;
				otherModelInput[idx] = 1;
			}
		}
		if(__getCastling(stateHistories[b][h], BLACK_IDX, LEFT_CASTLE)){
			for(c=0; c<BOARD_SIZE/2; c++){
				idx = ((b*3 + 1)*BOARD_SIZE + BOARD_SIZE - 1)*BOARD_SIZE + c;
				otherModelInput[idx] = 1;
			}
		}
		if(__getCastling(stateHistories[b][h], BLACK_IDX, RIGHT_CASTLE)){
			for(c=BOARD_SIZE/2-1; c<BOARD_SIZE; c++){
				idx = ((b*3 + 1)*BOARD_SIZE + BOARD_SIZE - 1)*BOARD_SIZE + c;
				otherModelInput[idx] = 1;
			}
		}

		// enpassant info
		if((c=__getEnPassant(stateHistories[b][h], WHITE_IDX))>=0){
			for(r=0; r<3; r++){
				idx = ((b*3 + 2)*BOARD_SIZE + 1 + r)*BOARD_SIZE + c;
				otherModelInput[idx] = 1;
			}
		}
		if((c=__getEnPassant(stateHistories[b][h], BLACK_IDX))>=0){
			for(r=0; r<3; r++){
				idx = ((b*3 + 2)*BOARD_SIZE + (BOARD_SIZE - 2) - r)*BOARD_SIZE + c;
				otherModelInput[idx] = 1;
			}
		}
	}

	//prepare model input as numpy
	dims[0] = batchSize;	dims[1] = BOARD_HISTORY*2;	dims[2] = BOARD_SIZE;	dims[3] = BOARD_SIZE;
	npBoard = PyArray_SimpleNewFromData(4, dims, NPY_INT8, boardModelInput);
	dims[1] = 3;
	npOther = PyArray_SimpleNewFromData(4, dims, NPY_INT8, otherModelInput);
	predictionInput = PyList_New(2);	PyList_SetItem(predictionInput, 0, npBoard);	PyList_SetItem(predictionInput, 1, npOther);

	return predictionInput;
}

static void __expandChildren(Node *node){
	char actions[ACTION_SIZE*MAX_AVAILABLE_MOVES];
	Node *child, *previousChild, *nextChild;

	char *repeatStateHistory[STATE_HISTORY_LEN];
	int endIdx, reward;
	int j;

	__getStateHistory(node, STATE_HISTORY_LEN, repeatStateHistory);
	
	for(previousChild=NULL, child=(node->firstChild); child!=NULL; previousChild=child, child=nextChild){
		nextChild = (child->sibling);
		__copyState(node->state, child->state);
		__play(child->state, child->previousAction, child->depth, actions, &endIdx, &reward);
		
		child->stateSetFlag = 1;
		child->end = endIdx>=0?1:0;
		child->reward = reward?-1:0;
		child->firstChild = __initNodeChildren(child, actions);

		// if this child returns to a state already present in its heritage, remove it
		for(j=0; j<STATE_HISTORY_LEN && repeatStateHistory[j]!=NULL; j++){
			if(__compareState(child->state, repeatStateHistory[j])){
				if(previousChild==NULL){
					node->firstChild = child->sibling;
				}
				else{
					previousChild->sibling = child->sibling;
				}
				__freeTree(child);
				child = previousChild;
				break;
			}
		}
	}
}

static void __setChildrenHistories(Node *node, char *stateHistories[][BOARD_HISTORY]){
	Node *child;
	char *nodeStateHistory[BOARD_HISTORY];
	int b, i, j;

	for(i=0; i<MAX_AVAILABLE_MOVES; i++){
		for(j=0; j<BOARD_HISTORY; j++){
			stateHistories[i][j] = NULL;
		}
	}

	__getStateHistory(node, BOARD_HISTORY, nodeStateHistory);

	for(b=0, child=(node->firstChild); child!=NULL; child=(child->sibling)){
		for(i=0; i<BOARD_HISTORY-1 && nodeStateHistory[i+1]!=NULL; i++){
			stateHistories[b][i] = nodeStateHistory[i+1];
		}
		stateHistories[b++][i] = child->state;
	}
}

static void __setChildrenValuePolicy(Node *node, PyObject *predictionOutput, int batchOffset){
	PyArrayObject *pyValues, *pyPolicies;
	Node *child, *grandChild;
	long idx;
	int b;

	pyValues = PyList_GetItem(predictionOutput, 0);
	pyPolicies = PyList_GetItem(predictionOutput, 1);

	for(b=0, child=(node->firstChild); child!=NULL; child=(child->sibling), b++){
		child->stateValue = *((float*)PyArray_GETPTR2(pyValues, batchOffset+b, 0));
		for(grandChild=(child->firstChild); grandChild!=NULL; grandChild=(grandChild->sibling)){
			idx = __actionIndex(grandChild->previousAction);
			grandChild->actionProbability = *((float*)PyArray_GETPTR2(pyPolicies, batchOffset+b, idx));
		}
	}
}

static double __nodeValue(Node *node, int explore){
	double value;
	value = node->stateValue + node->stateTotalValue;
	if(explore && node->parent!=NULL && node->common->training){
		value += MC_EXPLORATION_CONST * sqrt((double)(node->parent->visits));
	}
	value *= (node->actionProbability);
	return value/(node->visits + 1);
}

static Node* __bestChild(Node *node){
	double totalValue, minimumValue;
	Node *child;
	double doubleR;

	double values[MAX_AVAILABLE_MOVES];
	Node *children[MAX_AVAILABLE_MOVES];
	int count, i, bestIdx;

	for(count=0, child=(node->firstChild); count<MAX_AVAILABLE_MOVES && child!=NULL; count++, child=(child->sibling)){
		values[count] = __nodeValue(child, 1);
		children[count] = child;//printf("\n          %f  %f %x", values[count], children[count]->stateValue, children[count]);
	}

	// if we have a move which ends the game with a win(!draw), we always take it
	for(i=0; i<count; i++){
		if((children[i]->end) && (children[i]->reward)){
			return children[i];
		}
	}
	
	if(node->common->training){
		for(minimumValue=999999999, i=0; i<count; i++){
			if(values[i]<minimumValue){
				minimumValue = values[i];
			}
		}

		for(totalValue=0, i=0; i<count; i++){
			values[i] = pow(values[i]-minimumValue, 2) + 1e-3;
			totalValue += values[i];
		}
		doubleR = totalValue * (((double)(rand()))/((double)RAND_MAX));

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

static Node* __getLeaf(Node *node){
	while(node->end==0 && node->firstChild->stateSetFlag){
		node = __bestChild(node);
	}

	return node;
}

static void __backPropogate(Node *node){
	double decay, leafValue, flippingDecay;

	flippingDecay = BACKPROP_DECAY * -1;
	leafValue = node->end?node->reward:node->stateValue;

	for(decay=1; node!=NULL; decay*=flippingDecay, node=(node->parent)){
		node->visits++;
		node->stateTotalValue += leafValue * decay;
	}
}

static void __runSimulations(Node *roots[], int numConc){
	char *stateHistories[MAX_CONCURRENT_GAMES * MAX_AVAILABLE_MOVES][BOARD_HISTORY];
	char boardModelInput[MAX_CONCURRENT_GAMES*MAX_AVAILABLE_MOVES * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE];
	char otherModelInput[MAX_CONCURRENT_GAMES*MAX_AVAILABLE_MOVES * 3 * BOARD_SIZE*BOARD_SIZE];
	Node *leafs[MAX_CONCURRENT_GAMES], *child;
	PyObject *predictionInput, *predictionOutput;
	int batchSizes[MAX_CONCURRENT_GAMES], batchOffset[MAX_CONCURRENT_GAMES], totalBatchSize;
	int i;

	totalBatchSize = 0;
	for(i=0; i<numConc; i++){
		batchSizes[i] = 0;
		batchOffset[i] = 0;
		if(roots[i]->end){
			leafs[i] = NULL;
		}
		else{
			leafs[i] = __getLeaf(roots[i]);
		}
	}

	for(i=0; i<numConc; i++){
		if(leafs[i] != NULL && leafs[i]->end == 0){
			//find batch sizes
			for(batchSizes[i]=0, child=(leafs[i]->firstChild); batchSizes[i]<MAX_AVAILABLE_MOVES && child!=NULL; batchSizes[i]++, child=(child->sibling));

			// fetch histories
			__expandChildren(leafs[i]);
			__setChildrenHistories(leafs[i], &(stateHistories[totalBatchSize]));

			batchOffset[i] = totalBatchSize;
			totalBatchSize += batchSizes[i];
		}
	}
	if(totalBatchSize > 0){
		//prepare model input as numpy
		predictionInput = __prepareModelInput(totalBatchSize, stateHistories, boardModelInput, otherModelInput);
		if(predictionInput==NULL){printf(" ---- prediction input creation error \n");PyErr_Print();}

		//prediction using model
		predictionOutput = PyObject_CallFunction(roots[0]->common->predictor, "O", predictionInput);
		if(predictionOutput==NULL){printf(" ---- prediction error \n");PyErr_Print();}
		
		for(i=0; i<numConc; i++){
			if(batchSizes[i] > 0){
				//set children value policy
				__setChildrenValuePolicy(leafs[i], predictionOutput, batchOffset[i]);
			}
		}
		Py_XDECREF(predictionInput);Py_XDECREF(predictionOutput);
	}

	// after expansion, select best child of node
	for(i=0; i<numConc; i++){
		if(leafs[i] != NULL && leafs[i]->end == 0){
			leafs[i] = __bestChild(leafs[i]);
		}
	}
	
	// backpropogate all active game trees
	for(i=0; i<numConc; i++){
		if(leafs[i] != NULL){
			__backPropogate(leafs[i]);
		}
	}
}

static void __saveNodeInfo(Node *node){
	FILE * oFile;
	Node *child;
	char stateIdx[STATE_SIZE+1];
	char fileName[2650];
	char *stateHistory[STATE_HISTORY_LEN];
	int i;

	sprintf(fileName, "%sgame_%05ld_move_%03d.json", node->common->dataPath, node->common->gameIndex, node->depth);
	oFile = fopen(fileName,"w");

	fprintf(oFile, "{");
	__stateIndex(node->state, stateIdx);
	fprintf(oFile, "  \"STATE\" : \"%s\", \n", stateIdx);
	
	fprintf(oFile, "  \"STATE_HISTORY\" : [ \n");
	__getStateHistory(node, STATE_HISTORY_LEN, stateHistory);
	for(i=0; i<STATE_HISTORY_LEN && stateHistory[i]!=NULL; i++){
		__stateIndex(stateHistory[i], stateIdx);
		fprintf(oFile, "    \"%s\" ", stateIdx);
		if(i+1<STATE_HISTORY_LEN && stateHistory[i+1]!=NULL){fprintf(oFile, ",");}
		fprintf(oFile, "\n");
	}
	fprintf(oFile, "                    ], \n");

	fprintf(oFile, "  \"ACTIONS_POLICY\" : { \n");
	for(child=(node->firstChild); child!=NULL; child=(child->sibling)){
		fprintf(oFile, "    \"%i\" : %f ", __actionIndex(child->previousAction), child->actionProbability);
		if(child->sibling != NULL){	fprintf(oFile, ",");}
		fprintf(oFile, "\n");
	}
	fprintf(oFile, "                     }, \n");
	fprintf(oFile, "  \"SEARCHED_POLICY\" : { \n");
	for(child=(node->firstChild); child!=NULL; child=(child->sibling)){
		fprintf(oFile, "    \"%i\" : %f ", __actionIndex(child->previousAction), ((float)(child->visits)/(float)(node->visits)));
		if(child->sibling != NULL){	fprintf(oFile, ",");}
		fprintf(oFile, "\n");
	}
	fprintf(oFile, "                     }, \n");

	fprintf(oFile, "  \"END\" : %i, \n", node->end);
	fprintf(oFile, "  \"REWARD\" : %i, \n", node->reward);
	fprintf(oFile, "  \"STATE_VALUE\" : %f, \n", node->stateValue);
	fprintf(oFile, "  \"VALUE\" : %f, \n", __nodeValue(node, 0));
	fprintf(oFile, "  \"EXPLORATORY_VALUE\" : %f, \n", __nodeValue(node, 1));
	fprintf(oFile, "  \"TRAINING\" : %i, \n", node->common->training);

	fprintf(oFile, "  \"GAME_NUMBER\" : %li, \n", node->common->gameIndex);
	fprintf(oFile, "  \"MOVE_NUMBER\" : %i \n", node->depth);
	fprintf(oFile, "}");
	
	fclose (oFile);
}

static void __removeOtherChildren(Node *node, Node *best){
	Node *child, *nextChild;

	for(child=(node->firstChild); child!=NULL; child=nextChild){
		nextChild = child->sibling;
		if(child!=best){
			__freeTree(child);
		}
	}
	best->sibling = NULL;
	node->firstChild = best;
}

static void __freeTree(Node *node){
	Node *child, *nextChild;
	NodeCommon *common;

	common = node->common;
	
	for(child=(node->firstChild); child!=NULL; child=nextChild){
		nextChild = child->sibling;
		__freeTree(child);
	}
	__nodeFree(node);

	if(common->count == 0){
		printf("\nFreeing tree memory \n");
		for(child=(common->nodeBank); child!=NULL; child=nextChild){
			nextChild = child->sibling;
			free(child);
		}
		free(common);
	}
}

static void __freePyTree(PyObject *pyNode){
	Node *node;
	node = (Node*)PyCapsule_GetPointer(pyNode, NULL);
	node->common->pyCounter--;
	if(node->common->pyCounter==0){
		printf("\nFreeing py tree \n");
		while(node->parent != NULL){
			node = node->parent;
		}
		Py_XDECREF(node->common->predictor);
		__freeTree(node);
	}
}

static int __lastGameIndex(char *dataPath){
	int gameIndex, gameIndexTemp;
	char *fileName;

	gameIndex = -1;
	struct dirent *de;
	DIR *dr = opendir(dataPath);
	while ((de = readdir(dr)) != NULL){
		if(strstr(de->d_name, ".json") != NULL) {
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

static void __getStateHistoryFromPyHistory(PyObject *pyHistory, char stateHistory[][STATE_SIZE]){
	PyObject *pyState;
	int size, i;
	
	size = PyList_Size(pyHistory);
	for(i=0; i<size; i++){
		pyState = PyDict_GetItemString(PyList_GetItem(pyHistory, i), "STATE");
		__stateFromPy(pyState, stateHistory[i]);
	}
}

static Node* __initNodeChildren(Node *node, char actions[]){
	Node *child, *previousChild, *firstChild;
	int i, j, n;
	for(n=0; n<MAX_AVAILABLE_MOVES && actions[n*5]>=0; n++);
	if(n==0){
		return NULL;
	}
	previousChild = NULL;
	firstChild = NULL;
	for(i=0; i<n; i++){
		child = __nodeMalloc(node->common);

		child->parent = node;
		child->depth = child->parent->depth + 1;
		for(j=0; j<ACTION_SIZE; j++){child->previousAction[j] = actions[i*ACTION_SIZE + j];}

		if(previousChild!=NULL){
			previousChild->sibling = child;
		}
		else{
			firstChild = child;
		}
		previousChild = child;
	}

	return firstChild;
}

static PyObject* initTree(PyObject *self, PyObject *args){
	PyObject *pyState, *pyActions, *pyHistory, *pyPredictor, *predictionInput, *predictionOutput;
	Node *root;
	NodeCommon *common;
	int end, reward, training, i;
	char actions[MAX_AVAILABLE_MOVES * ACTION_SIZE];
	char *dataPath;

	char *stateHistories[1][BOARD_HISTORY];
	char boardModelInput[MAX_AVAILABLE_MOVES * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE];
	char otherModelInput[MAX_AVAILABLE_MOVES * 3 * BOARD_SIZE*BOARD_SIZE];
	Node temp;

	import_array();

	PyArg_ParseTuple(args, "OOiiOOsi", &pyState, &pyActions, &end, &reward, &pyHistory, &pyPredictor, &dataPath, &training);
	for(i=0; i<MAX_AVAILABLE_MOVES * ACTION_SIZE; i++){
		actions[i] = -1;
	}
	for (i=0; i<PyTuple_Size(pyActions); i++){
		__actionFromPy(PyTuple_GetItem(pyActions, i), &(actions[i*ACTION_SIZE]));
	}

	common = malloc(sizeof(NodeCommon));
	
	common->count = 0;
	common->rootStateHistoryLen = PyList_Size(pyHistory);
	__getStateHistoryFromPyHistory(pyHistory, common->rootStateHistory);
	common->predictor = pyPredictor;	Py_XINCREF(pyPredictor);
	common->gameIndex = __lastGameIndex(dataPath) + 1;
	common->training = training;
	common->pyCounter = 1;
	strcpy(common->dataPath, dataPath);
	common->nodeBank = NULL;

	root = __nodeMalloc(common);
	__stateFromPy(pyState, root->state);
	root->end = end?1:0;
	root->reward = reward?-1:0;
	root->depth = PyList_Size(pyHistory);
	root->isTop = 1;
	root->stateSetFlag = 1;
	root->firstChild = __initNodeChildren(root, actions);
	root->actionProbability = 1;


	__getStateHistory(root, BOARD_HISTORY, stateHistories[0]);
	predictionInput = __prepareModelInput(1, stateHistories, boardModelInput, otherModelInput);
	predictionOutput = PyObject_CallFunction(root->common->predictor, "O", predictionInput);
	temp.firstChild = root;
	__setChildrenValuePolicy(&temp, predictionOutput, 0);
	Py_XDECREF(predictionInput);Py_XDECREF(predictionOutput);

	return PyCapsule_New((void*)root, NULL, __freePyTree);
}

static PyObject* searchTree(PyObject *self, PyObject *args){
	PyObject *pyRoot, *output;
	PyObject *pyBestActions, *pyBestAction;
	Node *root, *best, *rootArr[1];
	char bestActions[2*ACTION_SIZE];
	int i;
	time_t t;

	srand((unsigned) time(&t));
	import_array();

	pyRoot = PyTuple_GetItem(args, 0);
	root = (Node*)PyCapsule_GetPointer(pyRoot, NULL);
	rootArr[0] = root;

	printf("\nstart number of nodes %li \n", root->common->count);
	for(i=0; i<NUM_SIMULATIONS; i++){
		__runSimulations(rootArr, 1);
	}
	printf("end number of nodes %li \n", root->common->count);

	__saveNodeInfo(root);
	best = __bestChild(root);
	__removeOtherChildren(root, best);
	root->common->pyCounter++;
	// __displayState(best->state);

	for(i=0; i<ACTION_SIZE;i++){bestActions[i] = best->previousAction[i];}
	for(; i<2*ACTION_SIZE;i++){bestActions[i] = -1;}
	pyBestActions = __actionsToPy(bestActions);
	pyBestAction = PyTuple_GetItem(pyBestActions, 0);
	Py_XINCREF(pyBestAction);
	Py_XDECREF(pyBestActions);

	output = PyTuple_New(2);
	PyTuple_SetItem(output, 0, PyCapsule_New((void*)best, NULL, __freePyTree));
	PyTuple_SetItem(output, 1, pyBestAction);
	return output;
}

static PyObject* generateGames(PyObject *self, PyObject *args){
	PyObject *pyRoots[MAX_CONCURRENT_GAMES];
	Node *roots[MAX_CONCURRENT_GAMES], *bests[MAX_CONCURRENT_GAMES];
	int i, s, flag, games, idxes;
	time_t startTime;
	
	srand((unsigned) time(&startTime));
	games = 0;
	for(i=0; i<MAX_CONCURRENT_GAMES; i++){
		pyRoots[i] = initTree(self, args);
		roots[i] = (Node*)PyCapsule_GetPointer(pyRoots[i], NULL);
		roots[i]->common->gameIndex += i;

		games++;
	}

	printf("----Start Time: %f \n", difftime(time(NULL), startTime));
	while(1){
		for(flag=1, i=0; i<MAX_CONCURRENT_GAMES; i++)
			if(roots[i]->end == 0)
				flag = 0;
		if(flag)break;

		for(s=0; s<NUM_SIMULATIONS; s++){
			__runSimulations(roots, MAX_CONCURRENT_GAMES);
		}

		for(i=0, idxes=0; i<MAX_CONCURRENT_GAMES; i++){
			if(roots[i]->end == 0){
				__saveNodeInfo(roots[i]);
				bests[i] = __bestChild(roots[i]);
				__removeOtherChildren(roots[i], bests[i]);
				roots[i] = bests[i];
				if(roots[i]->end){
					__displayState(roots[i]->state);
					printf("----Idx: %i         Move No: %i           Time: %f \n", i, roots[i]->depth, difftime(time(NULL), startTime));
				}
			}
			if(roots[i]->end && games < NUM_GENERATE_GAMES){
				printf("----Start new game at Idx: %i \n", i);
				Py_XDECREF(pyRoots[i]);

				pyRoots[i] = initTree(self, args);
				roots[i] = (Node*)PyCapsule_GetPointer(pyRoots[i], NULL);
				roots[i]->common->gameIndex += idxes;
				
				idxes++;
				games++;
			}
		}
	}

	for(i=0; i<MAX_CONCURRENT_GAMES; i++){
		Py_XDECREF(pyRoots[i]);
	}

	return PyLong_FromLong(0);
}

static void __freeMem(PyObject *capsule){
	void *ptr;
	ptr = PyCapsule_GetPointer(capsule, NULL);
	free(ptr);
}

static PyObject* allocNpMemory(PyObject *self, PyObject *args){
	char *boardModelInput, *otherModelInput;
	PyObject *pyOutput;

	boardModelInput = malloc(MAX_AVAILABLE_MOVES * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE);
	otherModelInput = malloc(MAX_AVAILABLE_MOVES * 3 * BOARD_SIZE*BOARD_SIZE);

	pyOutput = PyTuple_New(2);
	PyTuple_SetItem(pyOutput, 0, PyCapsule_New((void*)boardModelInput, NULL, __freeMem));
	PyTuple_SetItem(pyOutput, 1, PyCapsule_New((void*)otherModelInput, NULL, __freeMem));
	return pyOutput;
}

static PyObject* prepareModelInput(PyObject *self, PyObject *args){
	PyObject *pCapsules, *pyStateHistories, *pyStateHistory;
	char *boardModelInput, *otherModelInput;
	char *stateHistories[MAX_AVAILABLE_MOVES][BOARD_HISTORY];
	char states[MAX_AVAILABLE_MOVES * BOARD_HISTORY][STATE_SIZE];
	int batchSize, i, j, s, offset, numStates;

	import_array();

	pyStateHistories = PyTuple_GetItem(args, 0);
	pCapsules = PyTuple_GetItem(args, 1);

	boardModelInput = PyCapsule_GetPointer(PyTuple_GetItem(pCapsules, 0), NULL);
	otherModelInput = PyCapsule_GetPointer(PyTuple_GetItem(pCapsules, 1), NULL);

	batchSize = PyTuple_Size(pyStateHistories);
	for(i=0, s=0; i<batchSize; i++){
		pyStateHistory = PyTuple_GetItem(pyStateHistories, i);
		numStates = PyTuple_Size(pyStateHistory);
		for(j=0; j<BOARD_HISTORY; j++){
			stateHistories[i][j] = NULL;
		}
		offset = numStates>BOARD_HISTORY?numStates-BOARD_HISTORY:0;
		for(j=0; j+offset<numStates && j<BOARD_HISTORY; j++, s++){
			__stateFromPy(PyTuple_GetItem(pyStateHistory, j+offset), states[s]);
			stateHistories[i][j] = states[s];
		}
	}

	return __prepareModelInput(batchSize, stateHistories, boardModelInput, otherModelInput);
}

static void __test(PyObject *pyModel){
	PyObject *predictionOutput;
	PyObject *npBoard, *npOther;
	npy_intp dims[4];
	int i;

	char boardModelInput[MAX_AVAILABLE_MOVES * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE];
	char otherModelInput[MAX_AVAILABLE_MOVES * 3 * BOARD_SIZE*BOARD_SIZE];


	for(i=0;i<MAX_AVAILABLE_MOVES * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE;i++)boardModelInput[i]=0;
	for(i=0;i<MAX_AVAILABLE_MOVES * 3*BOARD_SIZE*BOARD_SIZE;i++)otherModelInput[i]=0;
	
	dims[0] = 2;	dims[1] = BOARD_HISTORY*2;	dims[2] = BOARD_SIZE;	dims[3] = BOARD_SIZE;
	npBoard = PyArray_SimpleNewFromData(4, dims, NPY_INT8, boardModelInput);
	dims[1] = 3;
	npOther = PyArray_SimpleNewFromData(4, dims, NPY_INT8, otherModelInput);

	printf("\n-----------------in func2----------------- \n");
	
	predictionOutput = PyObject_CallMethod(pyModel, "predict", "(OO)i", npBoard, npOther, MAX_AVAILABLE_MOVES);
	PyErr_Print();
	printf("\n-----------------in func30----------------- %s \n", PyBytes_AsString(PyUnicode_AsUTF8String((PyObject_Str(PyObject_Type(predictionOutput))))));
	printf("\n-----------------in func31----------------- %s \n", PyBytes_AsString(PyUnicode_AsUTF8String((PyObject_Str(PyObject_GetAttrString(PyList_GetItem(predictionOutput, 1), "shape"))))));
}

static PyObject* test(PyObject *self, PyObject *args){
	PyObject *pyModel;
	import_array();

	pyModel = PyTuple_GetItem(args, 0);
	__test(pyModel);
	return PyLong_FromLong(0);
}



static PyMethodDef csearchMethods[] = {
    {"initTree",  initTree, METH_VARARGS, "create new tree"},
    {"searchTree",  searchTree, METH_VARARGS, "search for next move."},
    {"generateGames",  generateGames, METH_VARARGS, "generate consurrent games."},
    {"allocNpMemory",  allocNpMemory, METH_VARARGS, "allocate memory for numpy arrays."},
    {"prepareModelInput",  prepareModelInput, METH_VARARGS, "prepare model input from histories."},
    {"test",  test, METH_VARARGS, "individual test func."},
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
	PyModule_AddIntConstant(module, "BOARD_HISTORY", BOARD_HISTORY);

    return module;
}