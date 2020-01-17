def testIndexing():
	from deepchess import constants as CONST
	from deepchess import engine
	idxmax = []
	idxDict = {}
	for i in range(CONST.BOARD_SIZE):
		for j in range(CONST.BOARD_SIZE):
			for k in range(CONST.BOARD_SIZE):
				for l in range(CONST.BOARD_SIZE):
					move = ((i,j), (k,l))
					idx = engine.moveIndex(move)
					((a,b), (c,d)) = engine.moveFromIndex(idx)
					if a!=i or b!=j or c!=k or d!=l:
						print(i,j,k,l, a, b, c, d)
					idxmax.append(idx)
					try:
						idxDict[idx] = idxDict[idx] + 1
					except KeyError:
						idxDict[idx] = 1
	for i in range(CONST.BOARD_SIZE**4):
		move = engine.moveFromIndex(i)
		if i!= engine.moveIndex(move):
			print(i, move)

	for i in range(CONST.BOARD_SIZE):
		for j in range(CONST.BOARD_SIZE):
			for p in range(min(CONST.PROMOTIONS), max(CONST.PROMOTIONS) + 1):
				move = ((1,i), (0,j,p))
				idx = engine.moveIndex(move)
				((a,b), (c,d, e)) = engine.moveFromIndex(idx)
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
				idx = engine.moveIndex(move)
				((a,b), (c,d, e)) = engine.moveFromIndex(idx)
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
	print(engine.moveIndex(((CONST.BOARD_SIZE - 2,CONST.BOARD_SIZE - 1), (CONST.BOARD_SIZE - 1,CONST.BOARD_SIZE - 1,CONST.PROMOTIONS[-1]))))
	print(max(idxmax))
	print(min(idxmax))

testIndexing()