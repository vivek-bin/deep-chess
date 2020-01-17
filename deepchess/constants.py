from inspect import getsourcefile
from os.path import abspath
from os.path import dirname
from os.path import isdir
from time import time, ctime
import os
import sys
import numpy as np
#from tensorflow.keras import backend as K

print(ctime().rjust(60,"-"))
START_TIME = time()
def LAPSED_TIME():
	return "{:10.2f} seconds".format((time() - START_TIME)).rjust(60,"-")

class HiddenPrints:
	def __enter__(self):
		self._original_stdout = sys.stdout
		self._original_stderr = sys.stderr
		sys.stdout = open(os.devnull, 'w')
		sys.stderr = open(os.devnull, 'w')

	def __exit__(self, exc_type, exc_val, exc_tb):
		sys.stdout.close()
		sys.stderr.close()
		sys.stdout = self._original_stdout
		sys.stderr = self._original_stderr

# game constants
BOARD_SIZE = 8
BOARD_HISTORY = 1
WHITE_IDX = 0
BLACK_IDX = 1
OPPONENT = {BLACK_IDX:WHITE_IDX, WHITE_IDX:BLACK_IDX}

EMPTY = 0
PAWN = 1
BISHOP = 2
KNIGHT = 3
ROOK = 4
QUEEN = 5
KING = 6
PROMOTIONS = [QUEEN, ROOK, KNIGHT, BISHOP]

MOVE_DIRECTIONS = {}
MOVE_DIRECTIONS[BISHOP] = [np.array((1, 1), dtype=int), np.array((1, -1), dtype=int), np.array((-1, -1), dtype=int), np.array((-1, 1), dtype=int)]
MOVE_DIRECTIONS[ROOK] = [np.array((1, 0), dtype=int), np.array((-1, 0), dtype=int), np.array((0, -1), dtype=int), np.array((0, 1), dtype=int)]
MOVE_DIRECTIONS[QUEEN] = MOVE_DIRECTIONS[BISHOP] + MOVE_DIRECTIONS[ROOK]

KNIGHT_MOVES = [np.array((1, 2), dtype=int), np.array((2, 1), dtype=int), np.array((-1, 2), dtype=int), np.array((2, -1), dtype=int), np.array((1, -2), dtype=int), np.array((-2, 1), dtype=int), np.array((-1, -2), dtype=int), np.array((-2, -1), dtype=int)]

KING_MOVES = MOVE_DIRECTIONS[BISHOP] + MOVE_DIRECTIONS[ROOK]
KING_LINE = {WHITE_IDX:0, BLACK_IDX:BOARD_SIZE - 1}
LEFT_CASTLE = 0
RIGHT_CASTLE = 1
KING_CASTLE_STEPS = {WHITE_IDX:{LEFT_CASTLE:[], RIGHT_CASTLE:[]}, BLACK_IDX:{LEFT_CASTLE:[], RIGHT_CASTLE:[]}}
for player in [WHITE_IDX, BLACK_IDX]:
	for i in range(3):
		KING_CASTLE_STEPS[player][LEFT_CASTLE].append(np.array((KING_LINE[player], BOARD_SIZE//2 - i), dtype=int))
		KING_CASTLE_STEPS[player][RIGHT_CASTLE].append(np.array((KING_LINE[player], BOARD_SIZE//2 + i), dtype=int))

PAWN_CAPTURE_MOVES = [np.array((1, 1), dtype=int), np.array((1, -1), dtype=int)]
PAWN_NORMAL_MOVE = np.array((1, 0), dtype=int)
PAWN_FIRST_MOVE = np.array((2, 0), dtype=int)
PAWN_DIRECTION = {WHITE_IDX:np.array((1, 1), dtype=int), BLACK_IDX:np.array((-1, 1), dtype=int)}


###paths
GOOGLE_DRIVE_PATH = "/content/drive/My Drive/"
if isdir(GOOGLE_DRIVE_PATH):
	PATH = GOOGLE_DRIVE_PATH
	PROJECT = GOOGLE_DRIVE_PATH
else:
	PATH = dirname(dirname(dirname(abspath(getsourcefile(lambda:0))))) + "/"
	PROJECT = dirname(dirname(abspath(getsourcefile(lambda:0)))) + "/"


###data paths
MODELS = PATH + "models/"
DATA = PATH + "data/"
LOGS = PATH + "logs/"


###model parameters
SCALE_DOWN_MODEL_BY = 1

#common model params
MODEL_BASE_UNITS = 512 // SCALE_DOWN_MODEL_BY

#DENSE_ACTIVATION = lambda x: K.maximum(x, x * 0.1) # leaky relu

###training parameters
DATA_PARTITIONS = 1000
TRAIN_SPLIT_PCT = 0.90
BATCH_SIZE = 32
NUM_EPOCHS = 10
VALIDATION_SPLIT_PCT = 0.1
LEARNING_RATE = 0.0002
LEARNING_RATE_DECAY = 0.
SCHEDULER_LEARNING_SCALE = 1.1
SCHEDULER_LEARNING_RATE = 4 * 10**-4
SCHEDULER_LEARNING_RAMPUP = 0.25
SCHEDULER_LEARNING_DECAY = 3
SCHEDULER_LEARNING_RATE_MIN = SCHEDULER_LEARNING_RATE * 10**-3
LABEL_SMOOTHENING = 0.0
LOSS_FUNCTION = "sparse_categorical_crossentropy"
EVALUATION_METRIC = "sparse_categorical_accuracy"
CHECKPOINT_PERIOD = 25
USE_TENSORBOARD = False

MODEL_NAME_SUFFIX = ".hdf5"
MODEL_CHECKPOINT_NAME_SUFFIX = "-{epoch:04d}-{val_" + EVALUATION_METRIC + ":.4f}" + MODEL_NAME_SUFFIX
MODEL_TRAINED_NAME_SUFFIX = "-Trained" + MODEL_NAME_SUFFIX

