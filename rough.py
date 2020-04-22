
from deepchess import constants as CONST
def testActionIndexing():
	if CONST.ENGINE_TYPE == "PY":
		from deepchess import engine as EG
	elif CONST.ENGINE_TYPE == "C":
		import cengine as EG
	else:
		raise ImportError
	idxmax = []
	idxDict = {}
	for i in range(EG.BOARD_SIZE):
		for j in range(EG.BOARD_SIZE):
			for k in range(EG.BOARD_SIZE):
				for l in range(EG.BOARD_SIZE):
					move = ((i,j), (k,l))
					idx = EG.actionIndex(move)
					((a,b), (c,d)) = EG.actionFromIndex(idx)
					if a!=i or b!=j or c!=k or d!=l:
						print(i,j,k,l, a, b, c, d)
					idxmax.append(idx)
					try:
						idxDict[idx] = idxDict[idx] + 1
					except KeyError:
						idxDict[idx] = 1
	for i in range(EG.BOARD_SIZE**4):
		move = EG.actionFromIndex(i)
		if i!= EG.actionIndex(move):
			print(i, move)

	for i in range(EG.BOARD_SIZE):
		for j in range(EG.BOARD_SIZE):
			for p in range(min(EG.PROMOTIONS), max(EG.PROMOTIONS) + 1):
				move = ((1,i), (0,j,p))
				idx = EG.actionIndex(move)
				((a,b), (c,d, e)) = EG.actionFromIndex(idx)
				if a!=1 or b!=i or c!=0 or d!=j or e!=p:
					print(1, i,0, j, p, "---" , a, b, c, d, e)
				idxmax.append(idx)
				try:
					idxDict[idx] = idxDict[idx] + 1
				except KeyError:
					idxDict[idx] = 1

	for i in range(EG.BOARD_SIZE):
		for j in range(EG.BOARD_SIZE):
			for p in range(min(EG.PROMOTIONS), max(EG.PROMOTIONS) + 1):
				move = ((6,i), (7,j,p))
				idx = EG.actionIndex(move)
				((a,b), (c,d, e)) = EG.actionFromIndex(idx)
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
	print(EG.actionIndex(((EG.BOARD_SIZE - 2,EG.BOARD_SIZE - 1), (EG.BOARD_SIZE - 1,EG.BOARD_SIZE - 1,EG.PROMOTIONS[-1]))))
	print(max(idxmax))
	print(min(idxmax))

def testStateIndexing():
	if CONST.ENGINE_TYPE == "PY":
		from deepchess import engine as EG
	elif CONST.ENGINE_TYPE == "C":
		import cengine as EG
	else:
		raise ImportError
	from random import random, choice
	state = EG.init()[0]
	for player in [EG.WHITE_IDX, EG.BLACK_IDX]:
		for castle in range(16):
			state["PLAYER"] = player
			state["CASTLING_AVAILABLE"][EG.WHITE_IDX][EG.LEFT_CASTLE] = (castle&1)>>0
			state["CASTLING_AVAILABLE"][EG.WHITE_IDX][EG.RIGHT_CASTLE] = (castle&2)>>1
			state["CASTLING_AVAILABLE"][EG.BLACK_IDX][EG.LEFT_CASTLE] = (castle&4)>>2
			state["CASTLING_AVAILABLE"][EG.BLACK_IDX][EG.RIGHT_CASTLE] = (castle&8)>>3
			for enPass in list(range(8))+[-1]:
				state["EN_PASSANT"][EG.OPPONENT[player]] = enPass
				for _ in range(10000):
					p = int(random()*2)
					c = int(random()*EG.BOARD_SIZE)
					r = int(random()*EG.BOARD_SIZE)
					piece = choice([EG.KING, EG.QUEEN, EG.ROOK, EG.KNIGHT, EG.BISHOP, EG.PAWN, EG.EMPTY])
					state["BOARD"][p][c][r] = piece

					if EG.stateIndex(state) != EG.stateIndex(EG.stateFromIndex(EG.stateIndex(state))):
						print("next state index ----")
						print("state index ----")
						print(state)
						print(EG.stateIndex(state).encode())
						print(EG.stateFromIndex(EG.stateIndex(state)))
					if state != EG.stateFromIndex(EG.stateIndex(state)):
						print("state from index ----")
						print(state)
						print(EG.stateIndex(state).encode())
						print(EG.stateFromIndex(EG.stateIndex(state)))
	
