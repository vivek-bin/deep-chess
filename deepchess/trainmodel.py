import numpy as np
import tensorflow as tf
from tensorflow.keras.models import load_model
from tensorflow.keras.optimizers import Adam
import json
import tensorflow.keras.backend as K
from tensorflow.keras.callbacks import ModelCheckpoint, TensorBoard, LearningRateScheduler
from tensorflow.keras.utils import Sequence
import os
import time
import h5py
import random

from . import constants as CONST
if CONST.ENGINE_TYPE == "PY":
	from . import engine as EG
elif CONST.ENGINE_TYPE == "C":
	import cengine as EG
else:
	raise ImportError

if CONST.ENGINE_TYPE == "PY":
	from . import search as SE
elif CONST.ENGINE_TYPE == "C":
	import csearch as SE
else:
	raise ImportError
from .models.models import *


class DataSequence(Sequence):
	def __init__(self, batchSize):
		self.historyFiles = os.listdir(CONST.DATA)
		self.batchSize = batchSize

		self.mem = SE.allocNpMemory()

		random.shuffle(self.historyFiles)

	def __len__(self):
		return int(np.ceil(len(self.historyFiles) / self.batchSize))

	def __getitem__(self, idx):
		fileNames = self.historyFiles[idx*self.batchSize:(idx+1)*self.batchSize]

		histories = []
		for fileName in fileNames:
			histories.append(self.loadJsonHistory(fileName))
		
		xBatch, yBatch = self.prepareTrainingInput(histories)
		return xBatch, yBatch

	def on_epoch_end(self):
		random.shuffle(self.historyFiles)

	def prepareTrainingInput(self, histories):
		stateHistories = tuple((tuple(history["STATE_HISTORY"]) for history in histories))
		(boardInputs, otherInputs) = SE.prepareModelInput(stateHistories, self.mem)

		values = np.array(tuple((history["VALUE"] for history in histories)))
		policies = []
		for history in histories:
			policy = [0] * EG.MAX_POSSIBLE_MOVES
			for actionIdx, probability in history["SEARCHED_POLICY"].items():
				policy[actionIdx] = probability

			policies.append(policy)
		policies = np.array(policies)

		return (boardInputs, otherInputs), (values, policies)

	def loadJsonHistory(self, fileName):
		with open(CONST.DATA + fileName, "r") as file:
			history = json.load(file)
		
		history["STATE"] = EG.stateFromIndex(history["STATE"])
		history["STATE_HISTORY"] = [EG.stateFromIndex(s) for s in history["STATE_HISTORY"]]

		history["ACTIONS_POLICY"] = {int(k):v for k, v in history["ACTIONS_POLICY"].items()}
		history["SEARCHED_POLICY"] = {int(k):v for k, v in history["SEARCHED_POLICY"].items()}

		return history

def valuePolicyLoss(targets=None, outputs=None):
	valueTarget = targets[0]
	valueOutput = outputs[0]
	valueLoss = K.square(valueOutput - valueTarget)
	valueLoss = K.flatten(valueLoss)
	valueLoss = K.sum(valueLoss, 0)

	policyTarget = targets[1]
	policyOutput = outputs[1]
	policyLoss = -(policyTarget)*K.log(policyOutput)
	policyLoss = K.flatten(policyLoss)
	policyLoss = K.sum(policyLoss, 0)
	
	return valueLoss + policyLoss


def getLastCheckpoint():
	c = os.listdir(CONST.MODELS)
	c = [x for x in c if x.startswith(CONST.MODEL_NAME_PREFIX) and x.endswith(CONST.MODEL_NAME_SUFFIX)]
	return sorted(c)[-1] if c else False

def getLastEpoch():
	lastCheckpoint = getLastCheckpoint()
	if lastCheckpoint:
		epoch = lastCheckpoint[len(CONST.MODEL_NAME_PREFIX)+1:][:4]
		try:
			return int(epoch)
		except ValueError:
			pass
	
	return 0

def loadModel(loadForTraining=True):
	#get model
	trainingModel = resNetChessModel()
	if loadForTraining:
		trainingModel.compile(optimizer=Adam(lr=CONST.LEARNING_RATE, decay=CONST.LEARNING_RATE_DECAY), loss=["mean_squared_error", "categorical_crossentropy"])
	trainingModel.summary()

	checkPointName = getLastCheckpoint()
	if checkPointName:
		# load checkpoint if available
		trainingModel.load_weights(CONST.MODELS + checkPointName)
	else:
		# save initial model
		trainingModel.save(CONST.MODELS + trainingModel.name + CONST.MODEL_NAME_SUFFIX)

	return trainingModel

def trainModel():
	# get model
	trainingModel = loadModel()
	initialEpoch = getLastEpoch()

	# prepare data generators
	trainingDataGenerator = DataSequence(batchSize=CONST.BATCH_SIZE)
	
	# prepare callbacks
	callbacks = []
	if CONST.USE_TENSORBOARD:
		callbacks.append(TensorBoard(log_dir=CONST.LOGS + "tensorboard-log", histogram_freq=1, batch_size=CONST.BATCH_SIZE, write_graph=False, write_grads=True, write_images=False))

	# start training
	_ = trainingModel.fit(trainingDataGenerator, epochs=CONST.NUM_EPOCHS, callbacks=callbacks)

	# save model after training
	trainingModel.save(CONST.MODELS + trainingModel.name + "-" + str(initialEpoch+1) + CONST.MODEL_NAME_SUFFIX)

