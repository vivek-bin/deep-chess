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
if CONST.SEARCH_TYPE == "PY":
	from .. import search as SE
elif CONST.SEARCH_TYPE == "C":
	import csearch as SE
else:
	raise ImportError

def resNetChessModel():
	inputBoard = layers.Input(batch_shape=(None, None, EG.BOARD_SIZE, EG.BOARD_SIZE))
	inputFlags = layers.Input(batch_shape=(None, 3, EG.BOARD_SIZE, EG.BOARD_SIZE))

	flatBoard = layers.Reshape(target_shape=(-1, ))(inputBoard)
	embeddedBoard = layers.Embedding(input_dim=7, output_dim=CONST.EMBEDDING_SIZE, embeddings_regularizer=l2(CONST.L2_REGULARISATION))(flatBoard)
	reformBoard = layers.Reshape(target_shape=(-1, EG.BOARD_SIZE, EG.BOARD_SIZE, CONST.EMBEDDING_SIZE))(embeddedBoard)

	reformBoard = layers.Permute((4, 2, 3, 1))(reformBoard)
	reformBoard = layers.Reshape(target_shape=(SE.BOARD_HISTORY*2*CONST.EMBEDDING_SIZE, EG.BOARD_SIZE, EG.BOARD_SIZE))(reformBoard)
	x = layers.Concatenate(axis=1)([reformBoard, inputFlags])

	if CONST.CONV_DATA_FORMAT == "channels_last":
		x = layers.Permute((2, 3, 1))(x)

	x = layers.BatchNormalization()(x)
	x = layers.Conv2D(filters=CONST.NUM_FILTERS, kernel_size=CONST.CONV_SIZE, padding="same", data_format=CONST.CONV_DATA_FORMAT, kernel_regularizer=l2(CONST.L2_REGULARISATION))(x)
	
	for _ in range(CONST.MODEL_DEPTH):
		x2 = layers.Conv2D(filters=CONST.NUM_FILTERS, kernel_size=CONST.CONV_SIZE, padding="same", data_format=CONST.CONV_DATA_FORMAT, kernel_regularizer=l2(CONST.L2_REGULARISATION))(x)
		x2 = layers.BatchNormalization()(x2)
		if CONST.CONV_ACTIVATION == "leakyRelu":
			x2 = layers.LeakyReLU(CONST.CONV_ACTIVATION_CONST)(x2)
		else:
			x2 = layers.Activation(CONST.CONV_ACTIVATION)(x2)

		x2 = layers.Conv2D(filters=CONST.NUM_FILTERS, kernel_size=CONST.CONV_SIZE, padding="same", data_format=CONST.CONV_DATA_FORMAT, kernel_regularizer=l2(CONST.L2_REGULARISATION))(x2)
		x = layers.Add()([x, x2])
		
		x = layers.BatchNormalization()(x)
		if CONST.CONV_ACTIVATION == "leakyRelu":
			x = layers.LeakyReLU(CONST.CONV_ACTIVATION_CONST)(x)
		else:
			x = layers.Activation(CONST.CONV_ACTIVATION)(x)

	x = layers.Conv2D(filters=2, kernel_size=CONST.CONV_SIZE, padding="same", data_format=CONST.CONV_DATA_FORMAT, kernel_regularizer=l2(CONST.L2_REGULARISATION))(x)
	if CONST.CONV_ACTIVATION == "leakyRelu":
		x = layers.LeakyReLU(CONST.CONV_ACTIVATION_CONST)(x)
	else:
		x = layers.Activation(CONST.CONV_ACTIVATION)(x)
	x = layers.Flatten()(x)

	valuePre = layers.Dense(EG.BOARD_SIZE*EG.BOARD_SIZE, kernel_regularizer=l2(CONST.L2_REGULARISATION))(x)
	if CONST.DENSE_ACTIVATION == "leakyRelu":
		valuePre = layers.LeakyReLU(CONST.DENSE_ACTIVATION_CONST)(valuePre)
	else:
		valuePre = layers.Activation(CONST.DENSE_ACTIVATION)(valuePre)
	valuePre = layers.Dense(1, kernel_regularizer=l2(CONST.L2_REGULARISATION))(valuePre)

	policyPre = layers.Dense(EG.BOARD_SIZE*EG.BOARD_SIZE, kernel_regularizer=l2(CONST.L2_REGULARISATION))(x)
	if CONST.DENSE_ACTIVATION == "leakyRelu":
		policyPre = layers.LeakyReLU(CONST.DENSE_ACTIVATION_CONST)(policyPre)
	else:
		policyPre = layers.Activation(CONST.DENSE_ACTIVATION)(policyPre)
	policyPre = layers.Dense(EG.MAX_POSSIBLE_MOVES, kernel_regularizer=l2(CONST.L2_REGULARISATION))(policyPre)

	value = layers.Activation("tanh", name="value")(valuePre)
	policy = layers.Activation("softmax", name="policy")(policyPre)

	model = tf.keras.Model(inputs=[inputBoard, inputFlags], outputs=[value, policy], name=CONST.MODEL_NAME_PREFIX)

	return model

