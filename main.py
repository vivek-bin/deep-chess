import sys
from deepchess import constants as CONST

VALID_FLAGS = ["--train", "--play"]

def main():
	flags = sys.argv[1:]
	trainModelFlag = False
	playFlag = False

	try:
		if flags[0] not in VALID_FLAGS:
			print("Invalid flag. Valid flags are "+", ".join(VALID_FLAGS))
			return False
		if "--train" == flags[0]:
			trainModelFlag = True
		if "--play" == flags[0]:
			playFlag = True
		flags.pop(0)
	except IndexError:
		playFlag = True
	
	if trainModelFlag:
		from deepchess.trainmodel import trainModel
		trainModel()

	if playFlag:
		from deepchess.engine import playGame
		for _ in range(100):
			playGame()
			print(CONST.LAPSED_TIME())


	return True


if __name__ == "__main__":
	main()