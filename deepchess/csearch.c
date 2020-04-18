#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "ceng_common.c"
#include <numpy/arrayobject.h>
#include <dirent.h>
#include <math.h>
#define MEMORY_BLOCK_SIZE (1<<19)
#define NUM_SIMULATIONS 800
#define MAX_CHILDREN (PREDICTION_BATCH_SIZE-1)
#define PREDICTION_BATCH_SIZE 256
#define BACKPROP_DECAY 0.98
#define MC_EXPLORATION_CONST 0.5
#define STATE_HISTORY_LEN 10
#define BOARD_HISTORY 4


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
static void __runSimulation(Node *node);
static void __saveNodeInfo(Node *node);
static void __removeOtherChildren(Node *node, Node *best);
static void __freeTree(Node *node);
static void __freePyTree(PyObject *pyNode);
static int __lastGameIndex(char *dataPath);
static void __getStateHistoryFromPyHistory(PyObject *pyHistory, char stateHistory[][STATE_SIZE]);
static void __setChildrenValuePolicy(Node *node, PyObject *predictionOutput);
static Node* __initNodeChildren(Node *node, char actions[]);
static void __test(PyObject *pyModel);
static void __freeMem(PyObject *capsule);
static PyObject* initTree(PyObject *self, PyObject *args);
static PyObject* searchTree(PyObject *self, PyObject *args);
static PyObject* test(PyObject *self, PyObject *args);



struct NodeCommon{
	long count;
	char training;
	PyObject *model;
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
	int numActions;
	int depth;
	char stateSetFlag;

	long visits;
	double sqrtVisits;
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

	if(common->nodeBank == NULL){
		common->nodeBank = malloc(sizeof(Node));
		common->nodeBank->sibling = NULL;
		for(i=0; i<MEMORY_BLOCK_SIZE; i++){
			node = malloc(sizeof(Node));
			node->sibling = common->nodeBank;
			common->nodeBank = node;
		}
	}

	node = common->nodeBank;
	common->nodeBank = node->sibling;

	return node;
}

