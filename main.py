import sys
from deepchess import constants as CONST

VALID_FLAGS = ["--train", "--play", "--generate", "--compare"]

def main():
	flags = sys.argv[1:]
	trainModelFlag = False
	playFlag = False
	generateFlag = False
	compareModelsFlag = False

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
		flags.pop(0)
	except IndexError:
		playFlag = True
	
	if trainModelFlag:
		from deepchess.trainmodel import trainModel
		trainModel()

	if playFlag:
		from deepchess.play import playGame
		for _ in range(10):
			end, history = playGame()
			print(CONST.LAPSED_TIME())

	if compareModelsFlag:
		from deepchess.play import compareModels

		rewards = compareModels(firstIdx=-1, secondIdx=0, count=4)
		print("latest model as white, results: ", ", ".join(rewards))

		rewards = compareModels(firstIdx=0, secondIdx=-1, count=4)
		print("latest model as black, results: ", ", ".join(rewards))

		print(CONST.LAPSED_TIME())

	if generateFlag:
		from deepchess.play import generateGames
		generateGames()
		print(CONST.LAPSED_TIME())


	return True


if __name__ == "__main__":
	main()