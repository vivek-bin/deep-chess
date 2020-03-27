import tensorflow as tf
from tensorflow.keras.models import Model
from tensorflow.keras import layers
from tensorflow.keras.regularizers import l2

from .. import constants as CONST
if CONST.ENGINE_TYPE == "PY":
	from .. import engine as EG
elif CONST.ENGINE_TYPE == "C":
	import cengine as EG
else:
	raise ImportError

def resNetChessModel():
	inputBoard = layers.Input(batch_shape=(None, None, EG.BOARD_SIZE, EG.BOARD_SIZE))
	inputState = layers.Input(batch_shape=(None, 3, EG.BOARD_SIZE, EG.BOARD_SIZE))

	flatBoard = layers.Reshape(target_shape=(-1, ))(inputBoard)
	embeddedBoard = layers.Embedding(input_dim=7, output_dim=CONST.EMBEDDING_SIZE, embeddings_regularizer=l2(CONST.L2_REGULARISATION))(flatBoard)
	reformBoard = layers.Reshape(target_shape=(-1, EG.BOARD_SIZE, EG.BOARD_SIZE, CONST.EMBEDDING_SIZE))(embeddedBoard)

	reformBoard = layers.Permute((4, 2, 3, 1))(reformBoard)
	reformBoard = layers.Reshape(target_shape=(CONST.BOARD_HISTORY*2*CONST.EMBEDDING_SIZE, EG.BOARD_SIZE, EG.BOARD_SIZE))(reformBoard)
	x = layers.Concatenate(axis=1)([reformBoard, inputState])

	if CONST.CONV_DATA_FORMAT == "channels_last":
		x = layers.Permute((2, 3, 1))(x)

	x = layers.BatchNormalization()(x)
	x = layers.Conv2D(filters=CONST.NUM_FILTERS, kernel_size=CONST.CONV_SIZE, padding="same", data_format=CONST.CONV_DATA_FORMAT, kernel_regularizer=l2(CONST.L2_REGULARISATION))(x)
	
	for _ in range(CONST.MODEL_DEPTH):
		x2 = layers.Conv2D(filters=CONST.NUM_FILTERS, kernel_size=CONST.CONV_SIZE, padding="same", data_format=CONST.CONV_DATA_FORMAT, kernel_regularizer=l2(CONST.L2_REGULARISATION))(x)
		x2 = layers.BatchNormalization()(x2)
		x2 = layers.Activation(CONST.CONV_ACTIVATION)(x2)

		x2 = layers.Conv2D(filters=CONST.NUM_FILTERS, kernel_size=CONST.CONV_SIZE, padding="same", data_format=CONST.CONV_DATA_FORMAT, kernel_regularizer=l2(CONST.L2_REGULARISATION))(x2)
		x = layers.Add()([x, x2])
		
		x = layers.BatchNormalization()(x)
		x = layers.Activation(CONST.CONV_ACTIVATION)(x)

	valuePre = layers.Conv2D(filters=CONST.NUM_FILTERS, kernel_size=1, padding="same", data_format=CONST.CONV_DATA_FORMAT, kernel_regularizer=l2(CONST.L2_REGULARISATION))(x)
	valuePre = layers.BatchNormalization()(valuePre)
	valuePre = layers.Activation(CONST.CONV_ACTIVATION)(valuePre)
	valuePre = layers.Flatten()(valuePre)
	valuePre = layers.Dense(CONST.NUM_FILTERS, activation=CONST.DENSE_ACTIVATION, kernel_regularizer=l2(CONST.L2_REGULARISATION))(valuePre)
	value = layers.Dense(1, activation="tanh", kernel_regularizer=l2(CONST.L2_REGULARISATION))(valuePre)

	policyPre = layers.Conv2D(filters=2*4, kernel_size=CONST.CONV_SIZE, padding="same", data_format=CONST.CONV_DATA_FORMAT, kernel_regularizer=l2(CONST.L2_REGULARISATION))(x)
	policyPre = layers.Flatten()(policyPre)
	policy = layers.Dense(EG.MAX_POSSIBLE_MOVES, activation="softmax", kernel_regularizer=l2(CONST.L2_REGULARISATION))(policyPre)

	model = tf.keras.Model(inputs=[inputBoard, inputState], outputs=[value, policy])

	return model

