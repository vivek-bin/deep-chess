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
    def __init__(self, parent=None, initialState=None):
        self.parent = parent
        self.children = []
        self.visits = 0
        self.state = initialState
        self.wins = 0
        self.losses = 0

    def addChild(self, child):
        self.children.append(child)

    def bestChild(self):
        best = self.children[0] if self.children else None
        for child in self.children:
            if child.value() > best.value():
                best = child
        
        return best

    def value(self):
        explore = math.sqrt(math.log(self.parent.visits)/self.visits) if self.parent else 0
        mean = self.wins/self.visits

        return mean + CONST.MC_EXPLORATION_CONST * explore

    
class Tree():
    def __init__(self, initialState):
        self.root = Node(None, initialState)
        self.current = self.root
        self.stateMap = {initialState:self.root}

        