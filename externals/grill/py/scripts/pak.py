import pyext

class pak(pyext._class):
    def __init__(self,n):
        # n should be type-checked
        self._inlets = n
        self._outlets = 1
        # initialize list
        self.lst = [0 for x in range(n)]

    def _anything_(self,n,args):
        # args should be type-checked!
        self.lst[n-1] = args 
        self._outlet(1,self.lst)
