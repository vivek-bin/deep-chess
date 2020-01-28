import tensorflow as tf
from tf.keras.models import Model
from tf.keras import layers

from .. import constants as CONST

def getModelUnit(inputTensor, size=3, filters=32):
    x = layers.Conv2d((size, size), filters, padding="same")(inputTensor)
    x = layers.BatchNormalization()(x)
    x = layers.Activation(CONST.LEAKY_RELU)(x)

    x = layers.Conv2d((size, size), filters, padding="same")(x)
    x = layers.Add()([inputTensor, x])
    
    x = layers.BatchNormalization()(x)
    x = layers.Activation(CONST.LEAKY_RELU)(x)

    return x

def getModel():

    inputTensor = layers.Input(batch_shape=(None, 2, CONST.BOARD_SIZE, CONST.BOARD_SIZE))

    flatTensor = layers.Reshape(target_shape=(-1, ))
    embedded = layers.Embedding(input_dim=7, output_dim=CONST.EMBEDDING_SIZE)(flatTensor)
    reformTensor = layers.Reshape(target_shape=(-1, 2, CONST.BOARD_SIZE, CONST.BOARD_SIZE, CONST.EMBEDDING_SIZE))

    x = reformTensor
    for _ in range(0, CONST.MODEL_DEPTH, 2):
        x = getModelUnit(x)

    outputTensor = layers.Dense(1)(x)
    model = tf.keras.Model(inputs=[inputTensor], outputs=[outputTensor])

    return model

