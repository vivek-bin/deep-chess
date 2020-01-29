

class Node():
    def __init__(self):
        self.parent = set()
        self.children = set()
        self.visits = 0
        self.state = None
        self.score = 0

    def addChild(self, child):
        self.children.add(child)
        child.addParent(self)

    def addParent(self, parent):
        self.parent.add(parent)

    
class Tree():
    def __init__(self, initialState):
        self.root = Node()
        self.current = self.root
        self.stateMap = {initialState:self.root}

        