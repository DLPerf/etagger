# source from 
# : http://forensics.tistory.com/29

from __future__ import print_function

class EarlyStopping():
    def __init__(self, patience=0, measure='loss', verbose=0):
        self._step = 0
        if measure == 'loss': # loss
            self._value = float('inf')
        else:                 # f1, accuracy
            self._value = 0
        self.patience  = patience
        self.verbose = verbose
 
    def validate(self, value, measure='loss'):
        going_worse = False
        if measure == 'loss': # loss
            if self._value < value: going_worse = True
        else:                 # f1, accuracy
            if self._value > value: going_worse = True 
        if going_worse:
            self._step += 1
            if self._step > self.patience:
                if self.verbose:
                    print('Training process is stopped early!')
                return True
        else:
            self._step = 0
            self._value = value
 
        return False