def checkPyCEngineSame():
	from deepchess import engine as EGP
	import cengine as EGC
	import random

	state, actions, end, reward = EGP.init()
	stateC, actionsC, endC, rewardC = EGC.init()
	
	while not end and not endC:
		action = actions[int(random.random()*len(actions))]
		assert action in actionsC
		
		state, actions, end, reward = EGP.play(state, action, 0)
		stateC, actionsC, endC, rewardC = EGC.play(stateC, action, 0)

		if (end, reward) != (endC, rewardC):
			print("end reward not same")
			print((end, reward))
			print((endC, rewardC))
		if [a for a in actions if a not in actionsC] or [a for a in actionsC if a not in actions]:
			print("actions not same")
			if state != stateC:
				print("states not same")
			print(tuple([a for a in actions if a not in actionsC]))
			print(tuple([a for a in actionsC if a not in actions]))
			print(stateC)
		if state != stateC:
			print("state not same")
			if state["BOARD"] != stateC["BOARD"]:
				print(state["BOARD"])
				print(stateC["BOARD"])
			if state["CASTLING_AVAILABLE"] != stateC["CASTLING_AVAILABLE"]:
				print(state["CASTLING_AVAILABLE"])
				print(stateC["CASTLING_AVAILABLE"])
			if state["EN_PASSANT"] != stateC["EN_PASSANT"]:
				print(state["EN_PASSANT"])
				print(stateC["EN_PASSANT"])
			if state["PLAYER"] != stateC["PLAYER"]:
				print(state["PLAYER"])
				print(stateC["PLAYER"])
			break

def memoryLeak():
	if CONST.ENGINE_TYPE == "PY":
		from deepchess import engine as EG
	elif CONST.ENGINE_TYPE == "C":
		import cengine as EG
	else:
		raise ImportError
	import psutil
	import guppy
	h = guppy.hpy()


	#state, actions, end, reward = EG.init()
	bef = h.heap()
	print("total memory used(kB) : ", psutil.virtual_memory().used/1e6, "  ,", psutil.virtual_memory().percent, "%")

	for i in range(int(1e5)):
		#nextState, actions, end, reward = EG.play(state, actions[1], 0)
		#x = EG.play(state, actions[1], 0)
		_ = EG.init()
	#print(h.heap()-bef)
	print("total memory used(kB) : ", psutil.virtual_memory().used/1e6, "  ,", psutil.virtual_memory().percent, "%")

def testStringC_API():
	if CONST.ENGINE_TYPE == "PY":
		from deepchess import engine as EG
	elif CONST.ENGINE_TYPE == "C":
		import cengine as EG
	else:
		raise ImportError
	from random import random, choice
	state1, actions, _, _ = EG.init()
	state2, actions, _, _ = EG.play(state1, actions[0], 0)

	idx1 = EG.stateIndex(state1)
	print(idx1.encode())
	idx2 = EG.stateIndex(state2)
	print(idx1.encode())
	print(idx2.encode())
	
def testCSearch():
	import deepchess.constants as CONST
	import deepchess.trainmodel as TM
	if CONST.ENGINE_TYPE == "PY":
		from deepchess import engine as EG
	elif CONST.ENGINE_TYPE == "C":
		import cengine as EG
	else:
		raise ImportError
	if CONST.ENGINE_TYPE == "PY":
		from deepchess import search as SE
	elif CONST.ENGINE_TYPE == "C":
		import csearch as SE
	else:
		raise ImportError


	class Check:
		name = "checkcheck"
		shape = (1,2)
		def predict(self, x, y):
			print("aaaaaaaaaaaaaa", x[0].shape)
			print(x[0])
			print(x[0].__class__)
			return [Check(),Check()]
		def predict2(self, ip):
			print(len(ip))
			print(ip[0].shape)
			print(ip[1].shape)
			print(ip[0])
			print(ip[1])

	state, actions, end, reward = EG.init()
	model = TM.loadModel(loadForTraining=False)
	#model = Check()
	

	#v = SE.test(model)
	root = SE.initTree(state, actions, end, reward, [], model, CONST.DATA)
	root, action = SE.searchTree(root)
	
