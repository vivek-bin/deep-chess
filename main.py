import sys
import os
from deepchess import constants as CONST

VALID_FLAGS = ["--train", "--play", "--generate", "--compare", "--iterate", "--view"]

def main():
	flags = sys.argv[1:]
	trainModelFlag = False
	playFlag = False
	generateFlag = False
	compareModelsFlag = False
	iterationFlag = False
	viewFlag = False

	try:
		if flags[0] not in VALID_FLAGS:
			print("Invalid flag. Valid flags are "+", ".join(VALID_FLAGS))
			return False
		if "--train" == flags[0]:
			trainModelFlag = True
		if "--play" == flags[0]:
			playFlag = True
		if "--generate" == flags[0]:
			generateFlag = True
		if "--compare" == flags[0]:
			compareModelsFlag = True
		if "--iterate" == flags[0]:
			iterationFlag = True
		if "--view" == flags[0]:
			viewFlag = True
		flags.pop(0)
	except IndexError:
		playFlag = True
	
	if trainModelFlag:
		from deepchess.trainmodel import trainModel
		trainModel()

	elif playFlag:
		from deepchess.play import playGame
		for _ in range(10):
			end, history = playGame()
			print(CONST.LAPSED_TIME())

	elif compareModelsFlag:
		from deepchess.play import compareModels

		rewards = compareModels(firstIdx=-1, secondIdx=-3, count=16)
		print("latest model as white, results: ", ", ".join(rewards))

		rewards = compareModels(firstIdx=-3, secondIdx=-1, count=16)
		print("latest model as black, results: ", ", ".join(rewards))

		print(CONST.LAPSED_TIME())

	elif generateFlag:
		from deepchess.play import generateGames
		generateGames()
		print(CONST.LAPSED_TIME())

	elif iterationFlag:
		from deepchess.play import generateGames
		from deepchess.trainmodel import trainModel

		generateGames()
		print("Data generated")
		print(CONST.LAPSED_TIME())

		trainModel()
		print("Model trained")
		print(CONST.LAPSED_TIME())

		existingArchieves = sorted([filename for filename in os.listdir(CONST.PATH) if filename.startswith("data_") and filename.endswith(".7z")])
		latestArchieveIndex = int(existingArchieves[-1][len("data_"):-len(".7z")])
		archieveName = CONST.PATH + "data_" + str(latestArchieveIndex+1) + ".7z"
		os.system(" ".join(["7z", "a", archieveName, CONST.DATA]))
		print("Data archieved")
		print(CONST.LAPSED_TIME())


		for filename in os.listdir(CONST.DATA):
			if filename.startswith("game_") and filename.endswith(".json"):
				os.remove(CONST.DATA + filename)
		print("Data cleared after archieving")
		

		print("\nOne iteration complete.")
		print(CONST.LAPSED_TIME())
	elif viewFlag:
		from deepchess.play import readGameJSON, displayGameLasts
		gameNum, moveNum = 0, -1
		if flags:
			gameNum = int(flags.pop(0))
		if flags:
			moveNum = int(flags.pop(0))
			
		for mi in range(200):
			nextMove = readGameJSON(gameNum=gameNum, moveNum=moveNum+mi)
			if not nextMove:
				break


	return True


if __name__ == "__main__":
	main()