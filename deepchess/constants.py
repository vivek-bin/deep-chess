from inspect import getsourcefile
from os.path import abspath
from os.path import dirname
from os.path import isdir
from time import time, ctime
import os
import sys

#os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
import tensorflow as tf
physical_devices = tf.config.list_physical_devices('GPU') 
try: 
	tf.config.experimental.set_memory_growth(physical_devices[0], True) 
except: 
	# Invalid device or cannot modify virtual devices once initialized. 
	pass

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
ENGINE_TYPE = "C"		#C or PY
SEARCH_TYPE = "C"		#C or PY
MC_SEARCH_MOVE = True
DISPLAY_TEXT_BOARD = True
DISPLAY_TK_BOARD = False
SHOW_BOARDS = True

###paths
GOOGLE_DRIVE_PATH = "/content/drive/My Drive/"
if isdir(GOOGLE_DRIVE_PATH):
	PATH = GOOGLE_DRIVE_PATH
else:
	PATH = dirname(dirname(dirname(abspath(getsourcefile(lambda:0))))) + "/"
PROJECT = dirname(dirname(abspath(getsourcefile(lambda:0)))) + "/"


###data paths
MODELS = PATH + "models/"
DATA = PATH + "data/"
LOGS = PATH + "logs/"
IMAGES = PROJECT + "deepchess/images/"
IMAGE_SIZE = 60

###model parameters
EMBEDDING_SIZE = 8
MODEL_DEPTH = 25

L2_REGULARISATION = 1e-4
CONV_DATA_FORMAT = "channels_last"
CONV_ACTIVATION = "leakyRelu"
DENSE_ACTIVATION = "leakyRelu"
CONV_ACTIVATION_CONST = 0.1 # for leaky relu
DENSE_ACTIVATION_CONST = 0.1 # for leaky relu
NUM_FILTERS = 128
CONV_SIZE = 3

###training parameters
PREDICTION_BATCH_SIZE = 512
BATCH_SIZE = 128
NUM_EPOCHS = 50
LEARNING_RATE = 2e-3
LEARNING_RATE_DECAY = 0.0
USE_TENSORBOARD = False

MODEL_NAME_SUFFIX = ".hdf5"
MODEL_NAME_PREFIX = "Alphazero"
MODEL_CHECKPOINT_NAME = MODEL_NAME_PREFIX + "-{epoch:04d}" + MODEL_NAME_SUFFIX

