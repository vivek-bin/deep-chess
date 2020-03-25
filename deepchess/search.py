from . import constants as CONST
if CONST.ENGINE_TYPE == "PY":
	from . import engine as EG
elif CONST.ENGINE_TYPE == "C":
	import cengine as EG
else:
	raise ImportError
from . import trainmodel as TM
import math
import statistics
import random
import weakref
import pickle


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
	count = 0
	training = True
	model = None
	gameIndex = 0
	childCountList = []

	def initGlobalCounters(self):
		print("minimum child count :", min(Node.childCountList))
		print("maximum child count :", max(Node.childCountList))
		print("average child count :", statistics.mean(Node.childCountList))
		print("median child count :", statistics.median(Node.childCountList))
		print("std dev child count :", statistics.stdev(Node.childCountList))
		print("number of nodes :", Node.count)
		Node.childCountList = []


	def __init__(self, training=None, model=None, parent=None, previousAction=None, actionProbability=None, state=None, actions=None, end=None, reward=None, stateHistory=None, stateIndexedHistory=None, gameIndex=None):
		assert stateHistory is None or stateIndexedHistory is None		# max one of em

		Node.count += 1
		Node.model = model if model else Node.model
		Node.training = training if training else Node.training
		Node.gameIndex = gameIndex if gameIndex else Node.gameIndex
		Node.childCountList.append(len(actions))
		
		self.stateHistory = stateIndexedHistory if stateIndexedHistory is not None else (tuple((stateIndex(s) for s in stateHistory)) if stateHistory else tuple())

		self.parent = weakref.ref(parent) if parent else lambda:None
		self.previousAction = previousAction
		self.actionProbability = actionProbability
		self.state = stateIndex(state)
		self.player = state["PLAYER"]
		self.actions = {actionIndex(x):0 for x in actions}
		self.end = end
		self.reward = reward

		self.children = []

		self.visits = 0
		self.stateTotalValue = 0

		self.stateValue = None
		self.exploreValue = 0#None

	def __del__(self):
		Node.count -= 1

	def setValuePolicy(self):
		states = self.stateHistory + (self.state,)
		states = [stateFromIndex(s) for s in states]
		modelInput = TM.prepareModelInput([states])

		value, policy = Node.model.predict(modelInput, batch_size=CONST.BATCH_SIZE)

		self.setValue(value[0])
		self.setPolicy(policy[0])

	def setValue(self, value):
		self.stateValue = value
		
	def setPolicy(self, policy):
		for actionIdx in self.actions.keys():
			self.actions[actionIdx] = policy[actionIdx]

	def bestChild(self):
		children = sorted([(child, child.nodeValue()) for child in self.children], key=lambda x:x[1])
		if Node.training:
			minimum = min((childValue for child, childValue in children))
			total = sum((childValue-minimum for child, childValue in children))
			num = ((random.random())**0.5) * total			# weigh upper level more

			while num>=0 and children:
				child, childValue = children.pop()
				num -= childValue-minimum
		else:
			child = children[-1][0]
		return child

	def nodeValue(self):
		x = (self.stateValue + self.stateTotalValue) / (self.visits + 1)
		if Node.training:
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
	
			if Node.training:
				node.exploreValue = self.actionProbability * math.sqrt(node.parent().visits)/(1+node.visits) if node.parent() else 0
	
			node = node.parent() if node.parent() else None
	
	def expandLeaf(self):
		if self.end:
			self.backPropogate()
		else:
			state = stateFromIndex(self.state)
			childStateHistoryIndexed = self.stateHistory + (self.state,)
			childStateHistory = tuple((stateFromIndex(state) for state in childStateHistoryIndexed))
			childList = []
			childStateHistories = []
			for actionIdx, actionProbability in self.actions.items():
				action = actionFromIndex(actionIdx)
				nextState, actions, end, reward = EG.play(state, action, 0)
				child = Node(parent=self, previousAction=action, actionProbability=actionProbability, state=nextState, actions=actions, end=end, reward=reward, stateIndexedHistory=childStateHistoryIndexed)
				
				childList.append(child)
				childStateHistories.append(childStateHistory + (nextState,))

			modelInput = TM.prepareModelInput(childStateHistories)
			value, policy = Node.model.predict(modelInput, batch_size=CONST.BATCH_SIZE)

			for i, child in enumerate(childList):
				child.setValue(value[i])
				child.setPolicy(policy[i])
				self.children.append(child)
			
			bestChildNode = self.bestChild()
			bestChildNode.backPropogate()

	def trimTree(self):
		self.children.sort(key=lambda x:-x.nodeValue())
		self.children = self.children[:10] 			#max(8, len(self.children)//2)]
		for child in self.children:
			child.trimTree()
		

	def saveNodeInfo(self):
		tempDict = {}
		tempDict["STATE"] = stateFromIndex(self.state)
		tempDict["ACTIONS_POLICY"] = self.actions
		tempDict["END"] = self.end
		tempDict["REWARD"] = self.reward
		tempDict["VALUE"] = self.nodeValue()
		tempDict["STATE_HISTORY"] = tuple((stateFromIndex(s) for s in self.stateHistory))[-CONST.BOARD_HISTORY:]
		tempDict["TRAINING"] = Node.training

		tempDict["GAME_NUMBER"] = Node.gameIndex
		tempDict["MOVE_NUMBER"] = len(self.stateHistory) + 1

		fileName = "game_" + str(tempDict["GAME_NUMBER"]).zfill(5) + "_move_" + str(tempDict["MOVE_NUMBER"]).zfill(3)
		
		with open(CONST.DATA + fileName + ".pickle", "wb") as p:
			pickle.dump(tempDict, p)



def initTree(state, actions, end, reward, history, model, gameIndex):
	if not actions:
		return None		# terminal node already
	
	stateHistory = tuple((x["STATE"] for x in history))
	root = Node(training=True, model=model, state=state, actions=actions, end=end, reward=reward, stateHistory=stateHistory, gameIndex=gameIndex)
	root.setValuePolicy()

	return root


def searchTree(root):
	for _ in range(CONST.NUM_SIMULATIONS):
		node = root.getLeaf()
		node.expandLeaf()
	
	root.initGlobalCounters()

	bestChild = root.bestChild()
	bestAction = bestChild.previousAction
	value = root.stateTotalValue / root.visits
	policy = {actionIndex(child.previousAction):(child.visits/root.visits) for child in root.children}

	return bestChild, bestAction, value, policy

