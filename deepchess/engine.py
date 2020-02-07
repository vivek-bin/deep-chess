import os
from copy import deepcopy
import random
from . import constants as CONST
import tkinter as tk
from PIL import Image, ImageTk

def initializeGame():
	state = {}
	state["BOARD"] = [[[CONST.EMPTY]*CONST.BOARD_SIZE for _ in range(CONST.BOARD_SIZE)] for _ in range(2)]
	for player in [CONST.WHITE_IDX, CONST.BLACK_IDX]:
		for i, piece in enumerate([CONST.ROOK, CONST.KNIGHT, CONST.BISHOP, CONST.QUEEN, CONST.KING, CONST.BISHOP, CONST.KNIGHT, CONST.ROOK]):
			state["BOARD"][player][CONST.KING_LINE[player]][i] = piece
		
		pawnLine = CONST.KING_LINE[player] + CONST.PAWN_DIRECTION[player][0]
		for i in range(CONST.BOARD_SIZE):
			state["BOARD"][player][pawnLine][i] = CONST.PAWN
	
	state["CASTLING_AVAILABLE"] = {CONST.WHITE_IDX:{CONST.LEFT_CASTLE:0, CONST.RIGHT_CASTLE:0}, CONST.BLACK_IDX:{CONST.LEFT_CASTLE:0, CONST.RIGHT_CASTLE:0}}
	for player in [CONST.WHITE_IDX, CONST.BLACK_IDX]:
		if state["BOARD"][player][CONST.KING_LINE[player]][CONST.BOARD_SIZE//2] == CONST.KING:
			if state["BOARD"][player][CONST.KING_LINE[player]][0] == CONST.ROOK:
				state["CASTLING_AVAILABLE"][player][CONST.LEFT_CASTLE] = 1
			if state["BOARD"][player][CONST.KING_LINE[player]][CONST.BOARD_SIZE - 1] == CONST.ROOK:
				state["CASTLING_AVAILABLE"][player][CONST.RIGHT_CASTLE] = 1

	state["EN_PASSANT"] = {CONST.WHITE_IDX:-1, CONST.BLACK_IDX:-1}
	state["PLAYER"] = CONST.WHITE_IDX

	return state

def allActions(state):
	actions = []
	
	for i in range(CONST.BOARD_SIZE):
		for j in range(CONST.BOARD_SIZE):
			actions.extend(positionAllowedMoves(state, (i, j)))
	
	return actions

def kingAttacked(state, player):
	kingPos = None
	for i in range(CONST.BOARD_SIZE):
		for j in range(CONST.BOARD_SIZE):
			if state["BOARD"][player][i][j] == CONST.KING:
				kingPos = (i, j)
				break
		if kingPos is not None:
			break
	else:
		printBoard(state)
		raise Exception		# no king found

	return positionAttacked(state, kingPos, player)

def positionAttacked(state, position, player):
	originalPiece = state["BOARD"][player][position[0]][position[1]]

	for piece in [CONST.PAWN, CONST.KNIGHT, CONST.BISHOP, CONST.ROOK, CONST.QUEEN, CONST.KING]:
		state["BOARD"][player][position[0]][position[1]] = piece
		for _, p in positionMoves(state, position, player, onlyFetchAttackSquares=True):
			if state["BOARD"][CONST.OPPONENT[player]][p[0]][p[1]] == piece:
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

	if box == CONST.EMPTY:
		pass

	elif box in CONST.MOVE_DIRECTIONS.keys():
		for direction in CONST.MOVE_DIRECTIONS[box]:
			yield from clipSlideMoves(state, position, direction, CONST.BOARD_SIZE - 1, player)
		
	
	elif box == CONST.KING:
		for movement in CONST.KING_MOVES:
			newPosition, _ = positionCheck(state, position, movement, player)
			if newPosition:
				yield (position, newPosition)
		
		if not onlyFetchAttackSquares:								# castling not an attack move
			# castling available => king and rook not moved
			for side in [CONST.LEFT_CASTLE, CONST.RIGHT_CASTLE]:
				if state["CASTLING_AVAILABLE"][player][side]:
					if not [True for p in CONST.KING_CASTLE_STEPS[player][side] if state["BOARD"][player][p[0]][p[1]] != CONST.EMPTY or state["BOARD"][CONST.OPPONENT[player]][p[0]][p[1]] != CONST.EMPTY]:
						# check if king or his movement under attack
						for cp in CONST.KING_CASTLE_STEPS[player][side]:
							if positionAttacked(state, cp, player):
								break
						else:
							yield (position, CONST.KING_CASTLE_STEPS[player][side][0])


	elif box == CONST.KNIGHT:
		for movement in CONST.KNIGHT_MOVES:
			newPosition, _ = positionCheck(state, position, movement, player)
			if newPosition:
				yield (position, newPosition)


	elif box == CONST.PAWN:
		for movement in CONST.PAWN_CAPTURE_MOVES:
			movement = CONST.MULTIPLYL(movement, CONST.PAWN_DIRECTION[player])
			newPosition, capture = positionCheck(state, position, movement, player)
			if newPosition:
				if capture:
					if not onlyFetchAttackSquares and (newPosition[0] == CONST.KING_LINE[CONST.OPPONENT[player]]):		# reach opponents king line
						yield (position, newPosition+(CONST.ROOK,))
						yield (position, newPosition+(CONST.BISHOP,))
						yield (position, newPosition+(CONST.KNIGHT,))
						yield (position, newPosition+(CONST.QUEEN,))
					else:
						yield (position, newPosition)
				else:
					if state["EN_PASSANT"][CONST.OPPONENT[player]] == newPosition[1]:
						if (CONST.KING_LINE[CONST.OPPONENT[player]] + ((1+2) * CONST.PAWN_DIRECTION[CONST.OPPONENT[player]][0])) == position[0]:
							if state["BOARD"][CONST.OPPONENT[player]][position[0]][newPosition[1]] == CONST.PAWN:
								yield (position, newPosition)

		if not onlyFetchAttackSquares:
			movement = CONST.MULTIPLYL(CONST.PAWN_NORMAL_MOVE, CONST.PAWN_DIRECTION[player])
			newPosition, capture = positionCheck(state, position, movement, player)
			if (not capture) and newPosition:
				if newPosition[0] == CONST.KING_LINE[CONST.OPPONENT[player]]:		# reach opponents king line
					yield (position, newPosition+(CONST.ROOK,))
					yield (position, newPosition+(CONST.BISHOP,))
					yield (position, newPosition+(CONST.KNIGHT,))
					yield (position, newPosition+(CONST.QUEEN,))
				else:
					yield (position, newPosition)
				
					if position[0] == (CONST.KING_LINE[player] + CONST.PAWN_DIRECTION[player][0]):		#check if pawn's first move
						movement = CONST.MULTIPLYL(CONST.PAWN_FIRST_MOVE, CONST.PAWN_DIRECTION[player])
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
	opponentPiece = newBoard[CONST.OPPONENT[player]][newPos[0]][newPos[1]]
	newBoard[CONST.OPPONENT[player]][newPos[0]][newPos[1]] = CONST.EMPTY
	
	newBoard[player][currentPos[0]][currentPos[1]] = CONST.EMPTY
	newBoard[player][newPos[0]][newPos[1]] = piece

	# reset en passant as only allowed immediately after
	newState["EN_PASSANT"][player] = -1

	if piece == CONST.KING:
		newState["CASTLING_AVAILABLE"][player][CONST.LEFT_CASTLE] = 0
		newState["CASTLING_AVAILABLE"][player][CONST.RIGHT_CASTLE] = 0

		if (currentPos[1] - newPos[1]) == 2:		#right castling
			newBoard[player][currentPos[0]][CONST.BOARD_SIZE - 1] = CONST.EMPTY
			newBoard[player][newPos[0]][newPos[1] - 1] = CONST.ROOK
		elif (currentPos[1] - newPos[1]) == -2:		#left castling
			newBoard[player][currentPos[0]][0] = CONST.EMPTY
			newBoard[player][newPos[0]][newPos[1] + 1] = CONST.ROOK
	
	elif piece == CONST.PAWN:
		pawnLine = (CONST.KING_LINE[player] + CONST.PAWN_DIRECTION[player][0])
		if newPos[0] == CONST.KING_LINE[CONST.OPPONENT[player]]:
			newBoard[player][newPos[0]][newPos[1]] = newPos[2]			#promotion
		elif currentPos[0] == pawnLine and abs(currentPos[0] - newPos[0]) > 1:
			newState["EN_PASSANT"][player] = newPos[1]
		elif opponentPiece == CONST.EMPTY:			#was en-passant
			newBoard[CONST.OPPONENT[player]][currentPos[0]][newPos[1]] = CONST.EMPTY
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


	newState["PLAYER"] = CONST.OPPONENT[newState["PLAYER"]]

	return newState

def clipSlideMoves(state, position, direction, stepSize, player):
	assert len(direction) == 2
	assert direction[0] in (0, 1, -1)
	assert direction[1] in (0, 1, -1)

	moves = []

	for i in range(1, stepSize + 1):
		movement = [direction[j]*i for j in range(len(direction))]
		pos = CONST.ADDL(position, movement)
		if max(pos) >= CONST.BOARD_SIZE or min(pos) < 0:
			break
		if state["BOARD"][player][pos[0]][pos[1]] != CONST.EMPTY:
			break

		if state["BOARD"][CONST.OPPONENT[player]][pos[0]][pos[1]] == CONST.EMPTY:
			moves.append((position, pos))
		else:
			moves.append((position, pos))
			break

	return moves

def positionCheck(state, position, movement, player):
	newPosition = CONST.ADDL(position, movement)

	if newPosition[0] < 0 or newPosition[0] >= CONST.BOARD_SIZE:
		return False, False
	if newPosition[1] < 0 or newPosition[1] >= CONST.BOARD_SIZE:
		return False, False

	if state["BOARD"][player][newPosition[0]][newPosition[1]] != CONST.EMPTY:
		return False, False

	capture = state["BOARD"][CONST.OPPONENT[player]][newPosition[0]][newPosition[1]]
	capture = False if capture == CONST.EMPTY else capture

	return newPosition, capture

def checkGameEnd(state, numActions, duration):
	reward = 0
	end = False
	if duration >= CONST.MAX_MOVES:
		end = "draw, moves exceeded!"
	elif not [box for playerBoard in state["BOARD"] for row in playerBoard for box in row if box not in (CONST.EMPTY, CONST.KING)]:
		end = "draw, only kings!"
	elif numActions == 0:
		if kingAttacked(state, state["PLAYER"]):
			winner = CONST.OPPONENT[state["PLAYER"]]
			reward = CONST.SCORING[winner]
			end = "winner : " + str(winner)
		else:
			end = "draw, stalemate!"
	
	return end, reward

def init():
	state = initializeGame()
	actions = allActions(state)
	end, reward = checkGameEnd(state, len(actions), 0)

	return state, actions, end, reward

def play(state, action, duration):
	nextState = performAction(state, action)
	actions = allActions(nextState)
	end, reward = checkGameEnd(nextState, len(actions), duration)

	return nextState, actions, end, reward

def printBoard(state):
	if CONST.DISPLAY_TEXT_BOARD:
		whitesBoard = state["BOARD"][CONST.WHITE_IDX]
		blacksBoard = state["BOARD"][CONST.BLACK_IDX]
		for i in range(CONST.BOARD_SIZE):
			row=[str(-blacksBoard[i][j] if blacksBoard[i][j] != CONST.EMPTY else whitesBoard[i][j]).rjust(2) for j in range(CONST.BOARD_SIZE)]
			print("  ".join(row))

	if CONST.DISPLAY_TK_BOARD:
		displayTk(state)

	print("current player:", state["PLAYER"])

def playGame():
	history = []

	state, actions, end, reward = init()
	while not end:
		action = actions[int(random.random()*len(actions))]
		nextState, actions, end, reward = play(state, action, len(history))

		history.append(dict(STATE=state, ACTION=action, NEXT_STATE=nextState, REWARD=reward))
		state = nextState

	print(end, len(history))
	printBoard(state)
	return history

def displayTk(state):
	root = tk.Tk()
	root.title("game")
	root.geometry(str(CONST.BOARD_SIZE*CONST.IMAGE_SIZE)+"x"+str(CONST.BOARD_SIZE*CONST.IMAGE_SIZE))
	root.resizable(0, 0)
	frame = tk.Frame(root)
	frame.pack()
	canvas = tk.Canvas(frame, bg="black", width=CONST.BOARD_SIZE*CONST.IMAGE_SIZE, height=CONST.BOARD_SIZE*CONST.IMAGE_SIZE)
	canvas.pack()

	boardImgTk = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "board.png"))
	attackImgTk = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "attack.png"))
	moveImgTk = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "move.png"))
	pieceImagesTk = {CONST.WHITE_IDX:{}, CONST.BLACK_IDX:{}}
	for player in [CONST.WHITE_IDX, CONST.BLACK_IDX]:
		pieceImagesTk[player][CONST.PAWN] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "pawn_" + str(player) + ".png"))
		pieceImagesTk[player][CONST.BISHOP] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "bishop_" + str(player) + ".png"))
		pieceImagesTk[player][CONST.KNIGHT] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "knight_" + str(player) + ".png"))
		pieceImagesTk[player][CONST.ROOK] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "rook_" + str(player) + ".png"))
		pieceImagesTk[player][CONST.QUEEN] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "queen_" + str(player) + ".png"))
		pieceImagesTk[player][CONST.KING] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "king_" + str(player) + ".png"))


	def onClick(event, state, moveImgTk):
		canvas = event.widget
		canvas.delete("move")

		j = event.x // CONST.IMAGE_SIZE
		i = event.y // CONST.IMAGE_SIZE

		for _, box in positionAllowedMoves(state, (i,j)):
			canvas.create_image(box[1]*CONST.IMAGE_SIZE, box[0]*CONST.IMAGE_SIZE, image=moveImgTk, anchor=tk.NW, tags="move")

	canvas.tag_bind("piece", "<ButtonPress-1>", lambda event, state=state, moveImgTk=moveImgTk: onClick(event, state, moveImgTk))
	canvas.tag_bind("board", "<ButtonPress-1>", lambda event, state=state, moveImgTk=moveImgTk: onClick(event, state, moveImgTk))
	canvas.tag_bind("move", "<ButtonPress-1>", lambda event, state=state, moveImgTk=moveImgTk: onClick(event, state, moveImgTk))

	# create board
	canvas.create_image(0, 0, image=boardImgTk, anchor=tk.NW, tags="board")

	# place pieces
	for player in [CONST.WHITE_IDX, CONST.BLACK_IDX]:
		for i in range(CONST.BOARD_SIZE):
			for j in range(CONST.BOARD_SIZE):
				box = state["BOARD"][player][i][j]
				if box != CONST.EMPTY:
					canvas.create_image(j*CONST.IMAGE_SIZE, i*CONST.IMAGE_SIZE, image=pieceImagesTk[player][box], anchor=tk.NW, tags="piece")

	root.mainloop()

if __name__ == "__main__":
	playGame()
