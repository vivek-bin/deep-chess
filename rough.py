
from deepchess import constants as CONST
def testActionIndexing():
	from deepchess import engine
	from deepchess import search
	idxmax = []
	idxDict = {}
	for i in range(CONST.BOARD_SIZE):
		for j in range(CONST.BOARD_SIZE):
			for k in range(CONST.BOARD_SIZE):
				for l in range(CONST.BOARD_SIZE):
					move = ((i,j), (k,l))
					idx = search.actionIndex(move)
					((a,b), (c,d)) = search.actionFromIndex(idx)
					if a!=i or b!=j or c!=k or d!=l:
						print(i,j,k,l, a, b, c, d)
					idxmax.append(idx)
					try:
						idxDict[idx] = idxDict[idx] + 1
					except KeyError:
						idxDict[idx] = 1
	for i in range(CONST.BOARD_SIZE**4):
		move = search.actionFromIndex(i)
		if i!= search.actionIndex(move):
			print(i, move)

	for i in range(CONST.BOARD_SIZE):
		for j in range(CONST.BOARD_SIZE):
			for p in range(min(CONST.PROMOTIONS), max(CONST.PROMOTIONS) + 1):
				move = ((1,i), (0,j,p))
				idx = search.actionIndex(move)
				((a,b), (c,d, e)) = search.actionFromIndex(idx)
				if a!=1 or b!=i or c!=0 or d!=j or e!=p:
					print(1, i,0, j, p, "---" , a, b, c, d, e)
				idxmax.append(idx)
				try:
					idxDict[idx] = idxDict[idx] + 1
				except KeyError:
					idxDict[idx] = 1

	for i in range(CONST.BOARD_SIZE):
		for j in range(CONST.BOARD_SIZE):
			for p in range(min(CONST.PROMOTIONS), max(CONST.PROMOTIONS) + 1):
				move = ((6,i), (7,j,p))
				idx = search.actionIndex(move)
				((a,b), (c,d, e)) = search.actionFromIndex(idx)
				if a!=6 or b!=i or c!=7 or d!=j or e!=p:
					print(6, i,7, j,p ,"---", a, b, c, d, e)
				idxmax.append(idx)
				try:
					idxDict[idx] = idxDict[idx] + 1
				except KeyError:
					idxDict[idx] = 1
	for key, value in idxDict.items():
		if value > 1:
			print(key)
	idxDict = {}
	print(search.actionIndex(((CONST.BOARD_SIZE - 2,CONST.BOARD_SIZE - 1), (CONST.BOARD_SIZE - 1,CONST.BOARD_SIZE - 1,CONST.PROMOTIONS[-1]))))
	print(max(idxmax))
	print(min(idxmax))

def testStateIndexing():
	from deepchess import engine
	from deepchess import search
	from random import random, choice
	state = engine.initializeGame()
	for player in [CONST.WHITE_IDX, CONST.BLACK_IDX]:
		for castle in range(16):
			state["PLAYER"] = player
			state["CASTLING_AVAILABLE"][CONST.WHITE_IDX][CONST.LEFT_CASTLE] = (castle&1)>>0
			state["CASTLING_AVAILABLE"][CONST.WHITE_IDX][CONST.RIGHT_CASTLE] = (castle&2)>>1
			state["CASTLING_AVAILABLE"][CONST.BLACK_IDX][CONST.LEFT_CASTLE] = (castle&4)>>2
			state["CASTLING_AVAILABLE"][CONST.BLACK_IDX][CONST.RIGHT_CASTLE] = (castle&8)>>3
			for enPass in list(range(8))+[-1]:
				state["EN_PASSANT"][CONST.OPPONENT[player]] = enPass
				for _ in range(1000):
					p = int(random()*2)
					c = int(random()*CONST.BOARD_SIZE)
					r = int(random()*CONST.BOARD_SIZE)
					piece = choice([CONST.KING, CONST.QUEEN, CONST.ROOK, CONST.KNIGHT, CONST.BISHOP, CONST.PAWN, CONST.EMPTY])
					state["BOARD"][p][c][r] = piece

					if search.stateIndex(state) != search.stateIndex(search.stateFromIndex(search.stateIndex(state))):
						print(state)
						print(search.stateFromIndex(search.stateIndex(state)))
					if state != search.stateFromIndex(search.stateIndex(state)):
						print(state)
						print(search.stateFromIndex(search.stateIndex(state)))
	


testActionIndexing()
testStateIndexing()
print(CONST.LAPSED_TIME())