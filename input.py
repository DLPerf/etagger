from __future__ import print_function
import numpy as np
import pickle
from embvec import EmbVec
import sys


class Input:

    def __init__(self, input_file, embvec, emb_dim, class_size, sentence_length=-1):
        word = []
        tag = []
        self.sentence = []
        self.sentence_tag = []
        self.emb_dim = emb_dim
        self.class_size = class_size
        if sentence_length == -1:
            max_sentence_length = self.find_max_length(input_file)
        else:
            max_sentence_length = sentence_length
        self.max_sentence_length = max_sentence_length
        # 'emb_dim + (5+5+1)' number of 0's
        self.word_dim = emb_dim + 11

        sentence_length = 0
        for line in open(input_file):
            if line in ['\n', '\r\n']:
                # padding
                for _ in range(max_sentence_length - sentence_length):
                    # five 0's
                    tag.append(np.array([0] * class_size))
                    temp = np.array([0 for _ in range(self.word_dim)])
                    word.append(temp)
                self.sentence.append(word)
                self.sentence_tag.append(np.array(tag))
                sentence_length = 0
                word = []
                tag = []
            else:
                tokens = line.split()
                assert (len(tokens) == 4)
                sentence_length += 1
                temp = embvec[tokens[0]]
                assert len(temp) == emb_dim
                temp = np.append(temp, self.pos(tokens[1]))       # adding pos one-hot(5)
                temp = np.append(temp, self.chunk(tokens[2]))     # adding chunk one-hot(5)
                temp = np.append(temp, self.capital(tokens[0]))   # adding capital one-hot(1)
                word.append(temp)
                tag.append(self.label(tokens[3], class_size))     # label one-hot(5)
        assert (len(self.sentence) == len(self.sentence_tag))

    @staticmethod
    def find_max_length(file_name):
        temp_len = 0
        max_length = 0
        for line in open(file_name):
            if line in ['\n', '\r\n']:
                if temp_len > max_length:
                    max_length = temp_len
                temp_len = 0
            else:
                temp_len += 1
        return max_length

    @staticmethod
    def pos(tag):
        one_hot = np.zeros(5)
        if tag == 'NN' or tag == 'NNS':
            one_hot[0] = 1
        elif tag == 'FW':
            one_hot[1] = 1
        elif tag == 'NNP' or tag == 'NNPS':
            one_hot[2] = 1
        elif 'VB' in tag:
            one_hot[3] = 1
        else:
            one_hot[4] = 1
        return one_hot

    @staticmethod
    def chunk(tag):
        one_hot = np.zeros(5)
        if 'NP' in tag:
            one_hot[0] = 1
        elif 'VP' in tag:
            one_hot[1] = 1
        elif 'PP' in tag:
            one_hot[2] = 1
        elif tag == 'O':
            one_hot[3] = 1
        else:
            one_hot[4] = 1
        return one_hot

    @staticmethod
    def capital(word):
        if ord('A') <= ord(word[0]) <= ord('Z'):
            return np.array([1])
        else:
            return np.array([0])

    @staticmethod
    def label(tag, class_size):
        one_hot = np.zeros(class_size)
        if tag.endswith('PER'):
            one_hot[0] = 1
        elif tag.endswith('LOC'):
            one_hot[1] = 1
        elif tag.endswith('ORG'):
            one_hot[2] = 1
        elif tag.endswith('MISC'):
            one_hot[3] = 1
        else:
            one_hot[4] = 1
        return one_hot