static void __nodeFree(Node *node){
	node->sibling = node->common->nodeBank;
	node->common->nodeBank = node;
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
	int b, h, p, r, c, i, j, historyOffset, modelInputOffset;
	long long idx;
	PyObject *predictionInput;
	PyObject *npBoard, *npOther;
	npy_intp dims[4];

	// initialize used area = 0
	for(i=0; i<batchSize * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE; i++){
		boardModelInput[i] = 0;
	}
	// initialize unused area = -1
	for(; i<PREDICTION_BATCH_SIZE * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE; i++){
		boardModelInput[i] = -1;
	}
	for(b=0; b<batchSize; b++){
		for(j=0; j<BOARD_HISTORY && stateHistories[b][j]!= NULL; j++);
		modelInputOffset = (j>BOARD_HISTORY?0:BOARD_HISTORY-j);
		historyOffset = (j>BOARD_HISTORY?j-BOARD_HISTORY:0);
		for(h=0; h<BOARD_HISTORY && h<j; h++){
			for(p=0; p<2; p++){
				for(r=0; r<BOARD_SIZE; r++){
					for(c=0; c<BOARD_SIZE; c++){
						idx = (((b*BOARD_HISTORY + h+modelInputOffset)*2 + p)*BOARD_SIZE + r)*BOARD_SIZE + c;
						boardModelInput[idx] = __getBoardBox(stateHistories[b][h+historyOffset], p, r, c);
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
	int end, reward;
	int j;

	__getStateHistory(node, STATE_HISTORY_LEN, repeatStateHistory);
	
	for(previousChild=NULL, child=(node->firstChild); child!=NULL; previousChild=child, child=nextChild){
		nextChild = (child->sibling);
		__copyState(node->state, child->state);
		__play(child->state, child->previousAction, child->depth, actions, &end, &reward);
		
		child->stateSetFlag = 1;
		child->end = end>=0?1:0;
		child->reward = reward?-1:0;
		for(j=0; j<MAX_AVAILABLE_MOVES && actions[j*ACTION_SIZE]>=0; j++);
		child->numActions = j;
		child->firstChild = __initNodeChildren(child, actions);

		for(j=0; j<STATE_HISTORY_LEN && repeatStateHistory[j]!=NULL; j++){
			if(__compareState(child->state, repeatStateHistory[j])){
				if(previousChild==NULL){
					node->firstChild = child->sibling;
				}
				else{
					previousChild->sibling = child->sibling;
				}
				__nodeFree(child);
				node->numActions--;
				child = previousChild;
				break;
			}
		}
	}
}

static void __setChildrenHistories(Node *node, char *stateHistories[][BOARD_HISTORY]){
	Node *child;
	char *nodeStateHistory[BOARD_HISTORY];
	int b, i;

	__getStateHistory(node, BOARD_HISTORY, nodeStateHistory);

	for(b=0, child=(node->firstChild); child!=NULL; child=(child->sibling)){
		for(i=0; i<BOARD_HISTORY-1; i++){
			stateHistories[b][i] = nodeStateHistory[i+1];
		}
		stateHistories[b++][i] = child->state;
	}
}

static void __setChildrenValuePolicy(Node *node, PyObject *predictionOutput){
	PyArrayObject *pyValues, *pyPolicies;
	Node *child, *grandChild;
	long idx;
	int b;

	pyValues = PyList_GetItem(predictionOutput, 0);
	pyPolicies = PyList_GetItem(predictionOutput, 1);

	for(b=0, child=(node->firstChild); child!=NULL; child=(child->sibling), b++){
		child->stateValue = *((float*)PyArray_GETPTR2(pyValues, b, 0));
		for(grandChild=(child->firstChild); grandChild!=NULL; grandChild=(grandChild->sibling)){
			idx = __actionIndex(grandChild->previousAction);
			grandChild->actionProbability = *((float*)PyArray_GETPTR2(pyPolicies, b, idx));
		}
	}
}

static double __nodeValue(Node *node, int explore){
	double value;
	value = node->stateValue + node->stateTotalValue;
	if(explore && node->parent!=NULL && node->common->training){
		value += MC_EXPLORATION_CONST * (node->parent->sqrtVisits);
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
		node->sqrtVisits = sqrt((double)(node->visits));
		node->stateTotalValue += leafValue * decay;
	}
}

static void __runSimulation(Node *node){
	char *stateHistories[PREDICTION_BATCH_SIZE][BOARD_HISTORY];
	char boardModelInput[PREDICTION_BATCH_SIZE * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE];
	char otherModelInput[PREDICTION_BATCH_SIZE * 3 * BOARD_SIZE*BOARD_SIZE];
	Node *leaf, *child;
	PyObject *predictionInput, *predictionOutput;
	int batchSize;

	leaf = __getLeaf(node);
	if(leaf->end==0){
		// find batch size
		for(batchSize=0, child=(leaf->firstChild); batchSize<MAX_CHILDREN && child!=NULL; batchSize++, child=(child->sibling));
	
		//fetch history
		__expandChildren(leaf);
		__setChildrenHistories(leaf, stateHistories);

		//prepare model input as numpy
		predictionInput = __prepareModelInput(batchSize, stateHistories, boardModelInput, otherModelInput);
		if(predictionInput==NULL){printf(" ---- prediction input creation error \n");PyErr_Print();}

		//prediction using model
		predictionOutput = PyObject_CallMethod(leaf->common->model, "predict", "O", predictionInput);
		if(predictionOutput==NULL){printf(" ---- prediction error \n");PyErr_Print();}
		
		//set children value policy
		__setChildrenValuePolicy(leaf, predictionOutput);
		Py_XDECREF(predictionInput);Py_XDECREF(predictionOutput);

		// after expansion, select best child of node
		leaf = __bestChild(leaf);
	}

	__backPropogate(leaf);
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
	node->common->count--;
	__nodeFree(node);

	if(common->count == 0){
		for(child=(common->nodeBank); child!=NULL; child=nextChild){
			nextChild = child->sibling;
			free(child);
		}
		free(common);
		printf("\nFreeing tree \n");
	}
}

static void __freePyTree(PyObject *pyNode){
	Node *node;
	node = (Node*)PyCapsule_GetPointer(pyNode, NULL);
	node->common->pyCounter--;
	if(node->common->pyCounter==0){
		while(node->parent != NULL){
			node = node->parent;
		}
		Py_XDECREF(node->common->model);
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
	for(n=0; n<MAX_CHILDREN && actions[n*5]>=0; n++);
	if(n==0){
		return NULL;
	}
	previousChild = NULL;
	firstChild = NULL;
	for(i=0; i<n; i++){
		child = __nodeMalloc(node->common);

		child->parent = node;
		child->common = child->parent->common;
		child->common->count++;
		child->depth = child->parent->depth + 1;
		child->isTop = 0;
		child->stateSetFlag = 0;

		child->end = 0;
		child->reward = 0;
		child->numActions = 0;

		child->visits = 0;
		child->sqrtVisits = 0;
		child->stateTotalValue = 0;
		child->stateValue = 0;
		child->firstChild = NULL;


		for(j=0; j<ACTION_SIZE; j++){
			child->previousAction[j] = actions[i*ACTION_SIZE + j];
		}
		child->actionProbability = 0;

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

static PyObject* initTree(PyObject *self, PyObject *args){
	PyObject *pyState, *pyActions, *pyHistory, *pyModel, *predictionInput, *predictionOutput;
	Node *root;
	NodeCommon *common;
	int end, reward, i;
	char actions[MAX_AVAILABLE_MOVES * ACTION_SIZE];
	char *dataPath;

	char *stateHistories[1][BOARD_HISTORY];
	char boardModelInput[PREDICTION_BATCH_SIZE * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE];
	char otherModelInput[PREDICTION_BATCH_SIZE * 3 * BOARD_SIZE*BOARD_SIZE];
	Node temp;


	import_array();

	PyArg_ParseTuple(args, "OOiiOOs", &pyState, &pyActions, &end, &reward, &pyHistory, &pyModel, &dataPath);
	for(i=0; i<MAX_AVAILABLE_MOVES * ACTION_SIZE; i++){
		actions[i] = -1;
	}
	for (i=0; i<PyTuple_Size(pyActions); i++){
		__actionFromPy(PyTuple_GetItem(pyActions, i), &(actions[i*ACTION_SIZE]));
	}

	common = malloc(sizeof(NodeCommon));
	
	common->count = 1;
	common->rootStateHistoryLen = PyList_Size(pyHistory);
	__getStateHistoryFromPyHistory(pyHistory, common->rootStateHistory);
	common->model = pyModel;	Py_XINCREF(pyModel);
	common->gameIndex = __lastGameIndex(dataPath) + 1;
	common->training = 1;
	common->pyCounter = 1;
	strcpy(common->dataPath, dataPath);
	common->nodeBank = NULL;

	root = __nodeMalloc(common);
	root->common = common;
	__stateFromPy(pyState, root->state);
	root->end = end?1:0;
	root->reward = reward?-1:0;
	root->numActions = PyTuple_Size(pyActions);
	root->parent = NULL;
	root->sibling = NULL;
	root->depth = PyList_Size(pyHistory) + 1;
	root->isTop = 1;
	root->stateSetFlag = 1;
	root->firstChild = __initNodeChildren(root, actions);

	for(i=0; i<ACTION_SIZE; i++){root->previousAction[i] = -1;}
	root->visits = 0;
	root->sqrtVisits = 0;
	root->stateTotalValue = 0;
	root->stateValue = 0;
	root->actionProbability = 1;


	__getStateHistory(root, BOARD_HISTORY, stateHistories[0]);
	predictionInput = __prepareModelInput(1, stateHistories, boardModelInput, otherModelInput);
	predictionOutput = PyObject_CallMethod(root->common->model, "predict", "O", predictionInput);
	temp.firstChild = root;
	__setChildrenValuePolicy(&temp, predictionOutput);
	Py_XDECREF(predictionInput);Py_XDECREF(predictionOutput);

	return PyCapsule_New((void*)root, NULL, __freePyTree);
}

static PyObject* searchTree(PyObject *self, PyObject *args){
	PyObject *pyRoot, *output;
	PyObject *pyBestActions, *pyBestAction;
	Node *root, *best;
	char bestActions[2*ACTION_SIZE];
	int i;
	time_t t;

	srand((unsigned) time(&t));
	import_array();

	pyRoot = PyTuple_GetItem(args, 0);
	root = (Node*)PyCapsule_GetPointer(pyRoot, NULL);

	printf("\nstart number of nodes %li \n", root->common->count);
	for(i=0; i<NUM_SIMULATIONS; i++){
		__runSimulation(root);
	}
	printf("end number of nodes %li \n", root->common->count);

	__saveNodeInfo(root);
	best = __bestChild(root);
	__removeOtherChildren(root, best);
	root->common->pyCounter++;

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

static void __freeMem(PyObject *capsule){
	void *ptr;
	ptr = PyCapsule_GetPointer(capsule, NULL);
	free(ptr);
}

static PyObject* allocNpMemory(PyObject *self, PyObject *args){
	char *boardModelInput, *otherModelInput;
	PyObject *pyOutput;

	boardModelInput = malloc(PREDICTION_BATCH_SIZE * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE);
	otherModelInput = malloc(PREDICTION_BATCH_SIZE * 3 * BOARD_SIZE*BOARD_SIZE);

	pyOutput = PyTuple_New(2);
	PyTuple_SetItem(pyOutput, 0, PyCapsule_New((void*)boardModelInput, NULL, __freeMem));
	PyTuple_SetItem(pyOutput, 1, PyCapsule_New((void*)otherModelInput, NULL, __freeMem));
	return pyOutput;
}

static PyObject* prepareModelInput(PyObject *self, PyObject *args){
	PyObject *pCapsules, *pyStateHistories, *pyStateHistory;
	char *boardModelInput, *otherModelInput;
	char *stateHistories[PREDICTION_BATCH_SIZE][BOARD_HISTORY];
	char states[PREDICTION_BATCH_SIZE * BOARD_HISTORY][STATE_SIZE];
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

	char boardModelInput[PREDICTION_BATCH_SIZE * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE];
	char otherModelInput[PREDICTION_BATCH_SIZE * 3 * BOARD_SIZE*BOARD_SIZE];


	for(i=0;i<PREDICTION_BATCH_SIZE * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE;i++)boardModelInput[i]=0;
	for(i=0;i<PREDICTION_BATCH_SIZE * 3*BOARD_SIZE*BOARD_SIZE;i++)otherModelInput[i]=0;
	
	dims[0] = 2;	dims[1] = BOARD_HISTORY*2;	dims[2] = BOARD_SIZE;	dims[3] = BOARD_SIZE;
	npBoard = PyArray_SimpleNewFromData(4, dims, NPY_INT8, boardModelInput);
	dims[1] = 3;
	npOther = PyArray_SimpleNewFromData(4, dims, NPY_INT8, otherModelInput);

	printf("\n-----------------in func2----------------- \n");
	
	predictionOutput = PyObject_CallMethod(pyModel, "predict", "(OO)i", npBoard, npOther, PREDICTION_BATCH_SIZE);
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