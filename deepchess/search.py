from . import constants as CONST
if CONST.ENGINE_TYPE == "PY":
	from . import engine as EG
elif CONST.ENGINE_TYPE == "C":
	import cengine as EG
else:
	raise ImportError
import math
import random
import json
import os
import numpy as np

NUM_SIMULATIONS = 500
BACKPROP_DECAY = 0.98
MC_EXPLORATION_CONST = 0.5
STATE_HISTORY_LEN = 10
BOARD_HISTORY = 4


class Node():
	count = 0
	training = True
	predictor = None
	gameIndex = 0
	lowStateMemoryUse = True
	dataPath = None

	def __init__(self, training=None, predictor=None, parent=None, previousAction=None, actionProbability=None, state=None, actions=None, end=None, reward=None, stateHistory=None, gameIndex=None, dataPath=None):
		assert (stateHistory is None) ^ (parent is None)		# one or the other

		Node.count += 1
		Node.predictor = predictor if predictor else Node.predictor
		Node.training = training if training else Node.training
		Node.gameIndex = gameIndex if gameIndex else Node.gameIndex
		Node.dataPath = dataPath if dataPath else Node.dataPath

		self.stateHistory = stateHistory
		self.parent = parent
		self.previousAction = previousAction
		self.actionProbability = actionProbability
		self.state = EG.stateIndex(state) if Node.lowStateMemoryUse else state
		self.actions = {x:0 for x in actions}
		self.end = end
		self.reward = -1 if reward else 0 		# take players perspective; if game ends with current player => not a draw and player had no moves => player lost

		self.children = []

		self.visits = 0
		self.stateTotalValue = 0
		self.stateValue = None
		self.exploreValue = math.sqrt(parent.visits) if parent is not None and Node.training else 1

	def __del__(self):
		Node.count -= 1

	def getStateHistory(self, limit=STATE_HISTORY_LEN, indexed=False):
		stateHistory = []
		node = self
		while len(stateHistory) < limit:
			if Node.lowStateMemoryUse:
				state = EG.stateFromIndex(node.state) if not indexed else node.state
			else:
				state = EG.stateIndex(node.state) if indexed else node.state
			stateHistory.insert(0, state)
			if node.parent is None:
				left = limit - len(stateHistory)
				if indexed:
					stateHistory = [EG.stateIndex(state) for state in node.stateHistory[-left:]] + stateHistory
				else:
					stateHistory = list(node.stateHistory[-left:]) + stateHistory
				break
			
			node = node.parent

		return tuple(stateHistory)

	def getModelPrediction(self, data):
		modelInput = prepareModelInput(data, False)
		return Node.predictor(modelInput)

	def setValuePolicy(self):
		value, policy = self.getModelPrediction([self.getStateHistory()])

		self.setValue(value[0])
		self.setPolicy(policy[0])

	def setValue(self, value):
		self.stateValue = float(value)
		
	def setPolicy(self, policy):
		for action in self.actions.keys():
			self.actions[action] = float(policy[EG.actionIndex(action)])

	def bestChild(self):
		children = sorted([(child, child.nodeValue()) for child in self.children], key=lambda x:x[1])
		if Node.training:
			minimum = min((childValue for child, childValue in children))
			children = [(child, (childValue-minimum)**2) for child,childValue in children] 		# weigh upper level more
			total = sum((childValue for child, childValue in children))
			num = total * 0.5 #(random.random())

			while num>=0 and children:
				child, childValue = children.pop()
				num -= childValue
		else:
			child = children[-1][0]
		return child

	def nodeValue(self, explore=True):
		x = (self.stateValue + self.stateTotalValue) / (self.visits + 1)
		if Node.training and explore:
			x += MC_EXPLORATION_CONST * self.actionProbability * self.exploreValue
		return x
	
	def getLeaf(self):
		node = self
		while node.children:
			node = node.bestChild()
		
		return node

	def backPropogate(self):
		decay = 1.0
		leafValue = self.reward if self.end else self.stateValue
		leafOppValue = -leafValue
		node = self

		while node is not None:
			node.visits += 1
			node.stateTotalValue += leafValue * decay
			node.exploreValue = math.sqrt(node.parent.visits)/(1+node.visits) if node.parent is not None and Node.training else 1

			decay *= BACKPROP_DECAY
			leafValue, leafOppValue = leafOppValue, leafValue
			node = node.parent
	
	def expandLeaf(self):
		if self.end:
			self.backPropogate()
		else:
			state = EG.stateFromIndex(self.state) if Node.lowStateMemoryUse else self.state
			childStateHistory = self.getStateHistory()
			repeatHistory = self.getStateHistory(indexed=True)
			childList = []
			childStateHistories = []
			for action, actionProbability in self.actions.items():
				nextState, actions, end, reward = EG.play(state, action, 0)
				if EG.stateIndex(nextState) not in repeatHistory:
					child = Node(parent=self, previousAction=action, actionProbability=actionProbability, state=nextState, actions=actions, end=end, reward=reward)
					
					childList.append(child)
					childStateHistories.append(childStateHistory + (nextState,))

			value, policy = self.getModelPrediction(childStateHistories)

			for i, child in enumerate(childList):
				child.setValue(value[i])
				child.setPolicy(policy[i])
				self.children.append(child)
			
			bestChildNode = self.bestChild()
			bestChildNode.backPropogate()

	def trimTree(self):
		self.children.sort(key=lambda x:-x.nodeValue())
		self.children = self.children[:max(10, len(self.children)//2)]
		for child in self.children:
			child.trimTree()
		
	def runSimulations(self):
		if Node.count>150000:
			print("------------- Before trim node count :", Node.count)
			self.trimTree()
			print("------------- After  trim node count :", Node.count)

		for _ in range(NUM_SIMULATIONS):
			self.getLeaf().expandLeaf()

	def saveNodeInfo(self):
		tempDict = {}
		tempDict["STATE"] = self.state if Node.lowStateMemoryUse else EG.stateIndex(self.state)
		tempDict["ACTIONS_POLICY"] = {str(EG.actionIndex(k)):v for k,v in self.actions.items()}
		tempDict["SEARCHED_POLICY"] = {str(EG.actionIndex(child.previousAction)):(child.visits/self.visits) for child in self.children}
		tempDict["STATE_HISTORY"] = self.getStateHistory(indexed=True)

		tempDict["END"] = self.end
		tempDict["REWARD"] = self.reward
		tempDict["STATE_VALUE"] = self.stateValue
		tempDict["VALUE"] = self.nodeValue(explore=False)
		tempDict["EXPLORATORY_VALUE"] = self.nodeValue()
		tempDict["TRAINING"] = int(Node.training)

		tempDict["GAME_NUMBER"] = Node.gameIndex
		tempDict["MOVE_NUMBER"] = len(self.getStateHistory(9999)) - 1

		fileName = "game_" + str(tempDict["GAME_NUMBER"]).zfill(5) + "_move_" + str(tempDict["MOVE_NUMBER"]).zfill(3)
		
		with open(Node.dataPath + fileName + ".json", "w") as p:
			json.dump(tempDict, p)



def initTree(state, actions, end, reward, history, predictor, dataPath, training):
	if not actions:
		return None		# terminal node already

	fileNames = [x for x in os.listdir(dataPath) if x.startswith("game_") and x.endswith(".json")]
	gameIndices = [int(x.split("_")[1]) for x in fileNames]
	gameIndex = max(gameIndices + [-1]) + 1
	
	stateHistory = tuple((x["STATE"] for x in history))
	root = Node(training=training, predictor=predictor, actionProbability=1, state=state, actions=actions, end=end, reward=reward, stateHistory=stateHistory, gameIndex=gameIndex, dataPath=dataPath)
	root.setValuePolicy()

	return root


def searchTree(root):
	print("start number of nodes :", Node.count)

	root.runSimulations()

	print("end number of nodes :", Node.count)
	root.saveNodeInfo()

	bestChild = root.bestChild()
	bestAction = bestChild.previousAction
	root.children = [bestChild]			# keep only played move history

	return bestChild, bestAction


def allocNpMemory():
	return False


def prepareModelInput(stateHistories, __memObj):		#mem object for compatibility with C module
	batchSize = len(stateHistories)
	boardInput = np.ones((batchSize, BOARD_HISTORY, 2, EG.BOARD_SIZE, EG.BOARD_SIZE), dtype=np.int8) * EG.EMPTY
	playerInput = np.ones((batchSize, 1, EG.BOARD_SIZE, EG.BOARD_SIZE), dtype=np.int8)
	castlingStateInput = np.zeros((batchSize, 1, EG.BOARD_SIZE, EG.BOARD_SIZE), dtype=np.int8)
	enPassantStateInput = np.zeros((batchSize, 1, EG.BOARD_SIZE, EG.BOARD_SIZE), dtype=np.int8)
	for i, stateHistory in enumerate(stateHistories):
		for j in range(min(BOARD_HISTORY, len(stateHistory))):
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

