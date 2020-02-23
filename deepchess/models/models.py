import tensorflow as tf
from tensorflow.keras.models import Model
from tensorflow.keras import layers

from .. import constants as CONST
if CONST.ENGINE_TYPE == "PY":
	from . import engine as EG
elif CONST.ENGINE_TYPE == "C":
	import cengine as EG
else:
	raise ImportError

def joinModelUnit(inputTensor, filters, kernel_size):
	x = layers.Conv2D(filters=filters, kernel_size=kernel_size, padding="same", data_format=CONST.CONV_DATA_FORMAT)(inputTensor)
	x = layers.BatchNormalization()(x)
	x = layers.Activation(CONST.DENSE_ACTIVATION)(x)

	x = layers.Conv2D(filters=filters, kernel_size=kernel_size, padding="same", data_format=CONST.CONV_DATA_FORMAT)(x)
	x = layers.Add()([inputTensor, x])
	
	x = layers.BatchNormalization()(x)
	x = layers.Activation(CONST.DENSE_ACTIVATION)(x)

	return x

def resNetChessModel():
	inputBoard = layers.Input(batch_shape=(None, None, EG.BOARD_SIZE, EG.BOARD_SIZE))
	inputState = layers.Input(batch_shape=(None, 3, EG.BOARD_SIZE, EG.BOARD_SIZE))

	flatBoard = layers.Reshape(target_shape=(-1, ))(inputBoard)
	embeddedBoard = layers.Embedding(input_dim=7, output_dim=CONST.EMBEDDING_SIZE)(flatBoard)
	reformBoard = layers.Reshape(target_shape=(-1, EG.BOARD_SIZE, EG.BOARD_SIZE, CONST.EMBEDDING_SIZE))(embeddedBoard)

	reformBoard = layers.Permute((4, 2, 3, 1))(reformBoard)
	reformBoard = layers.Reshape(target_shape=(CONST.BOARD_HISTORY*2*CONST.EMBEDDING_SIZE, EG.BOARD_SIZE, EG.BOARD_SIZE))(reformBoard)
	x = layers.Concatenate(axis=1)([reformBoard, inputState])

	if CONST.CONV_DATA_FORMAT == "channels_last":
		x = layers.Permute((2, 3, 1))(x)

	x = layers.BatchNormalization()(x)
	x = layers.Conv2D(filters=CONST.NUM_FILTERS, kernel_size=CONST.CONV_SIZE, padding="same", data_format=CONST.CONV_DATA_FORMAT)(x)
	for _ in range(CONST.MODEL_DEPTH):
		x = joinModelUnit(x, filters=CONST.NUM_FILTERS, kernel_size=CONST.CONV_SIZE)

	x = layers.Conv2D(filters=4, kernel_size=1, padding="same", data_format=CONST.CONV_DATA_FORMAT)(x)
	x = layers.Reshape(target_shape=(EG.BOARD_SIZE*EG.BOARD_SIZE*4,))(x)

	boardValue = layers.Dense(1,activation="tanh")(x)
	moveProbabilities = layers.Dense(EG.MAX_POSSIBLE_MOVES, activation="softmax")(x)

	model = tf.keras.Model(inputs=[inputBoard, inputState], outputs=[boardValue, moveProbabilities])

	return model

