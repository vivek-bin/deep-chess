import numpy as np
import tensorflow as tf
from tensorflow.keras.models import load_model
from tensorflow.keras.optimizers import Adam
import json
import tensorflow.keras.backend as K
from tensorflow.keras.callbacks import ModelCheckpoint, TensorBoard, LearningRateScheduler
import os
import time
import h5py

from . import constants as CONST
if CONST.ENGINE_TYPE == "PY":
	from . import engine as EG
elif CONST.ENGINE_TYPE == "C":
	import cengine as EG
else:
	raise ImportError
from . import search as SE
from .models.models import *

def prepareModelInput(stateHistories):
	batchSize = len(stateHistories)
	boardInput = np.ones((batchSize, CONST.BOARD_HISTORY, 2, EG.BOARD_SIZE, EG.BOARD_SIZE), dtype=np.int8) * EG.EMPTY
	playerInput = np.ones((batchSize, 1, EG.BOARD_SIZE, EG.BOARD_SIZE), dtype=np.int8)
	castlingStateInput = np.zeros((batchSize, 1, EG.BOARD_SIZE, EG.BOARD_SIZE), dtype=np.int8)
	enPassantStateInput = np.zeros((batchSize, 1, EG.BOARD_SIZE, EG.BOARD_SIZE), dtype=np.int8)
	for i, stateHistory in enumerate(stateHistories):
		for j in range(min(CONST.BOARD_HISTORY, len(stateHistory))):
			boardInput[i, j, :, :, :] = np.array(stateHistory[-1-j]["BOARD"], dtype=np.int8)
		
		state = stateHistory[-1]

		playerInput[i, 0] *= state["PLAYER"]
		
		for player in [EG.WHITE_IDX, EG.BLACK_IDX]:
			if state["CASTLING_AVAILABLE"][player][EG.LEFT_CASTLE]:
				castlingStateInput[i, 0, EG.KING_LINE[player], :EG.BOARD_SIZE//2] = 1
			if state["CASTLING_AVAILABLE"][player][EG.RIGHT_CASTLE]:
				castlingStateInput[i, 0, EG.KING_LINE[player], (EG.BOARD_SIZE//2 - 1):] = 1
		
		for player in [EG.WHITE_IDX, EG.BLACK_IDX]:
			if state["EN_PASSANT"][player] >= 0:
				movement = EG.PAWN_DIRECTION[player][0] * EG.PAWN_NORMAL_MOVE[0]
				pawnPos = EG.KING_LINE[player]
				for _ in range(3):
					pawnPos = pawnPos + movement
					enPassantStateInput[i, 0, pawnPos, state["EN_PASSANT"][player]] = 1

	boardInput = boardInput.reshape((batchSize, -1, EG.BOARD_SIZE, EG.BOARD_SIZE))
	stateInput = np.concatenate([playerInput, castlingStateInput, enPassantStateInput], axis=1)

	return (boardInput, stateInput)


def prepareTrainingInput(histories):
	stateHistories = tuple((tuple((x["STATE"] for x in history)) for history in histories))
	(boardInputs, stateInputs) = prepareModelInput(stateHistories)

	values = np.array((history[-1]["STATE_VALUE"] for history in histories))
	policies = []
	for history in histories:
		policy = [0] * EG.MAX_POSSIBLE_MOVES
		for actionIdx, probability in history[-1]["STATE_POLICY"].keys():
			policy[actionIdx] = probability

		policies.append(policy)
	policies = np.array(policies)

	return (boardInputs, stateInputs), (values, policies)


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
	else:
		# save initial model
		trainingModel.save(CONST.MODELS + trainingModel.name + CONST.MODEL_NAME_SUFFIX)

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
	#_ = trainingModel.fit_generator([], epochs=CONST.NUM_EPOCHS*CONST.DATA_PARTITIONS, callbacks=callbacks, initial_epoch=initialEpoch)

	# save model after training
	#trainingModel.save(CONST.MODELS + trainingModel.name + CONST.MODEL_TRAINED_NAME_SUFFIX)


def main():
	trainModel()

if __name__ == "__main__":
	main()


