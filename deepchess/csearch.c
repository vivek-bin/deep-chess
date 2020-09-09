#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "ceng_common.c"
#include <numpy/arrayobject.h>
#include <dirent.h>
#include <math.h>
#define VERBOSE 0
#define NODE_BANK_LOT_SIZE (50000)
#define ACTION_BANK_LOT_SIZE (500000)
#define MAX_ALLOCATIONS 100
#define STATE_HISTORY_LEN 10
#define BOARD_HISTORY 4
#define TRIM_DUPLICATES 1
#define GENERATE_WITH_MODEL 1
#define TESTING 1
#if TESTING == 1
#define NUM_SIMULATIONS 1200
#define BACKPROP_DECAY 0.98
#define MAX_CONCURRENT_GAMES 20
#define MC_EXPLORATION_CONST 1
#define NUM_GENERATE_GAMES 0
#define BEST_CHILD_SCALE 5.0			//weighted randomness in move selection; ex, during testing we want low randomness, so higher value/weight
#elif GENERATE_WITH_MODEL == 0
#define NUM_SIMULATIONS 2000
#define BACKPROP_DECAY 0.99
#define MAX_CONCURRENT_GAMES 1
#define MC_EXPLORATION_CONST 0.1
#define NUM_GENERATE_GAMES (MAX_CONCURRENT_GAMES * 400)
#define BEST_CHILD_SCALE 3.0			//weighted randomness in move selection; ex, during testing we want low randomness, so higher value/weight
#else
#define NUM_SIMULATIONS 1000
#define BACKPROP_DECAY 0.98
#define MAX_CONCURRENT_GAMES 4
#define MC_EXPLORATION_CONST 1
#define NUM_GENERATE_GAMES (MAX_CONCURRENT_GAMES * 50)
#define BEST_CHILD_SCALE 3.0			//weighted randomness in move selection; ex, during testing we want low randomness, so higher value/weight
#endif


typedef struct NodeCommon NodeCommon;
typedef struct Node Node;
typedef struct Action Action;
static Node* __nodeMalloc(NodeCommon *common);
static void __nodeFree(Node *node);
static Action* __actionMalloc(NodeCommon *common);
static void __actionFree(Action *action);
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
static void __freeActionTree(Action *action);
static void __freeTree(Node *node);
static void __freePyTree(PyObject *pyNode);
static int __lastGameIndex(char *dataPath);
static void __getStateHistoryFromPyHistory(PyObject *pyHistory, char stateHistory[][STATE_SIZE]);
static void __setChildrenValuePolicy(Node *node, PyObject *predictionOutput, int batchOffset);
static Action* __initNodeChildren(Node *node, char actions[]);
static void __test(PyObject *pyModel);
static void __freeMem(PyObject *capsule);
static Node* __reInitTreeFromNode(Node *node, PyObject *self, PyObject *args);
static PyObject* initTree(PyObject *self, PyObject *args);
static PyObject* searchTree(PyObject *self, PyObject *args);
static PyObject* generateGames(PyObject *self, PyObject *args);
static PyObject* getRootState(PyObject *self, PyObject *args);
static PyObject* test(PyObject *self, PyObject *args);



struct NodeCommon{
	long nodeCount;
	long actionCount;
	char training;
	PyObject *predictor;
	long gameIndex;
	char dataPath[2550];
	char rootStateHistory[STATE_HISTORY_LEN][STATE_SIZE];
	int rootStateHistoryLen;
	int pyCounter;
	long processId;
	Node *nodeBank;
	Action *actionBank;
	Node *nodeArrayBank[MAX_ALLOCATIONS];
	Action *actionArrayBank[MAX_ALLOCATIONS];
};

struct Node{
	NodeCommon *common;

	char state[STATE_SIZE];
	char end;
	int reward;
	int depth;

	long visits;
	double sqrtVisits;
	double stateTotalValue;
	double stateValue;

	Action *previousAction;
	Action *firstAction;

	Node *next;			// for node bank link
};

struct Action{
	Node *parent;
	char action[ACTION_SIZE];
	double actionProbability;
	Node *node;
	Action *sibling;
};


