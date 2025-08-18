class NewStyleObject(object):
    pass

class OldStyleObject:
    pass

class ObjectWithData(object):
    def __init__(self, data):
        self.data = data
