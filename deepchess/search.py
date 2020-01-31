from . import constants as CONST
import math

class Node():
    def __init__(self, parent=None, initialState=None):
        self.parent = parent
        self.children = []
        self.visits = 0
        self.state = initialState
        self.wins = 0
        self.losses = 0

    def addChild(self, child):
        self.children.append(child)

    def bestChild(self):
        best = self.children[0] if self.children else None
        for child in self.children:
            if child.value() > best.value():
                best = child
        
        return best

    def value(self):
        explore = math.sqrt(math.log(self.parent.visits)/self.visits) if self.parent else 0
        mean = self.wins/self.visits

        return mean + CONST.MC_EXPLORATION_CONST * explore

    
class Tree():
    def __init__(self, initialState):
        self.root = Node(None, initialState)
        self.current = self.root
        self.stateMap = {initialState:self.root}

        