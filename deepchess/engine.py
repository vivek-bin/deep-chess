import os
import numpy as np
from . import constants as CONST

def availableMoves(board, castleAvailable, player, skipKing=False):
	assert np.shape(board) == (2, 8, 8)
	assert player in (CONST.WHITE_IDX, CONST.BLACK_IDX)
	assert type(castleAvailable) == dict
	assert player in castleAvailable.keys()

	moves = []
	for i in range(CONST.BOARD_SIZE):
		for j in range(CONST.BOARD_SIZE):
			moves.extend(positionAvailableMoves(board, position, castleAvailable, player, skipKing=skipKing))
	
	return moves

def positionAvailableMoves(board, position, castleAvailable, player, onlyFetchAttackSquares=False):
    box = board[player, position[0], position[1]]
	moves = []

    if box == CONST.EMPTY:
        return moves

    if box in CONST.MOVE_DIRECTIONS.keys():
        for direction in CONST.MOVE_DIRECTIONS[box]:
            moves.extend(clipSlideMoves(board, position, direction, BOARD_SIZE - 1, player))
        
        return moves
    
    if box == CONST.KING:
		if not onlyFetchAttackSquares:
			attackedSquares = availableMoves(board, castleAvailable, CONST.OPPONENT[player], onlyFetchAttackSquares=True)		# castling not an attack move; king cant attack king
        for movement in CONST.KING_MOVES:
            newPosition = positionMoveCheck(board, position, movement, player)
            if newPosition:
				if onlyFetchAttackSquares or not [p for _, p in attackedSquares if p[0]==newPosition[0] and p[1]==newPosition[1]]:
	                moves.append((position, newPosition))
		
		if not onlyFetchAttackSquares:
			# castleAvailable => king and rook not moved
			for side in [CONST.LEFT_CASTLE, CONST.RIGHT_CASTLE]:
				if castleAvailable[player][side]:
					if not [True for p in CONST.KING_CASTLE[player][side] if board[player, p[0], p[1]] != CONST.EMPTY or board[CONST.OPPONENT[player], p[0], p[1]] != CONST.EMPTY]:
						# check if king or his movement under attack
						if not [True for _, p in attackedSquares for cp in CONST.KING_CASTLE[player][side] if p[0]==cp[0] and p[1]==cp[1])]:
							moves.append((position, CONST.KING_CASTLE[player][side][0]))

        return moves

    if box == CONST.KNIGHT:
        for movement in CONST.KNIGHT_MOVES:
            newPosition = positionMoveCheck(board, position, movement, player)
            if newPosition:
                moves.append((position, newPosition))

        return moves

    if box == CONST.PAWN:			# TO DO 'en passant', require history
        for movement in CONST.PAWN_CAPTURE_MOVES:
            newPosition = pawnPositionMoveCheck(board, position, movement, player, True)
            if newPosition:
				if newPosition[0] == CONST.KING_ORIGIN[CONST.OPPONENT[player]][0]:
	                moves.append((position, np.append(newPosition, CONST.ROOK)))
	                moves.append((position, np.append(newPosition, CONST.BISHOP)))
	                moves.append((position, np.append(newPosition, CONST.QUEEN)))
	                moves.append((position, np.append(newPosition, CONST.KNIGHT)))
				else:
					moves.append((position, newPosition))

        if not onlyFetchAttackSquares:
			newPosition = pawnPositionMoveCheck(board, position, CONST.PAWN_NORMAL_MOVE, player, False)
			if newPosition:
				if newPosition[0] == CONST.KING_ORIGIN[CONST.OPPONENT[player]][0]:
	                moves.append((position, np.append(newPosition, CONST.ROOK)))
	                moves.append((position, np.append(newPosition, CONST.BISHOP)))
	                moves.append((position, np.append(newPosition, CONST.QUEEN)))
	                moves.append((position, np.append(newPosition, CONST.KNIGHT)))
				else:
					moves.append((position, newPosition))
				
				if position[0] == CONST.PAWN_LINE[player]:		#pawn's first move
					newPosition = pawnPositionMoveCheck(board, position, CONST.PAWN_FIRST_MOVE, player, False)
					if newPosition:
						moves.append((position, newPosition))

        return moves

    raise Exception

def updateBoard(board, move, castleAvailable, player):

	return board, castleAvailable

def moveIndex(move):
	currentPos = move[0]
	newPos = move[1]
	promotion = 0 if len(newPos) < 3 else newPos[2]
	MAX_PROMOTION = max([CONST.ROOK, CONST.KNIGHT, CONST.BISHOP, CONST.QUEEN])
	MIN_PROMOTION = min([CONST.ROOK, CONST.KNIGHT, CONST.BISHOP, CONST.QUEEN])
	PROMOTION_ROWS = 2 * (MAX_PROMOTION - MIN_PROMOTION)

	currentLinearPos = currentPos[0] * CONST.BOARD_SIZE + currentPos[1]
	newLinearPos = newPos[0] * CONST.BOARD_SIZE + newPos[1]			### DOESNT HANDLE PROMOTION

	return newLinearPos * CONST.BOARD_SIZE * CONST.BOARD_SIZE + currentLinearPos
	
def moveFromIndex(idx):
	currentLinearPos = idx % (CONST.BOARD_SIZE * CONST.BOARD_SIZE)
	currentPos = np.array((currentLinearPos / CONST.BOARD_SIZE, currentLinearPos % CONST.BOARD_SIZE))
	
	restIdx = idx / (CONST.BOARD_SIZE * CONST.BOARD_SIZE)
	y = restIdx % CONST.BOARD_SIZE
	restIdx = restIdx / CONST.BOARD_SIZE			### DOESNT HANDLE OTHER SIDE PROMOTION
	x = min(restIdx, CONST.BOARD_SIZE - 1)
	promotion = max(0, restIdx - CONST.BOARD_SIZE)

	newPosition = np.array((x, y, promotion))
	
	return (currentPos, newPosition)

def clipSlideMoves(board, position, direction, stepSize, player):
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
		if board[player, pos[0], pos[1]] != CONST.EMPTY:
			break

		if board[CONST.OPPONENT[player], pos[0], pos[1]] == CONST.EMPTY:
			moves.append((position, pos))
		else:
			moves.append((position, pos))
			break

	return moves

def positionMoveCheck(board, position, movement, player):
	assert np.shape(position) == (2,)
	assert np.shape(movement) == (2,)
	assert player in (CONST.WHITE_IDX, CONST.BLACK_IDX)

	newPosition = position + movement

	if newPosition[0] < 0 or newPosition[0] >= CONST.BOARD_SIZE:
		return False
	if newPosition[1] < 0 or newPosition[1] >= CONST.BOARD_SIZE:
		return False

	if board[player, newPosition[0], newPosition[1]] != CONST.EMPTY:
		return False

	return newPosition

def pawnPositionMoveCheck(board, position, movement, player, capture):
    movement = movement * CONST.PAWN_DIRECTION[player]
	newPosition = positionMoveCheck(board, position, movement, player)

	captureMove = (board[CONST.OPPONENT[player], newPosition[0], newPosition[1]] != CONST.EMPTY)		# capture move or not

	if captureMove == capture:
		return newPosition
	else:
		return False
