from . import constants as CONST
if CONST.ENGINE_TYPE == "PY":
	from . import engine as EG
elif CONST.ENGINE_TYPE == "C":
	import cengine as EG
else:
	raise ImportError
import math


def actionIndex(move):
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
	def __init__(self, parent=None, previousAction=None, state=None, actions=None, end=None, reward=None):
		self.parent = parent
		self.previousAction = previousAction
		self.state = state
		self.actions = actions
		self.end = end
		self.reward = reward
		self.children = []
		self.visits = 0
		self.wins = 0
		self.losses = 0
		self.draws = 0

	def bestChild(self):
		best = self.children[0] if self.children else None
		bestValue = best.value()
		for child in self.children:
			childValue = child.value()
			if childValue > bestValue:
				best = child
				bestValue = childValue
		return best

	def value(self):
		explore = math.sqrt(math.log(self.parent.visits)/self.visits) if self.parent else 0

		return self.getNodeStateValue() + CONST.MC_EXPLORATION_CONST * explore

	def getNodeStateValue(self):
		stateValue = self.wins/self.visits #chessModel.evaluate()
		return stateValue
	
	def incrementCounters(self, reward):
		self.visits = self.visits + 1

		if reward > 0:
			self.wins = self.wins + 1
		elif reward < 0:
			self.losses = self.losses + 1
		else:
			self.draws = self.draws + 1
	


def searchTree():
	state, actions, end, reward = EG.init()
	root = Node(state=state, actions=actions, end=end, reward=reward)

	for i in range(CONST.NUM_SIMULATIONS):
		node = getToLeaf(root)
		expandLeaf(node)
	
	return root


def expandLeaf(node):
	bestChildNode = node
	reward = node.reward

	if node.actions:
		for action in node.actions:
			nextState, actions, end, reward = EG.play(node.state, action, 0)
			child = Node(parent=node, previousAction=action, state=nextState, actions=actions, end=end, reward=reward)
			node.children.append(child)
		bestChildNode = node.bestChild()
		nextState, actions, end, reward = EG.playRandomTillEnd(bestChildNode.state)

	backPropogateNode(bestChildNode, reward)

def getToLeaf(node):
	if node.children:
		return getToLeaf(node.bestChild())
	else:
		return node
	
def backPropogateNode(node, reward):
	node.incrementCounters(reward)
	parent = node.parent
	while(parent):
		parent.incrementCounters(reward)
		parent = parent.parent
	