import random
from . import constants as CONST
import tkinter as tk
from PIL import Image, ImageTk
if CONST.ENGINE_TYPE == "PY":
	from . import engine as EG
elif CONST.ENGINE_TYPE == "C":
	import cengine as EG
else:
	raise ImportError
if CONST.SEARCH_TYPE == "PY":
	from . import search as SE
elif CONST.SEARCH_TYPE == "C":
	import csearch as SE
else:
	raise ImportError
from . import trainmodel as TM
import copy
import gc
import psutil
import numpy as np

moveList = []

def printBoard(state, duration, end):
	if CONST.DISPLAY_TEXT_BOARD:
		whitesBoard = state["BOARD"][EG.WHITE_IDX]
		blacksBoard = state["BOARD"][EG.BLACK_IDX]
		for i in range(EG.BOARD_SIZE):
			row=[str(-blacksBoard[i][j] if blacksBoard[i][j] != EG.EMPTY else whitesBoard[i][j]).rjust(2) for j in range(EG.BOARD_SIZE)]
			print("  ".join(row))

	if CONST.DISPLAY_TK_BOARD:
		displayTk(state)
	print("Player:", state["PLAYER"], " "*15, "Move No:", duration, " "*15, "Result:", end)
	print("total memory used(kB) : ", psutil.virtual_memory().used/1000, "  ,", psutil.virtual_memory().percent, "%")
	print(CONST.LAPSED_TIME())

def displayTk(state):
	root = tk.Tk()
	root.title("game")
	root.geometry(str(EG.BOARD_SIZE*CONST.IMAGE_SIZE)+"x"+str(EG.BOARD_SIZE*CONST.IMAGE_SIZE))
	root.resizable(0, 0)
	frame = tk.Frame(root)
	frame.pack()
	canvas = tk.Canvas(frame, bg="black", width=EG.BOARD_SIZE*CONST.IMAGE_SIZE, height=EG.BOARD_SIZE*CONST.IMAGE_SIZE)
	canvas.pack()

	boardImgTk = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "board.png"))
	attackImgTk = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "attack.png"))
	moveImgTk = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "move.png"))
	pieceImagesTk = {EG.WHITE_IDX:{}, EG.BLACK_IDX:{}}
	promotionImagesTk = {EG.WHITE_IDX:{}, EG.BLACK_IDX:{}}
	for player in [EG.WHITE_IDX, EG.BLACK_IDX]:
		pieceImagesTk[player][EG.PAWN] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "pawn_" + str(player) + ".png"))
		pieceImagesTk[player][EG.BISHOP] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "bishop_" + str(player) + ".png"))
		pieceImagesTk[player][EG.KNIGHT] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "knight_" + str(player) + ".png"))
		pieceImagesTk[player][EG.ROOK] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "rook_" + str(player) + ".png"))
		pieceImagesTk[player][EG.QUEEN] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "queen_" + str(player) + ".png"))
		pieceImagesTk[player][EG.KING] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "king_" + str(player) + ".png"))

		promotionImagesTk[player][EG.BISHOP] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "pro_bishop_" + str(player) + ".png"))
		promotionImagesTk[player][EG.KNIGHT] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "pro_knight_" + str(player) + ".png"))
		promotionImagesTk[player][EG.ROOK] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "pro_rook_" + str(player) + ".png"))
		promotionImagesTk[player][EG.QUEEN] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "pro_queen_" + str(player) + ".png"))


	def onClick(event, state, moveImgTk, promotionImagesTk):
		canvas = event.widget
		canvas.delete("move")

		j = event.x // CONST.IMAGE_SIZE
		i = event.y // CONST.IMAGE_SIZE

		for startBox, endBox in EG.positionAllowedMoves(state, (i,j)):
			moveStr = ",".join([str(idx) for idx in (startBox + endBox)])
			if len(endBox)==2:
				canvas.create_image(endBox[1]*CONST.IMAGE_SIZE, endBox[0]*CONST.IMAGE_SIZE, image=moveImgTk, anchor=tk.NW, tags=("move", moveStr))
			elif len(endBox)==3:
				xOffset = CONST.IMAGE_SIZE//2 if endBox[2] in (EG.BISHOP, EG.KNIGHT) else 0
				yOffset = CONST.IMAGE_SIZE//2 if endBox[2] in (EG.ROOK, EG.KNIGHT) else 0
				canvas.create_image(endBox[1]*CONST.IMAGE_SIZE + xOffset, endBox[0]*CONST.IMAGE_SIZE + yOffset, image=promotionImagesTk[state["PLAYER"]][endBox[2]], anchor=tk.NW, tags=("move", moveStr))
			else:
				print("too many values in end position!")

	canvas.tag_bind("piece", "<ButtonPress-1>", lambda event, state=state, moveImgTk=moveImgTk, promotionImagesTk=promotionImagesTk: onClick(event, state, moveImgTk, promotionImagesTk))
	canvas.tag_bind("board", "<ButtonPress-1>", lambda event, state=state, moveImgTk=moveImgTk, promotionImagesTk=promotionImagesTk: onClick(event, state, moveImgTk, promotionImagesTk))

	def onClickMove(event, state, root):
		global moveList

		canvas = event.widget
		ids = canvas.find_overlapping(event.x-1, event.y-1, event.x+1, event.y+1)
		
		ids = [id for id in ids if "move" in canvas.gettags(id)]
		moveId = ids[0]

		moveTag = [tag for tag in list(canvas.gettags(moveId)) if "," in tag][0]
		moveIdx = tuple(int(x) for x in moveTag.split(","))
		move = (moveIdx[:2], moveIdx[2:])
		moveList.append(move)
		root.destroy()

	canvas.tag_bind("move", "<ButtonPress-1>", lambda event, state=state, root=root: onClickMove(event, state, root))

	# create board
	canvas.create_image(0, 0, image=boardImgTk, anchor=tk.NW, tags="board")

	kingUnderCheck = EG.kingAttacked(state)
	if kingUnderCheck:
		canvas.create_image(kingUnderCheck[1]*CONST.IMAGE_SIZE, kingUnderCheck[0]*CONST.IMAGE_SIZE, image=attackImgTk, anchor=tk.NW)

	# place pieces
	for player in [EG.WHITE_IDX, EG.BLACK_IDX]:
		for i in range(EG.BOARD_SIZE):
			for j in range(EG.BOARD_SIZE):
				box = state["BOARD"][player][i][j]
				if box != EG.EMPTY:
					canvas.create_image(j*CONST.IMAGE_SIZE, i*CONST.IMAGE_SIZE, image=pieceImagesTk[player][box], anchor=tk.NW, tags="piece")

	root.mainloop()


