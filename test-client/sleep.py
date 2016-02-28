import time
import os

def f():
    while True:
        time.sleep(1)
        print("%d"%(os.getpid()))

f()

