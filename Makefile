all: generic.so

generic.so: generic.c Python-2.7.8/python
	gcc -g -std=gnu99 -pthread -s -ldl -march=native -ffast-math -O3 -fPIC -IPython-2.7.8/Include -IPython-2.7.8 -I/usr/lib/python2.7/dist-packages/numpy/core/include/numpy generic.c -shared -o generic.so

test:
	python test_mbrot.py

clean:
	rm -f *.so *.o