def playGame():
	global moveList
	history = []
	root = None

	state, actions, end, reward = EG.init()
	if CONST.SHOW_BOARDS:
		printBoard(state, 0, False)
	if CONST.MC_SEARCH_MOVE:
		model = TM.loadModel(loadForTraining=False)
		predictor = lambda x:model.predict(x, batch_size=CONST.PREDICTION_BATCH_SIZE)
	
	while not end:
		if moveList:
			action = moveList.pop(0)
			root = None
		elif CONST.MC_SEARCH_MOVE:
			if root is None:
				print("creating new tree")
				root = SE.initTree(state, actions, end, reward, history, predictor, CONST.DATA, True)
				
			root, action = SE.searchTree(root)
		else:
			action = actions[int(random.random()*len(actions))]
			root = None
		
		gc.collect()

		nextState, actions, end, reward = EG.play(state, action, len(history)+1)

		history.append(dict(STATE=state, ACTION=action, NEXT_STATE=nextState, REWARD=reward))
		state = nextState
		if CONST.SHOW_BOARDS:
			printBoard(state, len(history), end)

	return end, history

def compareModels(firstIdx, secondIdx, count):
	modelFirst = TM.loadModel(loadForTraining=False, idx=firstIdx)
	modelSecond = TM.loadModel(loadForTraining=False, idx=secondIdx)
	predictorFirst = lambda x:modelFirst.predict(x, batch_size=CONST.PREDICTION_BATCH_SIZE)
	predictorSecond = lambda x:modelSecond.predict(x, batch_size=CONST.PREDICTION_BATCH_SIZE)
	
	state, actions, end, reward = EG.init()
	states = [state for _ in range(count)]
	histories = [[] for _ in range(count)]

	rootsFirst = [SE.initTree(state, actions, end, reward, [], predictorFirst, CONST.DATA, True) for _ in range(count)]
	rootsSecond = [SE.initTree(state, actions, end, reward, [], predictorSecond, CONST.DATA, True) for _ in range(count)]
	roots = [rootsFirst, rootsSecond]
	
	pIdx, oIdx = 0, 1
	while not end:
		results = SE.searchTree(roots[pIdx])
		for i, result in enumerate(results):
			roots[pIdx][i] = result[0]
			action = result[1]
			if(action):
				roots[oIdx][i] = SE.playMoveOnTree(roots[oIdx][i], action)

				nextState, actions, end, reward = EG.play(states[i], action, len(histories[i])+1)
				histories[i].append(dict(STATE=states[i], ACTION=action, NEXT_STATE=nextState, REWARD=reward, END=end))
				states[i] = nextState
		
		pIdx, oIdx = oIdx, pIdx
	
	for state, history in zip(states, histories):
		printBoard(state, len(history), end)
	
	return [str(history[-1]["REWARD"]) for history in histories]


def generateGames():
	assert CONST.SEARCH_TYPE == "C"

	model = TM.loadModel(loadForTraining=False)
	predictor = lambda x:model.predict(x, batch_size=CONST.PREDICTION_BATCH_SIZE)		# predict from model

	state, actions, end, reward = EG.init()

	SE.generateGames(state, actions, end, reward, [], predictor, CONST.DATA, True)



if __name__ == "__main__":
	playGame()

