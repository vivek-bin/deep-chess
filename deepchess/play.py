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

moveList = []

def printBoard(state):
	if CONST.DISPLAY_TEXT_BOARD:
		whitesBoard = state["BOARD"][EG.WHITE_IDX]
		blacksBoard = state["BOARD"][EG.BLACK_IDX]
		for i in range(EG.BOARD_SIZE):
			row=[str(-blacksBoard[i][j] if blacksBoard[i][j] != EG.EMPTY else whitesBoard[i][j]).rjust(2) for j in range(EG.BOARD_SIZE)]
			print("  ".join(row))

	if CONST.DISPLAY_TK_BOARD:
		displayTk(state)

	print("current player:", state["PLAYER"])

def playGame():
	global moveList
	history = []

	state, actions, end, reward = EG.init()
	while not end:
		if moveList:
			action = moveList[0]
			moveList.pop(0)
		else:
			action = actions[int(random.random()*len(actions))]
		nextState, actions, end, reward = EG.play(state, action, len(history))

		history.append(dict(STATE=state, ACTION=action, NEXT_STATE=nextState, REWARD=reward))
		state = nextState
		printBoard(state)

	print(end, len(history))
	printBoard(state)
	return end, history

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
	for player in [EG.WHITE_IDX, EG.BLACK_IDX]:
		pieceImagesTk[player][EG.PAWN] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "pawn_" + str(player) + ".png"))
		pieceImagesTk[player][EG.BISHOP] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "bishop_" + str(player) + ".png"))
		pieceImagesTk[player][EG.KNIGHT] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "knight_" + str(player) + ".png"))
		pieceImagesTk[player][EG.ROOK] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "rook_" + str(player) + ".png"))
		pieceImagesTk[player][EG.QUEEN] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "queen_" + str(player) + ".png"))
		pieceImagesTk[player][EG.KING] = ImageTk.PhotoImage(Image.open(CONST.IMAGES + "king_" + str(player) + ".png"))


	def onClick(event, state, moveImgTk):
		canvas = event.widget
		canvas.delete("move")

		j = event.x // CONST.IMAGE_SIZE
		i = event.y // CONST.IMAGE_SIZE

		for _, box in EG.positionAllowedMoves(state, (i,j)):
			canvas.create_image(box[1]*CONST.IMAGE_SIZE, box[0]*CONST.IMAGE_SIZE, image=moveImgTk, anchor=tk.NW, tags=("move", str(i)+","+str(j)))

	canvas.tag_bind("piece", "<ButtonPress-1>", lambda event, state=state, moveImgTk=moveImgTk: onClick(event, state, moveImgTk))
	canvas.tag_bind("board", "<ButtonPress-1>", lambda event, state=state, moveImgTk=moveImgTk: onClick(event, state, moveImgTk))

	def onClickMove(event, state, root):
		global moveList

		canvas = event.widget
		ids = canvas.find_overlapping(event.x-1, event.y-1, event.x+1, event.y+1)
		
		ids = [id for id in ids if "move" in canvas.gettags(id)]
		moveId = ids[0]

		originalBox = [tag for tag in list(canvas.gettags(moveId)) if "," in tag][0]
		originalBox = [int(x) for x in originalBox.split(",")]

		finalBox = [event.y // CONST.IMAGE_SIZE, event.x // CONST.IMAGE_SIZE]

		move = (originalBox, finalBox)
		moveList.append(move)
		root.destroy()

	canvas.tag_bind("move", "<ButtonPress-1>", lambda event, state=state, root=root: onClickMove(event, state, root))

	# create board
	canvas.create_image(0, 0, image=boardImgTk, anchor=tk.NW, tags="board")

	# place pieces
	for player in [EG.WHITE_IDX, EG.BLACK_IDX]:
		for i in range(EG.BOARD_SIZE):
			for j in range(EG.BOARD_SIZE):
				box = state["BOARD"][player][i][j]
				if box != EG.EMPTY:
					canvas.create_image(j*CONST.IMAGE_SIZE, i*CONST.IMAGE_SIZE, image=pieceImagesTk[player][box], anchor=tk.NW, tags="piece")

	root.mainloop()

if __name__ == "__main__":
	playGame()

