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
	actions = []
	
	for i in range(CONST.BOARD_SIZE):
		for j in range(CONST.BOARD_SIZE):
			position = np.array((i, j), dtype=np.int8)
			actions.extend(positionAllowedMoves(state, position, state["PLAYER"]))
	
	return actions

def kingAttacked(state, player):
	kingPos = None
	for i in range(CONST.BOARD_SIZE):
		for j in range(CONST.BOARD_SIZE):
			if state["BOARD"][player, i, j] == CONST.KING:
				kingPos = np.array((i, j), dtype=np.int8)
				break
		if kingPos is not None:
			break
	else:
		raise Exception		# no king found

	return positionAttacked(state, kingPos, player)

def positionAttacked(state, position, player):
	originalPiece = state["BOARD"][player, position[0], position[1]]

	for piece in [CONST.PAWN, CONST.KNIGHT, CONST.BISHOP, CONST.ROOK, CONST.QUEEN, CONST.KING]:
		state["BOARD"][player, position[0], position[1]] = piece
		for _, p in positionMoves(state, position, player, onlyFetchAttackSquares=True):
			if state["BOARD"][CONST.OPPONENT[player], p[0], p[1]] == piece:
				state["BOARD"][player, position[0], position[1]] = originalPiece
				return True
	
	state["BOARD"][player, position[0], position[1]] = originalPiece
	return False

def positionAllowedMoves(state, position, player):
	moves = [move for move in positionMoves(state, position, player) if not kingAttacked(updateBoard(state, move), player)]
	#
	# the list comprehension above is a quicker version of the code below 
	#moves = []
	#for move in positionMoves(state, position, player):
	#	tempState = updateBoard(state, move)
	#	if not kingAttacked(tempState, player):
	#		moves.append(move)
	return moves

def positionMoves(state, position, player, onlyFetchAttackSquares=False):
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
						for cp in CONST.KING_CASTLE_STEPS[player][side]:
							if positionAttacked(state, cp, player):
								break
						else:
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

def clipSlideMoves(state, position, direction, stepSize, player):
	assert np.shape(direction) == (2,)
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

def play():
	history = []

	state = initializeGame()
	count = 0
	while True:
		actions = allActions(state)
		if (not actions) or (len(history) >= CONST.MAX_MOVES) or (np.array_equal((state["BOARD"]!=CONST.EMPTY), (state["BOARD"]==CONST.KING))):
			break

		selectedAction = actions[int(random.random()*len(actions))]
		history.append(dict(state=state, action=selectedAction))

		state = updateBoard(state, selectedAction)
		state["PLAYER"] = CONST.OPPONENT[state["PLAYER"]]
		count += 1

	history.append(state)
	_ = finalScore(history)
	return history

def finalScore(history):
	state = history[-1]["STATE"]
	
	score = 0
	msg = ""
	if len(history) >= CONST.MAX_MOVES:
		msg = "draw, moves exceeded!"
	elif np.array_equal((state["BOARD"]!=CONST.EMPTY), (state["BOARD"]==CONST.KING)):
		msg = "draw, only kings!"
	else:
		if kingAttacked(state, state["PLAYER"]):
			winner = CONST.OPPONENT[state["PLAYER"]]
			score = CONST.SCORING[winner]
			msg = "winner : " + str(winner)
		else:
			msg = "draw, stalemate!"
	
	print(state["BOARD"][CONST.WHITE_IDX] - state["BOARD"][CONST.BLACK_IDX])
	print(msg, len(history), state["PLAYER"])
	return score

if __name__ == "__main__":
	play()
