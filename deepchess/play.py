import random
from . import constants as CONST
import tkinter as tk
from PIL import Image, ImageTk
from . import engine as EG

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

	state, actions, end, reward = EG.init()
	while not end:
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

		for _, box in EG.positionAllowedMoves(state, (i,j)):
			canvas.create_image(box[1]*CONST.IMAGE_SIZE, box[0]*CONST.IMAGE_SIZE, image=moveImgTk, anchor=tk.NW, tags=("move","piece"))

	canvas.tag_bind("piece", "<ButtonPress-1>", lambda event, state=state, moveImgTk=moveImgTk: onClick(event, state, moveImgTk))
	canvas.tag_bind("board", "<ButtonPress-1>", lambda event, state=state, moveImgTk=moveImgTk: onClick(event, state, moveImgTk))

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

