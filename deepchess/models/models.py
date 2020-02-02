import tensorflow as tf
from tensorflow.keras.models import Model
from tensorflow.keras import layers

from .. import constants as CONST

def getModelUnit(inputTensor, size=3, filters=32):
    x = layers.Conv2D(filters, size, padding="same")(inputTensor)
    x = layers.BatchNormalization()(x)
    x = layers.Activation(CONST.DENSE_ACTIVATION)(x)

    x = layers.Conv2D(filters, size, padding="same")(x)
    x = layers.Add()([inputTensor, x])
    
    x = layers.BatchNormalization()(x)
    x = layers.Activation(CONST.DENSE_ACTIVATION)(x)

    return x

def resNetChessModel():
    inputBoard = layers.Input(batch_shape=(None, None, CONST.BOARD_SIZE, CONST.BOARD_SIZE))
    inputState = layers.Input(batch_shape=(None, None, CONST.BOARD_SIZE, CONST.BOARD_SIZE))

    flatBoard = layers.Reshape(target_shape=(-1, ))(inputBoard)
    embeddedBoard = layers.Embedding(input_dim=7, output_dim=CONST.EMBEDDING_SIZE)(flatBoard)
    reformBoard = layers.Reshape(target_shape=(-1, CONST.BOARD_SIZE, CONST.BOARD_SIZE, CONST.EMBEDDING_SIZE))(embeddedBoard)
    reformBoard = layers.Permute((4, 2, 3, 1))(reformBoard)
    reformBoard = layers.Reshape(target_shape=(-1, CONST.BOARD_SIZE, CONST.BOARD_SIZE))(reformBoard)

    x = layers.Concatenate()([reformBoard, inputState])
    x = layers.Conv2D(CONST.NUM_FILTERS, 3, padding="same")(x)
    for _ in range(CONST.MODEL_DEPTH//2):
        x = getModelUnit(x, CONST.NUM_FILTERS)

    boardValue = layers.Dense(1)(x)
    moveProbabilities = layers.Dense(CONST.MAX_POSSIBLE_MOVES, activation="softmax")(x)

    model = tf.keras.Model(inputs=[inputBoard, inputState], outputs=[boardValue, moveProbabilities])

    return model