def numpyInputCheck():
	import numpy as np
	import copy

	import deepchess.constants as CONST
	import deepchess.trainmodel as TM
	import cengine as EG
	
	from deepchess import search as SEP
	import csearch as SEC

	playIps = [[]]

	def predictor(ip):
		playIps[-1].append(copy.deepcopy(ip))
		b = ip[0].shape[0]

		return [np.zeros((b, 1)), np.ones((b, EG.MAX_POSSIBLE_MOVES))*0.1]

	def displayBatch(ip, bIdx):
		boards = ip[0][bIdx]
		other = ip[1][bIdx]
		
		for r in range(EG.BOARD_SIZE):
			opRow = []
			for h in range(SEC.BOARD_HISTORY):
				for p in range(2):
					for c in range(EG.BOARD_SIZE):
						opRow.append(str(boards[h*2+p][r][c]))
					opRow.append("  ")
				opRow.append("||  ")
			print(" ".join(opRow))
		print("-"*70)
		for r in range(EG.BOARD_SIZE):
			opRow = []
			for o in range(3):
				for c in range(EG.BOARD_SIZE):
					opRow.append(str(other[o][r][c]))
				opRow.append("  ")
			print(" ".join(opRow))

		print("")

	def displayBatchPyC(ips, simulIdx, bIdx):
		print("python")
		displayBatch(ips[simulIdx*2+0], bIdx)
		print("c")
		displayBatch(ips[simulIdx*2+1], bIdx)
		print("\n")


	mp = SEP.allocNpMemory()
	statep, actionsp, endp, rewardp = EG.init()
	rootp = SEP.initTree(statep, actionsp, endp, rewardp, [], predictor, CONST.DATA, False)

	mc = SEC.allocNpMemory()
	statec, actionsc, endc, rewardc = EG.init()
	rootc = SEC.initTree(statec, actionsc, endc, rewardc, [], predictor, CONST.DATA, False)
	
	for i in range(10):
		playIps.append([])
		rootp, action = SEP.searchTree(rootp)
		rootc, action = SEC.searchTree(rootc)


	displayBatchPyC(playIps[-2], 0, 0)
	displayBatchPyC(playIps[-2], 1, 0)
	displayBatchPyC(playIps[-2], 2, 0)

	for i in range(2):
		displayBatchPyC(playIps[-1], i, 0)


def nodeClearingDebug():
	import numpy as np
	import copy

	import deepchess.constants as CONST
	import deepchess.trainmodel as TM
	import cengine as EG
	
	from deepchess import search as SEP
	import csearch as SEC

	playIps = [[]]

	def predictor(ip):
		playIps[-1].append(copy.deepcopy(ip))
		b = ip[0].shape[0]

		return [np.zeros((b, 1)), np.ones((b, EG.MAX_POSSIBLE_MOVES))*0.1]

	def displayBatch(ip, bIdx):
		boards = ip[0][bIdx]
		other = ip[1][bIdx]
		
		for r in range(EG.BOARD_SIZE):
			opRow = []
			for h in range(SEC.BOARD_HISTORY):
				for p in range(2):
					for c in range(EG.BOARD_SIZE):
						opRow.append(str(boards[h*2+p][r][c]))
					opRow.append("  ")
				opRow.append("||  ")
			print(" ".join(opRow))
		print("-"*70)
		for r in range(EG.BOARD_SIZE):
			opRow = []
			for o in range(3):
				for c in range(EG.BOARD_SIZE):
					opRow.append(str(other[o][r][c]))
				opRow.append("  ")
			print(" ".join(opRow))

		print("")

	def displayBatchPyC(ips, simulIdx, bIdx):
		displayBatch(ips[simulIdx], bIdx)
		print("\n")

	mc = SEC.allocNpMemory()
	statec, actionsc, endc, rewardc = EG.init()
	rootc = SEC.initTree(statec, actionsc, endc, rewardc, [], predictor, CONST.DATA, False)
	
	for i in range(17):
		playIps.append([])
		rootc, action = SEC.searchTree(rootc)

	for i in range(2):
		displayBatchPyC(playIps[-1], i, 0)


def checkNumpyAccessInC():
	import csearch as SE
	import numpy as np

	b = 5
	i = 3
	v = np.reshape(np.arange(0,b *2, 2, dtype="float32"), (b,1))
	p = np.reshape(np.arange(0,b*i *4, 4, dtype="float32"), (b, i))

	SE.test([v, p])

	print(v)
	print(p)


print(CONST.LAPSED_TIME())
checkNumpyAccessInC()
print(CONST.LAPSED_TIME())