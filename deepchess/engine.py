from copy import deepcopy
import random

BOARD_SIZE = 8
MAX_GAME_STEPS = 200
WHITE_IDX = 0
BLACK_IDX = 1
OPPONENT = {BLACK_IDX:WHITE_IDX, WHITE_IDX:BLACK_IDX}
SCORING = {BLACK_IDX:-1, WHITE_IDX:1}

EMPTY = 0
PAWN = 1
BISHOP = 2
KNIGHT = 3
ROOK = 4
QUEEN = 5
KING = 6
PROMOTIONS = [QUEEN, ROOK, KNIGHT, BISHOP]

MOVE_DIRECTIONS = {}
MOVE_DIRECTIONS[BISHOP] = [(1, 1), (1, -1), (-1, -1), (-1, 1)]
MOVE_DIRECTIONS[ROOK] = [(1, 0), (-1, 0), (0, -1), (0, 1)]
MOVE_DIRECTIONS[QUEEN] = MOVE_DIRECTIONS[BISHOP] + MOVE_DIRECTIONS[ROOK]

KNIGHT_MOVES = [(1, 2), (2, 1), (-1, 2), (2, -1), (1, -2), (-2, 1), (-1, -2), (-2, -1)]

KING_MOVES = MOVE_DIRECTIONS[BISHOP] + MOVE_DIRECTIONS[ROOK]
KING_LINE = {WHITE_IDX:0, BLACK_IDX:BOARD_SIZE - 1}
LEFT_CASTLE = 0
RIGHT_CASTLE = 1
KING_CASTLE_STEPS = {WHITE_IDX:{LEFT_CASTLE:[], RIGHT_CASTLE:[]}, BLACK_IDX:{LEFT_CASTLE:[], RIGHT_CASTLE:[]}}
for player in [WHITE_IDX, BLACK_IDX]:
	for i in range(3):
		KING_CASTLE_STEPS[player][LEFT_CASTLE].append((KING_LINE[player], BOARD_SIZE//2 - i - 1))
		KING_CASTLE_STEPS[player][RIGHT_CASTLE].append((KING_LINE[player], BOARD_SIZE//2 + i - 1))

PAWN_CAPTURE_MOVES = [(1, 1), (1, -1)]
PAWN_NORMAL_MOVE = (1, 0)
PAWN_FIRST_MOVE = (2, 0)
PAWN_DIRECTION = {WHITE_IDX:(1, 1), BLACK_IDX:(-1, 1)}

MAX_POSSIBLE_MOVES = BOARD_SIZE**4 + 2 * len(PROMOTIONS) * BOARD_SIZE**2

def ADDL(l1, l2):
	assert len(l1) == len(l2)
	return tuple(l1[i]+l2[i] for i in range(len(l1)))

def MULTIPLYL(l1, l2):
	assert len(l1) == len(l2)
	return tuple(l1[i]*l2[i] for i in range(len(l1)))

	
def actionIndex(move):
	if move is None:
		return None
	currentPos = move[0]
	newPos = move[1]

	if len(newPos) > 2:			# promotion
		promotion = PROMOTIONS.index(newPos[2])
		newPawnLinearPos = (newPos[0] // (BOARD_SIZE - 1)) * BOARD_SIZE + newPos[1]
		idx = (newPawnLinearPos * BOARD_SIZE + currentPos[1]) * len(PROMOTIONS) + promotion

		idx = idx + (BOARD_SIZE**4)
	else:
		currentLinearPos = currentPos[0] * BOARD_SIZE + currentPos[1]
		newLinearPos = newPos[0] * BOARD_SIZE + newPos[1]
		idx = newLinearPos * (BOARD_SIZE**2) + currentLinearPos

	return idx
	
def actionFromIndex(idx):
	if idx < (BOARD_SIZE**4):
		currentLinearPos = idx % (BOARD_SIZE * BOARD_SIZE)
		currentPos = (currentLinearPos // BOARD_SIZE, currentLinearPos % BOARD_SIZE)
		
		idx = idx // (BOARD_SIZE**2)
		newLinearPos = idx % (BOARD_SIZE * BOARD_SIZE)
		newPos = (newLinearPos // BOARD_SIZE, newLinearPos % BOARD_SIZE)
	else:
		idx = idx - (BOARD_SIZE**4)

		promotion = PROMOTIONS[idx % len(PROMOTIONS)]
		idx = idx // len(PROMOTIONS)

		currentPosCol = idx % BOARD_SIZE
		idx = idx // BOARD_SIZE
		newPosCol = idx % BOARD_SIZE
		idx = idx // BOARD_SIZE
		newPosRow = idx * (BOARD_SIZE - 1)
		currentPosRow = abs(newPosRow - 1)

		currentPos = (currentPosRow, currentPosCol)
		newPos = (newPosRow, newPosCol, promotion)

	return (currentPos, newPos)

def stateIndex(state):
	if state is None:
		return None

	charify = lambda x:chr(ord('0')+x)

	idx =  "".join([charify(box) for playerBoard in state["BOARD"] for row in playerBoard for box in row])
	sIdx = (charify(state["EN_PASSANT"][WHITE_IDX]), charify(state["EN_PASSANT"][BLACK_IDX])
		, charify(state["CASTLING_AVAILABLE"][WHITE_IDX][LEFT_CASTLE]) 
		, charify(state["CASTLING_AVAILABLE"][WHITE_IDX][RIGHT_CASTLE])
		, charify(state["CASTLING_AVAILABLE"][BLACK_IDX][LEFT_CASTLE]) 
		, charify(state["CASTLING_AVAILABLE"][BLACK_IDX][RIGHT_CASTLE])
		, charify(state["PLAYER"]))

	return idx + "".join(sIdx)

def stateFromIndex(idx):
	unCharify = lambda x:(ord(x)-ord('0'))

	state = {}
	boardIdx = [unCharify(x) for x in idx[:2*BOARD_SIZE*BOARD_SIZE]]
	stateIdx = [unCharify(x) for x in idx[2*BOARD_SIZE*BOARD_SIZE:]]

	state["BOARD"] = []
	for p in range(2):
		state["BOARD"].append([])
		for i in range(BOARD_SIZE):
			rowIdx = p*BOARD_SIZE*BOARD_SIZE + i*BOARD_SIZE
			state["BOARD"][p].append(boardIdx[rowIdx:rowIdx + BOARD_SIZE])

	state["EN_PASSANT"] = {}
	state["EN_PASSANT"][WHITE_IDX] = stateIdx[0]
	state["EN_PASSANT"][BLACK_IDX] = stateIdx[1]

	state["CASTLING_AVAILABLE"] = {WHITE_IDX:{}, BLACK_IDX:{}}
	state["CASTLING_AVAILABLE"][WHITE_IDX][LEFT_CASTLE] = stateIdx[2]
	state["CASTLING_AVAILABLE"][WHITE_IDX][RIGHT_CASTLE] = stateIdx[3]
	state["CASTLING_AVAILABLE"][BLACK_IDX][LEFT_CASTLE] = stateIdx[4]
	state["CASTLING_AVAILABLE"][BLACK_IDX][RIGHT_CASTLE] = stateIdx[5]

	state["PLAYER"] = stateIdx[6]

	return state


def initializeGame():
	state = {}
	state["BOARD"] = [[[EMPTY]*BOARD_SIZE for _ in range(BOARD_SIZE)] for _ in range(2)]
	for player in [WHITE_IDX, BLACK_IDX]:
		for i, piece in enumerate([ROOK, KNIGHT, BISHOP, KING, QUEEN, BISHOP, KNIGHT, ROOK]):
			state["BOARD"][player][KING_LINE[player]][i] = piece
		
		pawnLine = KING_LINE[player] + PAWN_DIRECTION[player][0]
		for i in range(BOARD_SIZE):
			state["BOARD"][player][pawnLine][i] = PAWN
	
	state["CASTLING_AVAILABLE"] = {WHITE_IDX:{LEFT_CASTLE:0, RIGHT_CASTLE:0}, BLACK_IDX:{LEFT_CASTLE:0, RIGHT_CASTLE:0}}
	for player in [WHITE_IDX, BLACK_IDX]:
		if state["BOARD"][player][KING_LINE[player]][KING_CASTLE_STEPS[player][LEFT_CASTLE][0][1]] == KING:
			if state["BOARD"][player][KING_LINE[player]][0] == ROOK:
				state["CASTLING_AVAILABLE"][player][LEFT_CASTLE] = 1
			if state["BOARD"][player][KING_LINE[player]][KING_CASTLE_STEPS[player][RIGHT_CASTLE][0][1]] == ROOK:
				state["CASTLING_AVAILABLE"][player][RIGHT_CASTLE] = 1

	state["EN_PASSANT"] = {WHITE_IDX:-1, BLACK_IDX:-1}
	state["PLAYER"] = WHITE_IDX

	return state

def allActions(state):
	actions = []
	
	for i in range(BOARD_SIZE):
		for j in range(BOARD_SIZE):
			actions.extend(positionAllowedMoves(state, (i, j)))
	
	return tuple(actions)

def kingAttacked(state, player):
	kingPos = None
	for i in range(BOARD_SIZE):
		for j in range(BOARD_SIZE):
			if state["BOARD"][player][i][j] == KING:
				kingPos = (i, j)
				break
		if kingPos is not None:
			break
	else:
		raise Exception		# no king found

	return positionAttacked(state, kingPos, player)

def positionAttacked(state, position, player):
	originalPiece = state["BOARD"][player][position[0]][position[1]]

	for piece in [PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING]:
		state["BOARD"][player][position[0]][position[1]] = piece
		for _, p in positionMoves(state, position, player, onlyFetchAttackSquares=True):
			if state["BOARD"][OPPONENT[player]][p[0]][p[1]] == piece:
				state["BOARD"][player][position[0]][position[1]] = originalPiece
				return True
	
	state["BOARD"][player][position[0]][position[1]] = originalPiece
	return False

def positionAllowedMoves(state, position):
	moves = [move for move in positionMoves(state, position, state["PLAYER"]) if not kingAttacked(performAction(state, move), state["PLAYER"])]
	#
	# the list comprehension above is a quicker version of the code below 
	#moves = []
	#for move in positionMoves(state, position, state["PLAYER"]):
	#	tempState = performAction(state, move)
	#	if not kingAttacked(tempState, state["PLAYER"]):
	#		moves.append(move)
	return moves

def positionMoves(state, position, player, onlyFetchAttackSquares=False):
	box = state["BOARD"][player][position[0]][position[1]]

	if box == EMPTY:
		pass

	elif box in MOVE_DIRECTIONS.keys():
		for direction in MOVE_DIRECTIONS[box]:
			yield from clipSlideMoves(state, position, direction, BOARD_SIZE - 1, player)
		
	
	elif box == KING:
		for movement in KING_MOVES:
			newPosition, _ = positionCheck(state, position, movement, player)
			if newPosition:
				yield (position, newPosition)
		
		if not onlyFetchAttackSquares:								# castling not an attack move
			# castling available => king and rook not moved
			for side in [LEFT_CASTLE, RIGHT_CASTLE]:
				if state["CASTLING_AVAILABLE"][player][side]:
					if not [True for p in KING_CASTLE_STEPS[player][side][1:] if state["BOARD"][player][p[0]][p[1]] != EMPTY or state["BOARD"][OPPONENT[player]][p[0]][p[1]] != EMPTY]:
						# check if king or his movement under attack
						for cp in KING_CASTLE_STEPS[player][side]:
							if positionAttacked(state, cp, player):
								break
						else:
							yield (position, KING_CASTLE_STEPS[player][side][-1])


	elif box == KNIGHT:
		for movement in KNIGHT_MOVES:
			newPosition, _ = positionCheck(state, position, movement, player)
			if newPosition:
				yield (position, newPosition)


	elif box == PAWN:
		for movement in PAWN_CAPTURE_MOVES:
			movement = MULTIPLYL(movement, PAWN_DIRECTION[player])
			newPosition, capture = positionCheck(state, position, movement, player)
			if newPosition:
				if capture:
					if not onlyFetchAttackSquares and (newPosition[0] == KING_LINE[OPPONENT[player]]):		# reach opponents king line
						yield (position, newPosition+(ROOK,))
						yield (position, newPosition+(BISHOP,))
						yield (position, newPosition+(KNIGHT,))
						yield (position, newPosition+(QUEEN,))
					else:
						yield (position, newPosition)
				else:
					if state["EN_PASSANT"][OPPONENT[player]] == newPosition[1]:
						if (KING_LINE[OPPONENT[player]] + ((1+2) * PAWN_DIRECTION[OPPONENT[player]][0])) == position[0]:
							if state["BOARD"][OPPONENT[player]][position[0]][newPosition[1]] == PAWN:
								yield (position, newPosition)

		if not onlyFetchAttackSquares:
			movement = MULTIPLYL(PAWN_NORMAL_MOVE, PAWN_DIRECTION[player])
			newPosition, capture = positionCheck(state, position, movement, player)
			if (not capture) and newPosition:
				if newPosition[0] == KING_LINE[OPPONENT[player]]:		# reach opponents king line
					yield (position, newPosition+(ROOK,))
					yield (position, newPosition+(BISHOP,))
					yield (position, newPosition+(KNIGHT,))
					yield (position, newPosition+(QUEEN,))
				else:
					yield (position, newPosition)
				
					if position[0] == (KING_LINE[player] + PAWN_DIRECTION[player][0]):		#check if pawn's first move
						movement = MULTIPLYL(PAWN_FIRST_MOVE, PAWN_DIRECTION[player])
						newPosition, capture = positionCheck(state, position, movement, player)
						if (not capture) and newPosition:
							yield (position, newPosition)
	else:
		raise Exception

def performAction(state, move):
	newState = deepcopy(state)
	newBoard = newState["BOARD"]
	currentPos = move[0]
	newPos = move[1]
	player = state["PLAYER"]

	piece = newBoard[player][currentPos[0]][currentPos[1]]
	opponentPiece = newBoard[OPPONENT[player]][newPos[0]][newPos[1]]
	newBoard[OPPONENT[player]][newPos[0]][newPos[1]] = EMPTY
	
	newBoard[player][currentPos[0]][currentPos[1]] = EMPTY
	newBoard[player][newPos[0]][newPos[1]] = piece

	# reset en passant as only allowed immediately after
	newState["EN_PASSANT"][player] = -1

	if piece == KING:
		newState["CASTLING_AVAILABLE"][player][LEFT_CASTLE] = 0
		newState["CASTLING_AVAILABLE"][player][RIGHT_CASTLE] = 0

		if (currentPos[1] - newPos[1]) == 2:		#right castling
			newBoard[player][currentPos[0]][0] = EMPTY
			newBoard[player][newPos[0]][newPos[1] + 1] = ROOK
		elif (currentPos[1] - newPos[1]) == -2:		#left castling
			newBoard[player][currentPos[0]][BOARD_SIZE - 1] = EMPTY
			newBoard[player][newPos[0]][newPos[1] - 1] = ROOK
	
	elif piece == PAWN:
		pawnLine = (KING_LINE[player] + PAWN_DIRECTION[player][0])
		if newPos[0] == KING_LINE[OPPONENT[player]]:
			newBoard[player][newPos[0]][newPos[1]] = newPos[2]			#promotion
		elif currentPos[0] == pawnLine and abs(currentPos[0] - newPos[0]) > 1:
			newState["EN_PASSANT"][player] = newPos[1]
		elif opponentPiece == EMPTY and newPos[1]==newState["EN_PASSANT"][OPPONENT[player]]:			#was en-passant
			newBoard[OPPONENT[player]][currentPos[0]][newPos[1]] = EMPTY
	
	elif piece == ROOK:
		if currentPos[1] == 0 and newState["CASTLING_AVAILABLE"][player][LEFT_CASTLE]:
			newState["CASTLING_AVAILABLE"][player][LEFT_CASTLE] = 0
		elif currentPos[1] == BOARD_SIZE - 1 and newState["CASTLING_AVAILABLE"][player][RIGHT_CASTLE]:
			newState["CASTLING_AVAILABLE"][player][RIGHT_CASTLE] = 0
	
	elif piece in [BISHOP, KNIGHT, QUEEN]:
		pass
	
	else:
		raise Exception


	newState["PLAYER"] = OPPONENT[newState["PLAYER"]]

	return newState

def clipSlideMoves(state, position, direction, stepSize, player):
	assert len(direction) == 2
	assert direction[0] in (0, 1, -1)
	assert direction[1] in (0, 1, -1)

	moves = []

	for i in range(1, stepSize + 1):
		movement = [direction[j]*i for j in range(len(direction))]
		pos = ADDL(position, movement)
		if max(pos) >= BOARD_SIZE or min(pos) < 0:
			break
		if state["BOARD"][player][pos[0]][pos[1]] != EMPTY:
			break

		if state["BOARD"][OPPONENT[player]][pos[0]][pos[1]] == EMPTY:
			moves.append((position, pos))
		else:
			moves.append((position, pos))
			break

	return moves

def positionCheck(state, position, movement, player):
	newPosition = ADDL(position, movement)

	if newPosition[0] < 0 or newPosition[0] >= BOARD_SIZE:
		return False, False
	if newPosition[1] < 0 or newPosition[1] >= BOARD_SIZE:
		return False, False

	if state["BOARD"][player][newPosition[0]][newPosition[1]] != EMPTY:
		return False, False

	capture = state["BOARD"][OPPONENT[player]][newPosition[0]][newPosition[1]]
	capture = False if capture == EMPTY else capture

	return newPosition, capture

def checkGameEnd(state, numActions, duration):
	reward = 0
	end = False
	if not [box for playerBoard in state["BOARD"] for row in playerBoard for box in row if box not in (EMPTY, KING)]:
		end = "draw,only_kings"
	elif numActions == 0:
		if kingAttacked(state, state["PLAYER"]):
			winner = OPPONENT[state["PLAYER"]]
			reward = SCORING[winner]
			end = "loss"
		else:
			end = "draw,stalemate"
	elif duration > MAX_GAME_STEPS:
		end = "draw,max_steps"
	
	return end, reward

def init():
	state = initializeGame()
	actions = allActions(state)
	end, reward = checkGameEnd(state, len(actions), 0)

	return state, actions, end, reward

def play(state, action, duration=0):
	nextState = performAction(state, action)
	actions = allActions(nextState)
	end, reward = checkGameEnd(nextState, len(actions), duration)

	return nextState, actions, end, reward

def playRandomTillEnd(state):
	actions = allActions(state)
	end, reward = checkGameEnd(state, len(actions), duration)
	
	while not end:
		action = random.choice(actions)
		
		state = performAction(state, action)
		actions = allActions(state)
		end, reward = checkGameEnd(state, len(actions), duration)

	return state, actions, end, reward

