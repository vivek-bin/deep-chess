import os
from copy import deepcopy
import numpy as np
import random
from . import constants as CONST

def initializeGame():
	state = {}
	state["BOARD"] = np.ones((2, CONST.BOARD_SIZE, CONST.BOARD_SIZE), dtype=np.int8) * CONST.EMPTY
	for player in [CONST.WHITE_IDX, CONST.BLACK_IDX]:
		for i, piece in enumerate([CONST.ROOK, CONST.KNIGHT, CONST.BISHOP, CONST.QUEEN, CONST.KING, CONST.BISHOP, CONST.KNIGHT, CONST.ROOK]):
			state["BOARD"][player, CONST.KING_LINE[player], i] = piece
		
		pawnLine = CONST.KING_LINE[player] + CONST.PAWN_DIRECTION[player][0]
		for i in range(CONST.BOARD_SIZE):
			state["BOARD"][player, pawnLine, i] = CONST.PAWN
	
	state["CASTLING_AVAILABLE"] = {CONST.WHITE_IDX:{CONST.LEFT_CASTLE:0, CONST.RIGHT_CASTLE:0}, CONST.BLACK_IDX:{CONST.LEFT_CASTLE:0, CONST.RIGHT_CASTLE:0}}
	for player in [CONST.WHITE_IDX, CONST.BLACK_IDX]:
		if state["BOARD"][player, CONST.KING_LINE[player], CONST.BOARD_SIZE//2] == CONST.KING:
			if state["BOARD"][player, CONST.KING_LINE[player], 0] == CONST.ROOK:
				state["CASTLING_AVAILABLE"][player][CONST.LEFT_CASTLE] = 1
			if state["BOARD"][player, CONST.KING_LINE[player], CONST.BOARD_SIZE - 1] == CONST.ROOK:
				state["CASTLING_AVAILABLE"][player][CONST.RIGHT_CASTLE] = 1

	state["EN_PASSANT"] = {CONST.WHITE_IDX:-1, CONST.BLACK_IDX:-1}
	state["PLAYER"] = CONST.WHITE_IDX

	return state

def allActions(state):
	player = state["PLAYER"]
	actions = []

	for i in range(CONST.BOARD_SIZE):
		for j in range(CONST.BOARD_SIZE):
			position = np.array((i, j), dtype=np.int8)
			for move in positionAllMoves(state, position):
				tempState = updateBoard(state, move)
				kingPos = kingPosition(tempState, player)
				for pos in attackedSquares(tempState):				# remove moves which allow opponent to capture king in next move
					if kingPos[0]==pos[0] and kingPos[1]==pos[1]:
						break
				else:
					actions.append(move)
	
	return actions

def attackedSquares(state):
	for i in range(CONST.BOARD_SIZE):
		for j in range(CONST.BOARD_SIZE):
			position = np.array((i, j), dtype=np.int8)
			for _, p in positionAllMoves(state, position, onlyFetchAttackSquares=True):
				yield p

def positionAllMoves(state, position, onlyFetchAttackSquares=False):
	player = CONST.OPPONENT[state["PLAYER"]] if onlyFetchAttackSquares else state["PLAYER"]
	box = state["BOARD"][player, position[0], position[1]]

	if box == CONST.EMPTY:
		pass

	elif box in CONST.MOVE_DIRECTIONS.keys():
		for direction in CONST.MOVE_DIRECTIONS[box]:
			yield from clipSlideMoves(state, position, direction, CONST.BOARD_SIZE - 1, player)
		
	
	elif box == CONST.KING:
		for movement in CONST.KING_MOVES:
			newPosition, _ = positionCheck(state, position, movement, player)
			if type(newPosition) == np.ndarray:
				yield (position, newPosition)
		
		if not onlyFetchAttackSquares:								# castling not an attack move
			# castling available => king and rook not moved
			for side in [CONST.LEFT_CASTLE, CONST.RIGHT_CASTLE]:
				if state["CASTLING_AVAILABLE"][player][side]:
					if not [True for p in CONST.KING_CASTLE_STEPS[player][side] if state["BOARD"][player, p[0], p[1]] != CONST.EMPTY or state["BOARD"][CONST.OPPONENT[player], p[0], p[1]] != CONST.EMPTY]:
						# check if king or his movement under attack
						blockedSquares = attackedSquares(state)
						if not [True for p in blockedSquares for cp in CONST.KING_CASTLE_STEPS[player][side] if p[0]==cp[0] and p[1]==cp[1]]:
							yield (position, CONST.KING_CASTLE_STEPS[player][side][0])


	elif box == CONST.KNIGHT:
		for movement in CONST.KNIGHT_MOVES:
			newPosition, _ = positionCheck(state, position, movement, player)
			if type(newPosition) == np.ndarray:
				yield (position, newPosition)


	elif box == CONST.PAWN:
		for movement in CONST.PAWN_CAPTURE_MOVES:
			movement = movement * CONST.PAWN_DIRECTION[player]
			newPosition, capture = positionCheck(state, position, movement, player)
			if type(newPosition) == np.ndarray:
				if capture:
					if not onlyFetchAttackSquares and (newPosition[0] == CONST.KING_LINE[CONST.OPPONENT[player]]):		# reach opponents king line
						yield (position, np.append(newPosition, CONST.ROOK))
						yield (position, np.append(newPosition, CONST.BISHOP))
						yield (position, np.append(newPosition, CONST.KNIGHT))
						yield (position, np.append(newPosition, CONST.QUEEN))
					else:
						yield (position, newPosition)
				else:
					if state["EN_PASSANT"][CONST.OPPONENT[player]] == newPosition[1]:
						if (CONST.KING_LINE[CONST.OPPONENT[player]] + ((1+2) * CONST.PAWN_DIRECTION[CONST.OPPONENT[player]][0])) == position[0]:
							if state["BOARD"][CONST.OPPONENT[player], position[0], newPosition[1]] == CONST.PAWN:
								yield (position, newPosition)

		if not onlyFetchAttackSquares:
			movement = CONST.PAWN_NORMAL_MOVE * CONST.PAWN_DIRECTION[player]
			newPosition, capture = positionCheck(state, position, movement, player)
			if (not capture) and type(newPosition) == np.ndarray:
				if newPosition[0] == CONST.KING_LINE[CONST.OPPONENT[player]]:		# reach opponents king line
					yield (position, np.append(newPosition, CONST.ROOK))
					yield (position, np.append(newPosition, CONST.BISHOP))
					yield (position, np.append(newPosition, CONST.KNIGHT))
					yield (position, np.append(newPosition, CONST.QUEEN))
				else:
					yield (position, newPosition)
				
					if position[0] == (CONST.KING_LINE[player] + CONST.PAWN_DIRECTION[player][0]):		#check if pawn's first move
						movement = CONST.PAWN_FIRST_MOVE * CONST.PAWN_DIRECTION[player]
						newPosition, capture = positionCheck(state, position, movement, player)
						if (not capture) and type(newPosition) == np.ndarray:
							yield (position, newPosition)
	else:
		raise Exception

def updateBoard(state, move):
	newState = deepcopy(state)
	newBoard = newState["BOARD"]
	currentPos = move[0]
	newPos = move[1]
	player = state["PLAYER"]

	piece = newBoard[player, currentPos[0], currentPos[1]]
	opponentPiece = newBoard[CONST.OPPONENT[player], newPos[0], newPos[1]]
	newBoard[CONST.OPPONENT[player], newPos[0], newPos[1]] = CONST.EMPTY
	
	newBoard[player, currentPos[0], currentPos[1]] = CONST.EMPTY
	newBoard[player, newPos[0], newPos[1]] = piece

	# reset en passant as only allowed immediately after
	newState["EN_PASSANT"][player] = -1

	if piece == CONST.KING:
		newState["CASTLING_AVAILABLE"][player][CONST.LEFT_CASTLE] = 0
		newState["CASTLING_AVAILABLE"][player][CONST.RIGHT_CASTLE] = 0

		if (currentPos[1] - newPos[1]) == 2:		#right castling
			newBoard[player, currentPos[0], CONST.BOARD_SIZE - 1] = CONST.EMPTY
			newBoard[player, newPos[0], newPos[1] - 1] = CONST.ROOK
		elif (currentPos[1] - newPos[1]) == -2:		#left castling
			newBoard[player, currentPos[0], 0] = CONST.EMPTY
			newBoard[player, newPos[0], newPos[1] + 1] = CONST.ROOK
	
	elif piece == CONST.PAWN:
		pawnLine = (CONST.KING_LINE[player] + CONST.PAWN_DIRECTION[player][0])
		if newPos[0] == CONST.KING_LINE[CONST.OPPONENT[player]]:
			newBoard[player, newPos[0], newPos[1]] = newPos[2]			#promotion
		elif currentPos[0] == pawnLine and abs(currentPos[0] - newPos[0]) > 1:
			newState["EN_PASSANT"][player] = newPos[1]
		elif opponentPiece == CONST.EMPTY:			#was en-passant
			newBoard[CONST.OPPONENT[player], currentPos[0], newPos[1]] = CONST.EMPTY
			opponentPiece = CONST.PAWN
	
	elif piece == CONST.ROOK:
		if currentPos[1] == 0 and newState["CASTLING_AVAILABLE"][player][CONST.LEFT_CASTLE]:
			newState["CASTLING_AVAILABLE"][player][CONST.LEFT_CASTLE] = 0
		elif currentPos[1] == CONST.BOARD_SIZE - 1 and newState["CASTLING_AVAILABLE"][player][CONST.RIGHT_CASTLE]:
			newState["CASTLING_AVAILABLE"][player][CONST.RIGHT_CASTLE] = 0
	
	elif piece in [CONST.BISHOP, CONST.KNIGHT, CONST.QUEEN]:
		pass
	
	else:
		raise Exception

	return newState

def actionIndex(move):
	currentPos = move[0]
	newPos = move[1]

	if len(newPos) > 2:			# promotion
		promotion = CONST.PROMOTIONS.index(newPos[2])
		newPawnLinearPos = (newPos[0] // (CONST.BOARD_SIZE - 1)) * CONST.BOARD_SIZE + newPos[1]
		idx = (newPawnLinearPos * CONST.BOARD_SIZE + currentPos[1]) * len(CONST.PROMOTIONS) + promotion

		idx = idx + 1 + (CONST.BOARD_SIZE**4)
	else:
		currentLinearPos = currentPos[0] * CONST.BOARD_SIZE + currentPos[1]
		newLinearPos = newPos[0] * CONST.BOARD_SIZE + newPos[1]
		idx = newLinearPos * (CONST.BOARD_SIZE**2) + currentLinearPos

	return idx
	
def actionFromIndex(idx):
	if idx < (CONST.BOARD_SIZE**4):
		currentLinearPos = idx % (CONST.BOARD_SIZE * CONST.BOARD_SIZE)
		currentPos = np.array((currentLinearPos // CONST.BOARD_SIZE, currentLinearPos % CONST.BOARD_SIZE))
		
		idx = idx // (CONST.BOARD_SIZE**2)
		newLinearPos = idx % (CONST.BOARD_SIZE * CONST.BOARD_SIZE)
		newPos = np.array((newLinearPos // CONST.BOARD_SIZE, newLinearPos % CONST.BOARD_SIZE))
	else:
		idx = idx - 1 - (CONST.BOARD_SIZE**4)

		promotion = CONST.PROMOTIONS[idx % len(CONST.PROMOTIONS)]
		idx = idx // len(CONST.PROMOTIONS)

		currentPosCol = idx % CONST.BOARD_SIZE
		idx = idx // CONST.BOARD_SIZE
		newPosCol = idx % CONST.BOARD_SIZE
		idx = idx // CONST.BOARD_SIZE
		newPosRow = idx * (CONST.BOARD_SIZE - 1)
		currentPosRow = abs(newPosRow - 1)

		currentPos = np.array((currentPosRow, currentPosCol))
		newPos = np.array((newPosRow, newPosCol, promotion))

	return (currentPos, newPos)

def stateIndex(state):
	idx =  state["BOARD"].tobytes()
	sIdx = (str(state["EN_PASSANT"][CONST.WHITE_IDX]) , str(state["EN_PASSANT"][CONST.BLACK_IDX])
		, str(state["CASTLING_AVAILABLE"][CONST.WHITE_IDX][CONST.LEFT_CASTLE]) , str(state["CASTLING_AVAILABLE"][CONST.WHITE_IDX][CONST.RIGHT_CASTLE])
		, str(state["CASTLING_AVAILABLE"][CONST.BLACK_IDX][CONST.LEFT_CASTLE])	, str(state["CASTLING_AVAILABLE"][CONST.BLACK_IDX][CONST.RIGHT_CASTLE])
		, str(state["PLAYER"]))

	return idx + ",".join(sIdx)

def stateFromIndex(idx):
	state = {}
	boardIdx = idx[:CONST.BOARD_SIZE*CONST.BOARD_SIZE]
	stateIdx = idx[CONST.BOARD_SIZE*CONST.BOARD_SIZE:].split(",")

	state["BOARD"] = np.array(np.frombuffer(boardIdx, dtype=np.int8))

	state["EN_PASSANT"] = {}
	state["EN_PASSANT"][CONST.WHITE_IDX] = int(stateIdx[0])
	state["EN_PASSANT"][CONST.BLACK_IDX] = int(stateIdx[1])

	state["CASTLING_AVAILABLE"] = {CONST.WHITE_IDX:{}, CONST.BLACK_IDX:{}}
	state["CASTLING_AVAILABLE"][CONST.WHITE_IDX][CONST.LEFT_CASTLE] = int(stateIdx[2])
	state["CASTLING_AVAILABLE"][CONST.WHITE_IDX][CONST.RIGHT_CASTLE] = int(stateIdx[3])
	state["CASTLING_AVAILABLE"][CONST.BLACK_IDX][CONST.LEFT_CASTLE] = int(stateIdx[4])
	state["CASTLING_AVAILABLE"][CONST.BLACK_IDX][CONST.RIGHT_CASTLE] = int(stateIdx[5])

	state["PLAYER"] = int(stateIdx[6])

	return state

def clipSlideMoves(state, position, direction, stepSize, player):
	assert np.shape(direction) == (2,)
	assert np.shape(position) == (2,)
	assert direction[0] in (0, 1, -1)
	assert direction[1] in (0, 1, -1)

	moves = []

	for i in range(1, stepSize + 1):
		movement = direction * i
		pos = position + movement
		if np.max(pos) >= CONST.BOARD_SIZE or np.min(pos) < 0:
			break
		if state["BOARD"][player, pos[0], pos[1]] != CONST.EMPTY:
			break

		if state["BOARD"][CONST.OPPONENT[player], pos[0], pos[1]] == CONST.EMPTY:
			moves.append((position, pos))
		else:
			moves.append((position, pos))
			break

	return moves

def positionCheck(state, position, movement, player):
	assert np.shape(position) == (2,)
	assert np.shape(movement) == (2,)

	newPosition = position + movement

	if newPosition[0] < 0 or newPosition[0] >= CONST.BOARD_SIZE:
		return False, False
	if newPosition[1] < 0 or newPosition[1] >= CONST.BOARD_SIZE:
		return False, False

	if state["BOARD"][player, newPosition[0], newPosition[1]] != CONST.EMPTY:
		return False, False

	capture = state["BOARD"][CONST.OPPONENT[player], newPosition[0], newPosition[1]]
	capture = False if capture == CONST.EMPTY else capture

	return newPosition, capture

def kingPosition(state, player):
	positions = []

	for i in range(CONST.BOARD_SIZE):
		for j in range(CONST.BOARD_SIZE):
			if state["BOARD"][player, i, j] == CONST.KING:
				return np.array((i, j), dtype=np.int8)
	
	raise Exception

def play():
	history = []

	state = initializeGame()
	count = 0
	while True:
		actions = allActions(state)
		if (not actions) or (count >= CONST.MAX_MOVES) or (np.array_equal((state["BOARD"]!=CONST.EMPTY), (state["BOARD"]==CONST.KING))):
			break

		selectedAction = actions[int(random.random()*len(actions))]
		history.append((state, selectedAction))

		state = updateBoard(state, selectedAction)
		state["PLAYER"] = CONST.OPPONENT[state["PLAYER"]]
		count += 1

	history.append(state)
	_ = finalScore(history)
	return history

def finalScore(history):
	state = history[-1]
	
	score = 0
	msg = ""
	if len(history) >= CONST.MAX_MOVES:
		msg = "draw, moves exceeded!"
	elif np.array_equal((state["BOARD"]!=CONST.EMPTY), (state["BOARD"]==CONST.KING)):
		msg = "draw, only kings!"
	else:
		kingPos = kingPosition(state, state["PLAYER"])
		for p in attackedSquares(state):
			if kingPos[0]==p[0] and kingPos[1]==p[1]:
				winner = CONST.OPPONENT[state["PLAYER"]]
				msg = "winner : " + str(winner)
				score = CONST.SCORING[winner]
				break
		else:
			msg = "draw, stalemate!"
	
	print(state["BOARD"][CONST.WHITE_IDX] - state["BOARD"][CONST.BLACK_IDX])
	print(msg, len(history), state["PLAYER"])
	return score

if __name__ == "__main__":
	play()