static Node* __nodeMalloc(NodeCommon *common){
	Node *node;
	long i, j;

	// allocate more if bank is empty
	if(common->nodeBank == NULL){
		for(j=0; j<MAX_ALLOCATIONS && common->nodeArrayBank[j]!=NULL; j++);
		common->nodeArrayBank[j] = malloc(sizeof(Node)*NODE_BANK_LOT_SIZE);
		common->nodeBank = &(common->nodeArrayBank[j][0]);
		common->nodeBank->next = NULL;
		for(i=1; i<NODE_BANK_LOT_SIZE; i++){
			node = &(common->nodeArrayBank[j][i]);
			node->next = common->nodeBank;
			common->nodeBank = node;
		}
	}

	// get new node
	node = common->nodeBank;
	common->nodeBank = node->next;
	common->nodeCount++;

	// initialize
	node->common = common;
	node->next = NULL;
	node->previousAction = NULL;
	node->firstAction = NULL;
	node->end = 0;
	node->reward = 0;
	node->depth = 0;

	node->visits = 0;
	node->sqrtVisits = 0;
	node->stateTotalValue = 0;
	node->stateValue = 0;

	return node;
}

static void __nodeFree(Node *node){
	node->next = node->common->nodeBank;
	node->common->nodeBank = node;
	node->common->nodeCount--;
}


static Action* __actionMalloc(NodeCommon *common){
	Action *action;
	long i, j;

	// allocate more if bank is empty
	if(common->actionBank == NULL){
		for(j=0; j<MAX_ALLOCATIONS && common->actionArrayBank[j]!=NULL; j++);
		common->actionArrayBank[j] = malloc(sizeof(Action)*ACTION_BANK_LOT_SIZE);
		common->actionBank = &(common->actionArrayBank[j][0]);
		common->actionBank->sibling = NULL;
		for(i=1; i<ACTION_BANK_LOT_SIZE; i++){
			action = &(common->actionArrayBank[j][i]);
			action->sibling = common->actionBank;
			common->actionBank = action;
		}
	}

	// get new action
	action = common->actionBank;
	common->actionBank = action->sibling;
	common->actionCount++;

	// initialize
	action->parent = NULL;
	for(i=0; i<ACTION_SIZE; i++){action->action[i] = -1;}
	action->actionProbability = 1;
	action->sibling = NULL;
	action->node = NULL;

	return action;
}

static void __actionFree(Action *action){
	action->sibling = action->parent->common->actionBank;
	action->parent->common->actionBank = action;
	action->parent->common->actionCount--;
}

