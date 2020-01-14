import os
import numpy as np
from . import constants as CONST

def availableMoves(board, castleAvailable, player):
	assert np.shape(board) == (2, 8, 8)
	assert type(castleAvailable) == bool
	assert player in (CONST.WHITE_IDX, CONST.BLACK_IDX)

	moves = []
	for i in range(CONST.BOARD_SIZE):
		for j in range(CONST.BOARD_SIZE):
			moves.extend(positionAvailableMoves(board, position, castleAvailable, player))

	return moves

def positionAvailableMoves(board, position, castleAvailable, player):
    box = board[player, position[0], position[1]]
	moves = []

    if box == CONST.EMPTY:
        return moves

    if box in CONST.MOVE_DIRECTIONS.keys():
        for direction in CONST.MOVE_DIRECTIONS[box]:
            moves.extend(clipSlideMoves(board, position, direction, CONST.MOVE_MAX_STEPS[box], player))
        
        if box == CONST.KING and castleAvailable:				# castling
            if np.array_equal(board[player, CONST.KING_ORIGIN[player][0], :CONST.KING_ORIGIN[player][1] + 1], np.array((CONST.ROOK, CONST.EMPTY, CONST.EMPTY, CONST.KING))):
                moves.append((position, np.array((position[0], position[1] - 2))))
            if np.array_equal(board[player, CONST.KING_ORIGIN[player][0], CONST.KING_ORIGIN[player][1]:], np.array((CONST.KING, CONST.EMPTY, CONST.EMPTY, CONST.EMPTY, CONST.ROOK))):
                moves.append((position, np.array((position[0], position[1] + 2))))
                    
        return moves
    
    if box == CONST.KNIGHT:
        for movement in CONST.KNIGHT_MOVES:
            newPosition = positionAvailabilityCheck(board, position, movement, player)
            if newPosition:
                moves.append((position, newPosition))

        return moves

    if box == CONST.PAWN:			# TO DO 'en passant', require history
        for movement in CONST.PAWN_CAPTURE_MOVES:
            newPosition = pawnPositionAvailabilityCheck(board, position, movement, player, True)
            if newPosition:
                moves.append((position, newPosition))
        
        newPosition = pawnPositionAvailabilityCheck(board, position, CONST.PAWN_NORMAL_MOVE, player, False)
        if newPosition:
            moves.append((position, newPosition))
            if position[0] == 1:		#pawn's first move
                newPosition = pawnPositionAvailabilityCheck(board, position, CONST.PAWN_FIRST_MOVE, player, False)
                if newPosition:
                    moves.append((position, newPosition))

        return moves

    raise Exception

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

def positionAvailabilityCheck(board, position, movement, player):
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

def pawnPositionAvailabilityCheck(board, position, movement, player, capture):
    movement = movement * CONST.PAWN_DIRECTION[player]
	newPosition = positionAvailabilityCheck(board, position, movement, player)

	captureMove = (board[CONST.OPPONENT[player], newPosition[0], newPosition[1]] != CONST.EMPTY)		# capture move or not

	if captureMove == capture:
		return newPosition
	else:
		return False
