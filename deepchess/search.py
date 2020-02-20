from . import constants as CONST
if CONST.ENGINE_TYPE == "PY":
	from . import engine as EG
elif CONST.ENGINE_TYPE == "C":
	import cengine as EG
else:
	raise ImportError
import math
import random


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
	sIdx = (str(state["EN_PASSANT"][EG.WHITE_IDX]) , str(state["EN_PASSANT"][EG.BLACK_IDX])
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
	def __init__(self, parent=None, previousAction=None, state=None, actions=None, end=None, reward=None, history=None):
		self.parent = parent
		self.previousAction = actionIndex(previousAction)
		self.state = stateIndex(state)
		self.player = state["PLAYER"]
		self.actions = [actionIndex(x) for x in actions]
		self.end = end
		self.reward = reward
		self.history = history

		self.children = []
		self.visits = 0
		self.wins = 0
		self.losses = 0
		self.draws = 0

		self.exploreValue = 0
		self.stateValue = 0

	def getState(self):
		return stateFromIndex(self.state)

	def getPreviousAction(self):
		return actionFromIndex(self.previousAction)

	def getActions(self):
		return [actionFromIndex(x) for x in self.actions]

	def statePolicy(self):
		sortedChildren = sorted([(x.previousAction, x.nodeValue(exploratory=False)) for x in self.children], key=lambda x:-x[1])

		return sortedChildren

	def bestChild(self, exploratory=False):
		best = random.choice(self.children) if self.children else None
		bestValue = best.nodeValue(exploratory)
		for child in self.children:
			childValue = child.nodeValue(exploratory)
			if childValue > bestValue:
				best = child
				bestValue = childValue
		return best

	def nodeValue(self, exploratory):
		return self.stateValue + (CONST.MC_EXPLORATION_CONST * self.exploreValue) if exploratory else 0
	
	def incrementCounters(self, reward):
		self.visits = self.visits + 1

		if reward > 0:
			self.wins = self.wins + 1
		elif reward < 0:
			self.losses = self.losses + 1
		else:
			self.draws = self.draws + 1
		
		self.stateValue = (self.wins if self.player == EG.WHITE_IDX else self.losses)/self.visits if self.visits else 0 #chessModel.evaluate()
		self.exploreValue = math.sqrt(math.log(self.parent.visits)/self.visits) if self.parent and self.parent.visits and self.visits else 0
	

def searchTree(state, actions, end, reward, history):
	if not actions:
		return None		# terminal node already
	
	root = Node(state=state, actions=actions, end=end, reward=reward, history=history)

	for _ in range(CONST.NUM_SIMULATIONS):
		node = getToLeaf(root)
		expandLeaf(node)
	
	bestAction = root.bestChild().getPreviousAction()
	return bestAction, root.stateValue, root.statePolicy()


def expandLeaf(node):
	bestChildNode = node
	reward = node.reward

	if node.actions:
		for action in node.getActions():
			nextState, actions, end, reward = EG.play(node.getState(), action, 0)
			child = Node(parent=node, previousAction=action, state=nextState, actions=actions, end=end, reward=reward)
			node.children.append(child)
		bestChildNode = node.bestChild(exploratory=True)

		nextState, actions, end, reward = EG.playRandomTillEnd(bestChildNode.getState())

	backPropogateNode(bestChildNode, reward)

def getToLeaf(node):
	while node.children:
		node = node.bestChild(exploratory=True)
	
	return node
	
def backPropogateNode(node, reward):
	while(node):
		node.incrementCounters(reward)
		node = node.parent
	
	