from . import constants as CONST
if CONST.ENGINE_TYPE == "PY":
	from . import engine as EG
elif CONST.ENGINE_TYPE == "C":
	import cengine as EG
else:
	raise ImportError
from . import trainmodel as TM
import math
import random
import weakref


def actionIndex(move):
	if move is None:
		return None
	currentPos = move[0]
	newPos = move[1]

	if len(newPos) > 2:			# promotion
		promotion = EG.PROMOTIONS.index(newPos[2])
		newPawnLinearPos = (newPos[0] // (EG.BOARD_SIZE - 1)) * EG.BOARD_SIZE + newPos[1]
		idx = (newPawnLinearPos * EG.BOARD_SIZE + currentPos[1]) * len(EG.PROMOTIONS) + promotion

		idx = idx + 1 + (EG.BOARD_SIZE**4)
	else:
		currentLinearPos = currentPos[0] * EG.BOARD_SIZE + currentPos[1]
		newLinearPos = newPos[0] * EG.BOARD_SIZE + newPos[1]
		idx = newLinearPos * (EG.BOARD_SIZE**2) + currentLinearPos

	return idx
	
def actionFromIndex(idx):
	if idx < (EG.BOARD_SIZE**4):
		currentLinearPos = idx % (EG.BOARD_SIZE * EG.BOARD_SIZE)
		currentPos = (currentLinearPos // EG.BOARD_SIZE, currentLinearPos % EG.BOARD_SIZE)
		
		idx = idx // (EG.BOARD_SIZE**2)
		newLinearPos = idx % (EG.BOARD_SIZE * EG.BOARD_SIZE)
		newPos = (newLinearPos // EG.BOARD_SIZE, newLinearPos % EG.BOARD_SIZE)
	else:
		idx = idx - 1 - (EG.BOARD_SIZE**4)

		promotion = EG.PROMOTIONS[idx % len(EG.PROMOTIONS)]
		idx = idx // len(EG.PROMOTIONS)

		currentPosCol = idx % EG.BOARD_SIZE
		idx = idx // EG.BOARD_SIZE
		newPosCol = idx % EG.BOARD_SIZE
		idx = idx // EG.BOARD_SIZE
		newPosRow = idx * (EG.BOARD_SIZE - 1)
		currentPosRow = abs(newPosRow - 1)

		currentPos = (currentPosRow, currentPosCol)
		newPos = (newPosRow, newPosCol, promotion)

	return (currentPos, newPos)

def stateIndex(state):
	if state is None:
		return None

	idx =  "".join([str(box) for playerBoard in state["BOARD"] for row in playerBoard for box in row])
	sIdx = (str(state["EN_PASSANT"][EG.WHITE_IDX]), str(state["EN_PASSANT"][EG.BLACK_IDX])
		, str(state["CASTLING_AVAILABLE"][EG.WHITE_IDX][EG.LEFT_CASTLE]) 
		, str(state["CASTLING_AVAILABLE"][EG.WHITE_IDX][EG.RIGHT_CASTLE])
		, str(state["CASTLING_AVAILABLE"][EG.BLACK_IDX][EG.LEFT_CASTLE]) 
		, str(state["CASTLING_AVAILABLE"][EG.BLACK_IDX][EG.RIGHT_CASTLE])
		, str(state["PLAYER"]))

	return idx + ",".join(sIdx)

def stateFromIndex(idx):
	state = {}
	boardIdx = [int(x) for x in idx[:2*EG.BOARD_SIZE*EG.BOARD_SIZE]]
	stateIdx = [int(x) for x in idx[2*EG.BOARD_SIZE*EG.BOARD_SIZE:].split(",")]

	state["BOARD"] = []
	for p in range(2):
		state["BOARD"].append([])
		for i in range(EG.BOARD_SIZE):
			rowIdx = p*EG.BOARD_SIZE*EG.BOARD_SIZE + i*EG.BOARD_SIZE
			state["BOARD"][p].append(boardIdx[rowIdx:rowIdx + EG.BOARD_SIZE])

	state["EN_PASSANT"] = {}
	state["EN_PASSANT"][EG.WHITE_IDX] = stateIdx[0]
	state["EN_PASSANT"][EG.BLACK_IDX] = stateIdx[1]

	state["CASTLING_AVAILABLE"] = {EG.WHITE_IDX:{}, EG.BLACK_IDX:{}}
	state["CASTLING_AVAILABLE"][EG.WHITE_IDX][EG.LEFT_CASTLE] = stateIdx[2]
	state["CASTLING_AVAILABLE"][EG.WHITE_IDX][EG.RIGHT_CASTLE] = stateIdx[3]
	state["CASTLING_AVAILABLE"][EG.BLACK_IDX][EG.LEFT_CASTLE] = stateIdx[4]
	state["CASTLING_AVAILABLE"][EG.BLACK_IDX][EG.RIGHT_CASTLE] = stateIdx[5]

	state["PLAYER"] = stateIdx[6]

	return state


class Node():
	def __init__(self, training=None, model=None, parent=None, previousAction=None, actionProbability=None, state=None, actions=None, end=None, reward=None, stateHistory=None):
		self.training = parent.training if parent else training
		self.model = parent.model if parent else model
		self.stateHistory = (parent.stateHistory if parent else stateHistory if stateHistory else tuple()) + (state, )

		self.parent = weakref.ref(parent) if parent else None
		self.previousAction = previousAction
		self.actionProbability = actionProbability
		self.state = state
		self.player = state["PLAYER"]
		self.actions = {actionIndex(x):0 for x in actions}
		self.end = end
		self.reward = reward

		self.children = []

		self.visits = 0
		self.stateTotalValue = 0

		self.stateValue = None
		self.exploreValue = 0#None

	def setValuePolicy(self):
		modelInput = TM.prepareModelInput([self.stateHistory])

		value, policy = self.model.predict(modelInput, batch_size=EG.MAX_POSSIBLE_MOVES)

		self.setValue(value[0])
		self.setPolicy(policy[0])

	def setValue(self, value):
		self.stateValue = value
		
	def setPolicy(self, policy):
		for actionIdx in self.actions.keys():
			self.actions[actionIdx] = policy[actionIdx]

	def setLikeRoot(self):
		self.parent = None
		self.previousAction = None

	def bestChild(self):
		best = random.choice(self.children) if self.children else None
		bestValue = best.nodeValue()
		for child in self.children:
			childValue = child.nodeValue()
			if childValue > bestValue:
				best = child
				bestValue = childValue
		return best

	def nodeValue(self):
		x = (self.stateValue + self.stateTotalValue) / (self.visits + 1)
		if self.training:
			x += CONST.MC_EXPLORATION_CONST * self.exploreValue
		return x
	
	def getLeaf(self):
		node = self
		while node.children:
			node = node.bestChild()
		
		return node

	def backPropogate(self):
		leafValue = self.reward if self.end else self.stateValue

		node = self
		while(node):
			node.visits += 1
			node.stateTotalValue += leafValue * EG.SCORING[node.player]
	
			if self.training:
				node.exploreValue = self.actionProbability * math.sqrt(node.parent().visits)/(1+node.visits) if node.parent else 0
	
			node = node.parent() if node.parent else None
	
	def expandLeaf(self):
		if self.end:
			self.backPropogate()
		else:
			parentStateHistory = self.stateHistory
			parentState = parentStateHistory[-1]
			childList = []
			childStateHistories = []
			for actionIdx, actionProbability in self.actions.items():
				action = actionFromIndex(actionIdx)
				nextState, actions, end, reward = EG.play(parentState, action, 0)
				child = Node(parent=self, previousAction=action, actionProbability=actionProbability, state=nextState, actions=actions, end=end, reward=reward)
				
				childList.append(child)
				childStateHistories.append(parentStateHistory+(nextState,))

			modelInput = TM.prepareModelInput(childStateHistories)
			value, policy = self.model.predict(modelInput, batch_size=EG.MAX_POSSIBLE_MOVES)

			for i, child in enumerate(childList):
				child.setValue(value[i])
				child.setPolicy(policy[i])
				self.children.append(child)
			
			bestChildNode = self.bestChild()
			bestChildNode.backPropogate()



def initTree(state, actions, end, reward, history, model):
	if not actions:
		return None		# terminal node already
	
	stateHistory = tuple((x["STATE"] for x in history))
	root = Node(training=True, model=model, state=state, actions=actions, end=end, reward=reward, stateHistory=stateHistory)
	root.setValuePolicy()

	return root


def searchTree(root):
	for _ in range(CONST.NUM_SIMULATIONS):
		node = root.getLeaf()
		node.expandLeaf()
	
	bestChild = root.bestChild()
	bestAction = bestChild.previousAction
	value = root.stateTotalValue / root.visits
	policy = {actionIndex(child.previousAction):(child.visits/root.visits) for child in root.children}

	bestChild.setLikeRoot()
	return bestChild, bestAction, value, policy

