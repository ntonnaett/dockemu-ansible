#!/bin/python3

from matplotlib import pyplot as plt
from matplotlib import style
import sys
import numpy as np

style.use('ggplot')

np.loadtxt(sys.argv[1])


plt.title('IO Stats')
plt.xlabel('time [ms]')
plt.ylabel('count')

plt.show()
