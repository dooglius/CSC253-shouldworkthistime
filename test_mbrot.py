import generic
import time
import numpy

def manual(x, y):
        c = x+y*1j
        z = c
        divtime = 1000 + numpy.zeros(z.shape, dtype=int)

        for i in xrange(1000):
                z  = z**2 + c
                diverge = z*numpy.conj(z) > 2**2            # who is diverging
                div_now = diverge & (divtime==1000)  # who is diverging now
                divtime[div_now] = i                  # note when
                z[diverge] = 2                        # avoid diverging too much

        return divtime

c_code = (
"int it=0;"
"double cr = real;"
"double ci = im;"
"double realsq = real*real;"
"double imsq = im*im;"
"for(it=0; it<1000; it++){"
	"double newreal = realsq-imsq;"
	"double newim = 2*real*im;"
	"real = newreal+cr;"
	"im = newim+ci;"
	"realsq = real*real;"
	"imsq = im*im;"
	"if(realsq + imsq > 4.0){"
		"return it;"
	"}"
"}"
"return 1000;"
)

h = 500
w = 500

y = numpy.tile(numpy.arange(-1.4,1.4,2.8/h),w)
x = numpy.repeat(numpy.arange(-2.0,0.8,2.8/w),h)

ans1 = numpy.zeros(x.shape, dtype=numpy.int);
ans2 = numpy.zeros(x.shape, dtype=numpy.int);
ans3 = numpy.zeros(x.shape, dtype=numpy.int);
ans4 = numpy.zeros(x.shape, dtype=numpy.int);

time0 = time.time()
ans1 = manual(x,y)
time1 = time.time()
f = generic.make_c_function(1, c_code, [("double","real"),("double","im")], "int")
time2 = time.time()
generic.call_c_function(f,(x,y, ans2),0,h*w)
time3 = time.time()
handle = generic.call_c_function_parallel(f,(x,y,ans3),0,h*w,4,1)
generic.wait_for_parallel_finish(handle)
time4 = time.time()
handle = generic.call_c_function_parallel(f,(x,y,ans4),0,h*w,4,1)
generic.wait_for_parallel_finish(handle)
time5 = time.time()

# slight differences will make the array not equal, but they're close enough
print 'Numpy reference: %f\nJIT:\n creation: %f\n single-thread: %f\n four threads with stride access: %f\n four threads with chunks: %f\n' % (time1-time0, time2-time1, time3-time2, time4-time3, time5-time4)
