import generic
import time
import numpy

def func(x):
	return 10*x*x*x*x*x+4*x*x*x*x+x*x*x-20*x*x+3;

def manual(list):
	for i in xrange(len(list)):
		list[i] = func(list[i])

num = 1000000

arr = [0.0+x for x in range(0,num)]
n1 = numpy.array(arr,numpy.double);
n2 = numpy.array(arr,numpy.double);
n3 = numpy.array(arr,numpy.double);
n4 = numpy.array(arr,numpy.double);
n5 = numpy.array(arr,numpy.double);
n6 = numpy.array(arr,numpy.double);
time0 = time.time()
manual(n1)
time1 = time.time()
n2 = func(n2)
time2 = time.time()
f = generic.make_c_function(1, "return 10*x*x*x*x*x+4*x*x*x*x+x*x*x-20*x*x+3;", [("double","x")], "double")
time3 = time.time()
generic.call_c_function(f,(n3, n3),0,num)
time4 = time.time()
h = generic.call_c_function_parallel(f,(n4,n4),0,num,1,1)
generic.wait_for_parallel_finish(h)
time5 = time.time()
h = generic.call_c_function_parallel(f,(n5,n5),0,num,2,1)
generic.wait_for_parallel_finish(h)
time6 = time.time()
h = generic.call_c_function_parallel(f,(n6,n6),0,num,2,2)
generic.wait_for_parallel_finish(h)
time7 = time.time()

if(numpy.allclose(n2,n3) and numpy.allclose(n2,n4) and numpy.allclose(n2,n5)):
	print 'Manual: %f\nNumpy all at once: %f\nJIT:\n creation: %f\n single-thread: %f\n single-thread in parallel: %f\n two-threads with stride access: %f\n two threads with half each: %f\n' % (time1-time0, time2-time1, time3-time2, time4-time3, time5-time4, time6-time5, time7-time6)
else:
	print 'FAILURE'
	print n2
	print n3
	print n4
	print n5