static void __getStateHistory(Node *node, long limit, char *stateHistory[]){
	long i, j;
	for(i=0; i<limit; i++){
		stateHistory[i] = NULL;
	}
	i = limit - 1;
	while(i>=0 && node!=NULL){
		stateHistory[i--] = node->state;
		if(node->previousAction == NULL){
			if(node->common->rootStateHistory != NULL){
				for(j = node->common->rootStateHistoryLen - 1; j>=0 && i>=0; j--){
					stateHistory[i--] = node->common->rootStateHistory[j];
				}
			}
			break;
		}
		node = node->previousAction->parent;
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
	Action *child, *previousChild, *nextChild;

	char *repeatStateHistory[STATE_HISTORY_LEN];
	int endIdx, reward;
	int j;

	__getStateHistory(node, STATE_HISTORY_LEN, repeatStateHistory);
	
	for(previousChild=NULL, child=(node->firstAction); (node->end==0) && child!=NULL; previousChild=child, child=nextChild){
		nextChild = (child->sibling);
		child->node = __nodeMalloc(node->common);

		child->node->previousAction = child;
		child->node->depth = node->depth + 1;
		__copyState(node->state, child->node->state);

		__play(child->node->state, child->action, child->node->depth, actions, &endIdx, &reward);
		
		child->node->end = endIdx>=0?1:0;
		child->node->reward = reward?-1:0;
		child->node->firstAction = __initNodeChildren(child->node, actions);

		// if this child returns to a state already present in its heritage, remove it
		for(j=0; TRIM_DUPLICATES && node->common->training && j<STATE_HISTORY_LEN && repeatStateHistory[j]!=NULL; j++){
			if(__compareState(child->node->state, repeatStateHistory[j])){
				if(previousChild==NULL){
					node->firstAction = nextChild;
				}
				else{
					previousChild->sibling = nextChild;
				}
				__freeActionTree(child);
				child = previousChild;

				// if the repeat node was the only child, it will be an end node, with a draw
				if(node->firstAction == NULL){
					node->end = 1;
				}
				break;
			}
		}
	}
}

static void __setChildrenHistories(Node *node, char *stateHistories[][BOARD_HISTORY]){
	Action *child;
	char *nodeStateHistory[BOARD_HISTORY];
	int b, i, j;

	for(i=0; i<MAX_AVAILABLE_MOVES; i++){
		for(j=0; j<BOARD_HISTORY; j++){
			stateHistories[i][j] = NULL;
		}
	}

	__getStateHistory(node, BOARD_HISTORY, nodeStateHistory);

	for(b=0, child=(node->firstAction); child!=NULL; child=(child->sibling)){
		for(i=0; i<BOARD_HISTORY-1 && nodeStateHistory[i+1]!=NULL; i++){
			stateHistories[b][i] = nodeStateHistory[i+1];
		}
		stateHistories[b++][i] = child->node->state;
	}
}

static void __setChildrenValuePolicy(Node *node, PyObject *predictionOutput, int batchOffset){
	PyArrayObject *pyValues, *pyPolicies;
	Action *child, *grandChild;
	long idx;
	int b;
	double policyTotal;

	pyValues = (PyArrayObject*) PyList_GetItem(predictionOutput, 0);
	pyPolicies = (PyArrayObject*) PyList_GetItem(predictionOutput, 1);

	for(b=0, child=(node->firstAction); child!=NULL; child=(child->sibling), b++){
		child->node->stateValue = *((float*)PyArray_GETPTR2(pyValues, batchOffset+b, 0));
		for(policyTotal=0, grandChild=(child->node->firstAction); grandChild!=NULL; grandChild=(grandChild->sibling)){
			idx = __actionIndex(grandChild->action);
			grandChild->actionProbability = *((float*)PyArray_GETPTR2(pyPolicies, batchOffset+b, idx));
			policyTotal += grandChild->actionProbability;
		}
		for(grandChild=(child->node->firstAction); grandChild!=NULL; grandChild=(grandChild->sibling)){
			grandChild->actionProbability /= policyTotal;
		}
	}
}

static double __nodeValue(Node *node, int explore){
	double value;
	value = node->stateValue + node->stateTotalValue;
	if(explore && node->previousAction!=NULL){
		value -= MC_EXPLORATION_CONST * (node->previousAction->actionProbability) * (node->previousAction->parent->sqrtVisits);
	}
	return value/(node->visits + 1);
}

static Node* __bestChild(Node *node){
	double totalValue, minimumValue, childRnd;
	Action *child;
	int count, i, bestIdx;

	double values[MAX_AVAILABLE_MOVES];
	Node *children[MAX_AVAILABLE_MOVES];

	for(count=0, child=(node->firstAction); count<MAX_AVAILABLE_MOVES && child!=NULL; count++, child=(child->sibling)){
		values[count] = __nodeValue(child->node, 1) * -1;
		children[count] = child->node;
	}

	// if we have a move which ends the game with a win(!draw), we always take it
	for(i=0; i<count; i++){
		if((children[i]->end) && (children[i]->reward)){
			return children[i];
		}
	}
	
	if(node->common->training){
		// find minimum value of children
		for(minimumValue=999999999, i=0; i<count; i++){
			if(values[i]<minimumValue){
				minimumValue = values[i];
			}
		}

		// move to range [0, max) and scale to weigh better values more
		for(totalValue=0, i=0; i<count; i++){
			values[i] = pow(values[i] - minimumValue + 1e-5, BEST_CHILD_SCALE);
			totalValue += values[i];
		}
		for(i=0; i<count; i++){
			values[i] /= totalValue;
		}
		childRnd = ((double)(rand()))/((double)RAND_MAX);

		for(bestIdx=count-1, totalValue=0, i=0; i<count; i++){
			totalValue += values[i];
			if(totalValue > childRnd){
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
	while(node->end==0 && node->firstAction->node!=NULL){
		node = __bestChild(node);
	}

	return node;
}

static void __backPropogate(Node *node){
	double decay, leafValue, flippingDecay;

	flippingDecay = BACKPROP_DECAY * -1;
	leafValue = node->end?node->reward:node->stateValue;

	for(decay=1; node!=NULL; decay*=flippingDecay, node=(node->previousAction->parent)){
		node->visits++;
		node->sqrtVisits = sqrt(((double)(node->visits)));
		node->stateTotalValue += leafValue * decay;
		if(node->previousAction == NULL){
			break;
		}
	}
}

static void __runSimulations(Node *roots[], int numConc){
	char *stateHistories[MAX_CONCURRENT_GAMES * MAX_AVAILABLE_MOVES][BOARD_HISTORY];
	char boardModelInput[MAX_CONCURRENT_GAMES*MAX_AVAILABLE_MOVES * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE];
	char otherModelInput[MAX_CONCURRENT_GAMES*MAX_AVAILABLE_MOVES * 3 * BOARD_SIZE*BOARD_SIZE];
	Node *leafs[MAX_CONCURRENT_GAMES];
	Action *child;
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
			if(VERBOSE){printf("Travelling to leaf for root#%i\n", i);}
			leafs[i] = __getLeaf(roots[i]);
			__expandChildren(leafs[i]);
		}
	}

	for(i=0; GENERATE_WITH_MODEL && i<numConc; i++){
		if(leafs[i] != NULL && leafs[i]->end == 0){
			if(VERBOSE){printf("Setting history for root#%i\n", i);}
			//find batch sizes
			for(batchSizes[i]=0, child=(leafs[i]->firstAction); batchSizes[i]<MAX_AVAILABLE_MOVES && child!=NULL; batchSizes[i]++, child=(child->sibling));

			// fetch histories
			__setChildrenHistories(leafs[i], &(stateHistories[totalBatchSize]));

			batchOffset[i] = totalBatchSize;
			totalBatchSize += batchSizes[i];
		}
	}
	if(GENERATE_WITH_MODEL && totalBatchSize > 0){
		//prepare model input as numpy
		if(VERBOSE){printf("Preparing model input\n");}
		predictionInput = __prepareModelInput(totalBatchSize, stateHistories, boardModelInput, otherModelInput);
		if(predictionInput==NULL){printf(" ---- prediction input creation error \n");PyErr_Print();}

		//prediction using model
		if(VERBOSE){printf("Querying model\n");}
		predictionOutput = PyObject_CallFunction(roots[0]->common->predictor, "O", predictionInput);
		if(predictionOutput==NULL){printf(" ---- prediction error \n");PyErr_Print();}
		
		for(i=0; i<numConc; i++){
			if(batchSizes[i] > 0){
				//set children value policy
				if(VERBOSE){printf("Setting value and policy for root#%i\n", i);}
				__setChildrenValuePolicy(leafs[i], predictionOutput, batchOffset[i]);
			}
		}
		Py_XDECREF(predictionInput);Py_XDECREF(predictionOutput);
	}

	// after expansion, select best child of node
	for(i=0; i<numConc; i++){
		if(leafs[i] != NULL && leafs[i]->end == 0){
			if(VERBOSE){printf("Select best child for backpropogation for root#%i\n", i);}
			leafs[i] = __bestChild(leafs[i]);
		}
	}
	
	// backpropogate all active game trees
	for(i=0; i<numConc; i++){
		if(leafs[i] != NULL){
			if(VERBOSE){printf("Backpropogating root#%i\n", i);}
			__backPropogate(leafs[i]);
		}
	}
}

static void __saveNodeInfo(Node *node){
	FILE * oFile;
	Action *child;
	char stateIdx[STATE_SIZE+1];
	char fileName[2650];
	char *stateHistory[STATE_HISTORY_LEN];
	int i;

	sprintf(fileName, "%sgame_%05ld_move_%03d_process_%04ld.json", node->common->dataPath, node->common->gameIndex, node->depth, (node->common->processId%10000));
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
	for(child=(node->firstAction); child!=NULL; child=(child->sibling)){
		fprintf(oFile, "    \"%i\" : %.10f ", __actionIndex(child->action), child->actionProbability);
		if(child->sibling != NULL){	fprintf(oFile, ",");}
		fprintf(oFile, "\n");
	}
	fprintf(oFile, "                     }, \n");
	fprintf(oFile, "  \"SEARCHED_POLICY\" : { \n");
	for(child=(node->firstAction); child!=NULL; child=(child->sibling)){
		fprintf(oFile, "    \"%i\" : %.10f ", __actionIndex(child->action), ((float)(child->node->visits)/(float)(node->visits)));
		if(child->sibling != NULL){	fprintf(oFile, ",");}
		fprintf(oFile, "\n");
	}
	fprintf(oFile, "                     }, \n");

	fprintf(oFile, "  \"END\" : %i, \n", node->end);
	fprintf(oFile, "  \"REWARD\" : %i, \n", node->reward);
	fprintf(oFile, "  \"STATE_VALUE\" : %.10f, \n", node->stateValue);
	fprintf(oFile, "  \"VALUE\" : %.10f, \n", __nodeValue(node, 0));
	fprintf(oFile, "  \"EXPLORATORY_VALUE\" : %.10f, \n", __nodeValue(node, 1));
	fprintf(oFile, "  \"TRAINING\" : %i, \n", node->common->training);

	fprintf(oFile, "  \"GAME_NUMBER\" : %li, \n", node->common->gameIndex);
	fprintf(oFile, "  \"MOVE_NUMBER\" : %i \n", node->depth);
	fprintf(oFile, "}");
	
	fclose (oFile);
}

static void __removeOtherChildren(Node *node, Node *best){
	Action *child, *nextChild;

	for(child=(node->firstAction); child!=NULL; child=nextChild){
		nextChild = child->sibling;
		if(child->node != best){
			__freeActionTree(child);
		}
	}
	best->previousAction->sibling = NULL;
	node->firstAction = best->previousAction;
}

static void __freeActionTree(Action *action){
	Node *temp;

	temp = __nodeMalloc(action->parent->common);
	temp->firstAction = action;
	action->sibling = NULL;
	action->parent = temp;

	__freeTree(temp);
}

static void __freeTree(Node *node){
	Action *child, *nextChild;
	NodeCommon *common;
	int j;

	common = node->common;
	
	for(child=(node->firstAction); child!=NULL; child=nextChild){
		nextChild = child->sibling;
		if(child->node != NULL){
			__freeTree(child->node);
		}

		__actionFree(child);
	}
	__nodeFree(node);

	if(common->nodeCount == 0){
		if(VERBOSE)printf("Freeing tree memory \n");
		for(j=0; j<MAX_ALLOCATIONS && common->nodeArrayBank[j]!=NULL; j++){
			free(common->nodeArrayBank[j]);
		}
		for(j=0; j<MAX_ALLOCATIONS && common->actionArrayBank[j]!=NULL; j++){
			free(common->actionArrayBank[j]);
		}
		free(common);
	}
}

static void __freePyTree(PyObject *pyNode){
	Node *node;
	node = (Node*)PyCapsule_GetPointer(pyNode, NULL);
	node->common->pyCounter--;
	if(node->common->pyCounter==0){
		if(VERBOSE)printf("Freeing py tree \n");
		while(node->previousAction != NULL){
			node = node->previousAction->parent;
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

static Action* __initNodeChildren(Node *node, char actions[]){
	Action *child, *previousChild, *firstChild;
	int i, j, n;
	for(n=0; n<MAX_AVAILABLE_MOVES && actions[n*5]>=0; n++);
	if(n==0){
		return NULL;
	}
	previousChild = NULL;
	firstChild = NULL;
	for(i=0; i<n; i++){
		child = __actionMalloc(node->common);

		child->parent = node;
		for(j=0; j<ACTION_SIZE; j++){child->action[j] = actions[i*ACTION_SIZE + j];}

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
	PyObject *pyState, *pyActions, *pyHistory, *pyPredictor;
	Node *root;
	NodeCommon *common;
	int end, reward, training;
	char *dataPath;
	int i;

	import_array();

	PyArg_ParseTuple(args, "OOiiOOsi", &pyState, &pyActions, &end, &reward, &pyHistory, &pyPredictor, &dataPath, &training);

	common = malloc(sizeof(NodeCommon));
	
	common->nodeCount = 0;
	common->actionCount = 0;
	common->rootStateHistoryLen = PyList_Size(pyHistory);
	__getStateHistoryFromPyHistory(pyHistory, common->rootStateHistory);
	common->predictor = pyPredictor;	Py_XINCREF(pyPredictor);
	common->gameIndex = __lastGameIndex(dataPath) + 1;
	common->training = training;
	common->pyCounter = 1;
	common->processId = (long)getpid();
	strcpy(common->dataPath, dataPath);
	common->nodeBank = NULL;
	common->actionBank = NULL;
	for(i=0; i<MAX_ALLOCATIONS; i++){common->nodeArrayBank[i] = NULL;}
	for(i=0; i<MAX_ALLOCATIONS; i++){common->actionArrayBank[i] = NULL;}

	root = __reInitTreeFromNode(__nodeMalloc(common), self, args);

	return PyCapsule_New((void*)root, NULL, __freePyTree);
}

static Node* __reInitTreeFromNode(Node *node, PyObject *self, PyObject *args){
	PyObject *pyState, *pyActions, *pyHistory, *pyPredictor, *predictionInput, *predictionOutput;
	Node *root, *temp;
	NodeCommon *common;
	int end, reward, training, i;
	char actions[MAX_AVAILABLE_MOVES * ACTION_SIZE];
	char *dataPath;

	char *stateHistories[1][BOARD_HISTORY];
	char boardModelInput[MAX_AVAILABLE_MOVES * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE];
	char otherModelInput[MAX_AVAILABLE_MOVES * 3 * BOARD_SIZE*BOARD_SIZE];

	PyArg_ParseTuple(args, "OOiiOOsi", &pyState, &pyActions, &end, &reward, &pyHistory, &pyPredictor, &dataPath, &training);
	for(i=0; i<MAX_AVAILABLE_MOVES * ACTION_SIZE; i++){
		actions[i] = -1;
	}
	for (i=0; i<PyTuple_Size(pyActions); i++){
		__actionFromPy(PyTuple_GetItem(pyActions, i), &(actions[i*ACTION_SIZE]));
	}

	common = node->common;

	root = __nodeMalloc(common);
	__stateFromPy(pyState, root->state);
	root->end = end?1:0;
	root->reward = reward?-1:0;
	root->depth = common->rootStateHistoryLen;
	root->firstAction = __initNodeChildren(root, actions);

	// free old node's tree entirely
	while(node->previousAction != NULL){
		node = node->previousAction->parent;
	}
	__freeTree(node);
	if(VERBOSE)printf("New roots' node count: %li          action count: %li \n", root->common->nodeCount, root->common->actionCount);

	// set value and policy of root
	if(GENERATE_WITH_MODEL){
		__getStateHistory(root, BOARD_HISTORY, stateHistories[0]);
		predictionInput = __prepareModelInput(1, stateHistories, boardModelInput, otherModelInput);
		predictionOutput = PyObject_CallFunction(root->common->predictor, "O", predictionInput);
		temp = __nodeMalloc(common);
		temp->firstAction = __actionMalloc(common);
		temp->firstAction->parent = temp;
		temp->firstAction->node = root;
		__setChildrenValuePolicy(temp, predictionOutput, 0);
		__actionFree(temp->firstAction);
		__nodeFree(temp);
		Py_XDECREF(predictionInput);Py_XDECREF(predictionOutput);
	}

	return root;
}

static PyObject* searchTree(PyObject *self, PyObject *args){
	PyObject *pyRoots, *pyOutput, *pyOutputs;
	PyObject *pyBestActions, *pyBestAction;
	Node *roots[MAX_CONCURRENT_GAMES], *best;
	char bestActions[2*ACTION_SIZE];
	int i, j, numRoots;
	time_t t;

	srand((unsigned) time(&t));
	import_array();

	pyRoots = PyTuple_GetItem(args, 0);
	if(PyList_Check(pyRoots)){
		numRoots = PyList_Size(pyRoots);
		for(i=0; i<numRoots; i++){
			roots[i] = (Node*)PyCapsule_GetPointer(PyList_GetItem(pyRoots, i), NULL);
			if(VERBOSE){
				printf("\n\n    Game Number: %i\n", i);
				__displayState(roots[i]->state);
			}
		}
	}
	else{
		numRoots = 1;
		roots[0] = (Node*)PyCapsule_GetPointer(pyRoots, NULL);
	}

	if(VERBOSE)printf("\nstart number of nodes %li \n", roots[0]->common->nodeCount);
	for(i=0; i<NUM_SIMULATIONS; i++){
		__runSimulations(roots, numRoots);
	}
	if(VERBOSE)printf("end number of nodes %li \n", roots[0]->common->nodeCount);

	for(pyOutputs=NULL, i=0; i<numRoots; i++){
		if(roots[i]->end == 0){
			if(VERBOSE)printf("Saving current root and moving to best child for root#%i \n", i);
			__saveNodeInfo(roots[i]);
			best = __bestChild(roots[i]);
			__removeOtherChildren(roots[i], best);

			for(j=0; j<ACTION_SIZE;j++){bestActions[j] = best->previousAction->action[j];}
			for(; j<2*ACTION_SIZE;j++){bestActions[j] = -1;}
			pyBestActions = __actionsToPy(bestActions);
			pyBestAction = PyTuple_GetItem(pyBestActions, 0);
			Py_XINCREF(pyBestAction);
			Py_XDECREF(pyBestActions);

			roots[i]->common->pyCounter++;
			pyOutput = PyTuple_New(2);
			PyTuple_SetItem(pyOutput, 0, PyCapsule_New((void*)best, NULL, __freePyTree));
			PyTuple_SetItem(pyOutput, 1, pyBestAction);
		}
		else{
			if(VERBOSE)printf("Return same because tree has ended, for root#%i \n", i);
			roots[i]->common->pyCounter++;
			pyOutput = PyTuple_New(2);
			PyTuple_SetItem(pyOutput, 0, PyCapsule_New((void*)roots[i], NULL, __freePyTree));
			PyTuple_SetItem(pyOutput, 1, PyTuple_New(0));
		}

		if(PyList_Check(pyRoots)){
			if(i == 0){
				pyOutputs = PyList_New(numRoots);
			}
			PyList_SetItem(pyOutputs, i, pyOutput);
		}
		else{
			pyOutputs = pyOutput;
		}
	}

	if(VERBOSE)printf("Returning from search \n");
	return pyOutputs;
}

static PyObject* playMoveOnTree(PyObject *self, PyObject *args){
	PyObject *pyRoot, *pyOutput;
	PyObject *pyAction;
	Node *root, *nextRoot, *rootArr[1];
	Action *actionNode;
	char action[ACTION_SIZE];
	int i;

	pyRoot = PyTuple_GetItem(args, 0);
	pyAction = PyTuple_GetItem(args, 1);
	root = (Node*)PyCapsule_GetPointer(pyRoot, NULL);
	__actionFromPy(pyAction, action);
		
	if(root->firstAction->node == NULL){
		rootArr[0] = root;
		__runSimulations(rootArr, 1);
	}
	for(nextRoot=NULL, actionNode=root->firstAction; actionNode!=NULL; actionNode=actionNode->sibling){
		for(i=0; i<ACTION_SIZE && actionNode->action[i]==action[i]; i++);
		if(i == ACTION_SIZE){
			nextRoot = actionNode->node;
			break;
		}
	}
	if(nextRoot == NULL){printf("action not found! \n");}
	
	__removeOtherChildren(root, nextRoot);
	nextRoot->common->pyCounter++;
	pyOutput = PyCapsule_New((void*)nextRoot, NULL, __freePyTree);

	return pyOutput;
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
					printf("Game ended at        Idx: %i         Move No: %i           Time: %f \n", i, roots[i]->depth, difftime(time(NULL), startTime));
				}
			}
			if(roots[i]->end && games < NUM_GENERATE_GAMES){
				roots[i] = __reInitTreeFromNode(roots[i], self, args);
				roots[i]->common->gameIndex = __lastGameIndex(roots[i]->common->dataPath) + 1 + idxes;
				PyCapsule_SetPointer(pyRoots[i], (void*)roots[i]);
				
				printf("Start new game number: %i          at Idx: %i \n\n\n", games, i);
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

	boardModelInput = malloc(4096 * BOARD_HISTORY * 2*BOARD_SIZE*BOARD_SIZE);
	otherModelInput = malloc(4096 * 3 * BOARD_SIZE*BOARD_SIZE);

	pyOutput = PyTuple_New(2);
	PyTuple_SetItem(pyOutput, 0, PyCapsule_New((void*)boardModelInput, NULL, __freeMem));
	PyTuple_SetItem(pyOutput, 1, PyCapsule_New((void*)otherModelInput, NULL, __freeMem));
	return pyOutput;
}

static PyObject* prepareModelInput(PyObject *self, PyObject *args){
	PyObject *pCapsules, *pyStateHistories, *pyStateHistory;
	char *boardModelInput, *otherModelInput;
	char *stateHistories[4096][BOARD_HISTORY];
	char states[4096 * BOARD_HISTORY][STATE_SIZE];
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

static PyObject* getRootState(PyObject *self, PyObject *args){
	PyObject *pyRoot, *pyState;
	Node *root;

	pyRoot = PyTuple_GetItem(args, 0);
	root = (Node*)PyCapsule_GetPointer(pyRoot, NULL);
	
	pyState = __stateToPy(root->state);
	return pyState;
}

static void __test(PyObject *predictionOutput){
	PyArrayObject *pyValues, *pyPolicies;
	long idx, b;

	pyValues = (PyArrayObject*) PyList_GetItem(predictionOutput, 0);
	pyPolicies = (PyArrayObject*) PyList_GetItem(predictionOutput, 1);
	printf("jjgjggj %li %li %li %li\n", PyArray_DIM(pyValues, 0), PyArray_DIM(pyValues, 1), PyArray_DIM(pyPolicies, 0), PyArray_DIM(pyPolicies, 1));
	for(b=0; b<5; b++){
		printf("\n\n batch:%li \n", b);
		printf("     v: %f \n", *((float*)PyArray_GETPTR2(pyValues, b, 0)));
		for(idx=0; idx<3; idx+=1){
			printf("     p%li: %f \n", idx, *((float*)PyArray_GETPTR2(pyPolicies, b, idx)));
		}
	}
}

static PyObject* test(PyObject *self, PyObject *args){
	import_array();

	__test(PyTuple_GetItem(args, 0));
	return PyLong_FromLong(0);
}



static PyMethodDef csearchMethods[] = {
    {"initTree",  initTree, METH_VARARGS, "create new tree"},
    {"searchTree",  searchTree, METH_VARARGS, "search for next move."},
    {"playMoveOnTree",  playMoveOnTree, METH_VARARGS, "play a move on an existing tree, to move one node down."},
    {"generateGames",  generateGames, METH_VARARGS, "generate consurrent games."},
    {"allocNpMemory",  allocNpMemory, METH_VARARGS, "allocate memory for numpy arrays."},
    {"prepareModelInput",  prepareModelInput, METH_VARARGS, "prepare model input from histories."},
    {"getRootState",  getRootState, METH_VARARGS, "print the state of current root node."},
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