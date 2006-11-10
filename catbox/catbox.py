import catbox_int

class CatboxRet(object):
    def __init__(self):
        self.ownerships = {}
        self.modes = {}
        self.ret = 0

def run(func, writable_paths=[], logger=None):
    '''returns a CatboxRet object after running the specified callable'''
    ret_object = CatboxRet()
    ret_object.ret = catbox_int.run(func, ret_object = ret_object, writable_paths=writable_paths, logger=logger)
    return ret_object 