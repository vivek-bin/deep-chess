import sys
from deepchess import constants as CONST

VALID_FLAGS = ["--train", "--play"]

def main():
	flags = sys.argv[1:]
	trainModelFlag = False
	playFlag = False
	modelNum = 0


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
	
	try:
		i = flags.index("-m")
		modelNum = int(flags.pop(i+1))
		flags.pop(i)
	except ValueError:
		modelNum = 1

	if trainModelFlag:
		assert modelNum in [1, 2]

		from deepchess.trainmodel import trainModel
		trainModel(modelNum=modelNum)

	if playFlag:
    pass


	return True


if __name__ == "__main__":
	main()