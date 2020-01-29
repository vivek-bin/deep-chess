import numpy as np
from keras.models import load_model
from keras.optimizers import Adam
import json
import keras.backend as K
from keras.callbacks import ModelCheckpoint, TensorBoard, LearningRateScheduler
import os
import time
import h5py

from . import constants as CONST
from . import engine as EG
from .models.models import *
from .processing import fileaccess as FA

def prepareModelInput(gameHistory):
	score = EG.finalScore(gameHistory)
	allInput = []

	for i in range(len(gameHistory) - 1):
		boardInput = np.ones((CONST.BOARD_HISTORY, 2, CONST.BOARD_SIZE, CONST.BOARD_SIZE), dtype=np.int8) * CONST.EMPTY
		for j in range(CONST.BOARD_HISTORY):
			if i >= j:
				boardInput[j] = gameHistory[i-j][0]
		boardInput = boardInput.reshape((-1, CONST.BOARD_SIZE, CONST.BOARD_SIZE))

		states = gameHistory[i][1]
		
		playerInput = np.ones((1, CONST.BOARD_SIZE, CONST.BOARD_SIZE), dtype=np.int8) * states["PLAYER"]
		
		castlingStateInput = np.zeros((1, CONST.BOARD_SIZE, CONST.BOARD_SIZE), dtype=np.int8)
		for player in [CONST.WHITE_IDX, CONST.BLACK_IDX]:
			if states["CASTLING_AVAILABLE"][player][CONST.LEFT_CASTLE]:
				for i in range(0, CONST.BOARD_SIZE//2):
					castlingStateInput[0, CONST.KING_LINE[player], i] = 1
			if states["CASTLING_AVAILABLE"][player][CONST.RIGHT_CASTLE]:
				for i in range(CONST.BOARD_SIZE//2 + 1, CONST.BOARD_SIZE):
					castlingStateInput[0, CONST.KING_LINE[player], i] = 1
		
		enPassantStateInput = np.zeros((1, CONST.BOARD_SIZE, CONST.BOARD_SIZE), dtype=np.int8)
		for player in [CONST.WHITE_IDX, CONST.BLACK_IDX]:
			if states["EN_PASSANT"][player] >= 0:
				movement = CONST.PAWN_DIRECTION[player] * CONST.PAWN_NORMAL_MOVE
				pawnPos = np.array((CONST.KING_LINE[player], states["EN_PASSANT"][player]), dtype=np.int8)
				for _ in range(3):
					pawnPos = pawnPos + movement
					enPassantStateInput[0, pawnPos[0], pawnPos[1]] = 1
					pawnPos = pawnPos + movement
					enPassantStateInput[0, pawnPos[0], pawnPos[1]] = 1
					pawnPos = pawnPos + movement
					enPassantStateInput[0, pawnPos[0], pawnPos[1]] = 1
		statesInput = np.concatenate([playerInput, castlingStateInput, enPassantStateInput])


		moveScore = score * (CONST.SCORE_DECAY ** (len(gameHistory) - i - 1))
		moveIndex = EG.moveIndex(gameHistory[i][2])

		x = (boardInput, statesInput)
		y = (moveScore, moveIndex)
		allInput.append(x, y)
	
	return allInput


def sparseCrossEntropyLoss(targets=None, outputs=None):
	batchSize = K.shape(outputs)[0]
	sequenceSize = K.shape(outputs)[1]
	vocabularySize = K.shape(outputs)[2]
	firstPositionShifter = K.repeat(K.expand_dims(K.arange(sequenceSize) * vocabularySize, 0), batchSize)
	secondPositionShifter = K.repeat(K.expand_dims(K.arange(batchSize) * sequenceSize * vocabularySize, 1), sequenceSize)

	shiftedtargets = K.cast(K.flatten(targets), "int32") + K.flatten(firstPositionShifter) + K.flatten(secondPositionShifter)

	relevantValues = K.gather(K.flatten(outputs), shiftedtargets)
	relevantValues = K.reshape(relevantValues, (batchSize, -1))
	relevantValues = K.clip(relevantValues, K.epsilon(), 1. - K.epsilon())
	cost = -K.log(relevantValues)
	return cost

def lrScheduler(epoch):
	x = (epoch + 1) / (CONST.NUM_EPOCHS * CONST.DATA_PARTITIONS)
	temp = CONST.SCHEDULER_LEARNING_RAMPUP / CONST.SCHEDULER_LEARNING_DECAY
	scale = ((1+temp)**CONST.SCHEDULER_LEARNING_DECAY) * ((1+1/temp)**CONST.SCHEDULER_LEARNING_RAMPUP) * CONST.SCHEDULER_LEARNING_SCALE

	lr = CONST.SCHEDULER_LEARNING_RATE * scale * (x**CONST.SCHEDULER_LEARNING_RAMPUP) * ((1 - x)**CONST.SCHEDULER_LEARNING_DECAY)

	print("Learning rate =", lr)
	return np.clip(lr, CONST.SCHEDULER_LEARNING_RATE_MIN, CONST.SCHEDULER_LEARNING_RATE)

def evaluateModel(model, xTest, yTest):
	scores = model.evaluate(xTest, yTest, verbose=0)
	print("%s: %.2f%%" % (model.metrics_names[1], scores[1]*100))


def getLastCheckpoint(modelName):
	c = os.listdir(CONST.MODELS)
	c = [x for x in c if x.startswith(modelName) and x.endswith(CONST.MODEL_NAME_SUFFIX)]
	return sorted(c)[-1] if c else False

def getLastEpoch(modelName):
	lastCheckpoint = getLastCheckpoint(modelName)
	if lastCheckpoint:
		epoch = lastCheckpoint[len(modelName)+1:][:4]
		try:
			return int(epoch)
		except ValueError:
			pass
	
	return 0

def loadModel(loadForTraining=True):
	#get model
	trainingModel = resNetChessModel()
	if loadForTraining:
		trainingModel.compile(optimizer=Adam(lr=CONST.LEARNING_RATE, decay=CONST.LEARNING_RATE_DECAY/CONST.DATA_PARTITIONS), loss=sparseCrossEntropyLoss, metrics=[CONST.EVALUATION_METRIC])
		trainingModel.summary()

	#load checkpoint if available
	checkPointName = getLastCheckpoint(trainingModel.name)
	if checkPointName:
		trainingModel.load_weights(CONST.MODELS + checkPointName)

		if loadForTraining:
			savedOptimizerStates = h5py.File(CONST.MODELS + checkPointName, mode="r")["optimizer_weights"]
			optimizerWeightNames = [n.decode("utf8") for n in savedOptimizerStates.attrs["weight_names"]]
			optimizerWeightValues = [savedOptimizerStates[n] for n in optimizerWeightNames]

			trainingModel._make_train_function()
			trainingModel.optimizer.set_weights(optimizerWeightValues)

	return trainingModel

def trainModel():
	# get model
	trainingModel, _ = loadModel()
	initialEpoch = getLastEpoch(trainingModel.name)

	# prepare callbacks
	callbacks = []
	callbacks.append(ModelCheckpoint(CONST.MODELS + trainingModel.name + CONST.MODEL_CHECKPOINT_NAME_SUFFIX, monitor=CONST.EVALUATION_METRIC, mode='max', save_best_only=True, period=CONST.CHECKPOINT_PERIOD))
	callbacks.append(LearningRateScheduler(lrScheduler))

	if CONST.USE_TENSORBOARD:
		callbacks.append(TensorBoard(log_dir=CONST.LOGS + "tensorboard-log", histogram_freq=1, batch_size=CONST.BATCH_SIZE, write_graph=False, write_grads=True, write_images=False))

	# start training
	_ = trainingModel.fit_generator([], epochs=CONST.NUM_EPOCHS*CONST.DATA_PARTITIONS, callbacks=callbacks, initial_epoch=initialEpoch)

	# save model after training
	trainingModel.save(CONST.MODELS + trainingModel.name + CONST.MODEL_TRAINED_NAME_SUFFIX)


def main():
	trainModel()

if __name__ == "__main__":
	main()


