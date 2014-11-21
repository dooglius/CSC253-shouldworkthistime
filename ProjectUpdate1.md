# Project Update

At this point in the project, I have completed the basic proof-of-concept of offloading big computations to C code that runs in parallel to Python code.

I found the documentation at https://docs.python.org/2/c-api/index.html to be extremely helpful in doing this project, although a good deal of trial and error had to be done.
Importantly, the following boilerplate in a C shared library is needed for Python to be able to use it:
    static PyMethodDef exported[] = {
            {"calc_sync", calc_sync, METH_O, NULL},
            {"calc_async", calc_async, METH_O, NULL},
            {"calc_wait", calc_wait, METH_O, NULL},
            // terminator
            {NULL}
    };
    
    PyMODINIT_FUNC initconcept(void){
            Py_InitModule("concept", exported);
    }
    
Note that <code>calc_sync</code>, <code>calc_async</code>, and <code>calc_wait</code> are the methods being exported, and "concept" is the name of the module. The name "initconcept" is "init"+<module name>, which Python looks for, and this is compiled into the shared library <code>concept.so</code>.

Recall from my project proposal that for the proof-of-concept, I'm using the polynomial 10x^5+4x^4+x^3-20x^2+3 as a computational test.
To make things fair, I used exactly the same line of code to compute this value in both C and Python:

    static double func(double x){
        return 10*x*x*x*x*x+4*x*x*x*x+x*x*x-20*x*x+3;
    }

    def func(x):
        return 10*x*x*x*x*x+4*x*x*x*x+x*x*x-20*x*x+3
        
However, both methods run far too quickly to have a reasonable comparison by just computing one value, as the overhead of the Python to C switch would outweigh the time saved by the call.
Thus, I decided to have both Python and C compute the polynomial value for arrays of floating point numbers:

    import concept
    import time
    
    def func(x):
            return 10*x*x*x*x*x+4*x*x*x*x+x*x*x-20*x*x+3;
    
    def manual(list):
            for i in range(0,len(list)):
                    list[i] = func(list[i])
    
    arr1 = [0.0+x for x in range(0,2000000)]
    arr2 = [0.0+x for x in range(0,2000000)]
    arr3 = [0.0+x for x in range(0,2000000)]
    arr4 = [0.0+x for x in range(0,2000000)]
    arr5 = [0.0+x for x in range(0,2000000)]
    arr6 = [0.0+x for x in range(0,2000000)]
    arr7 = [0.0+x for x in range(0,2000000)]
    time0 = time.time()
    manual(arr1)
    time1 = time.time()
    concept.calc_sync(arr2)
    time2 = time.time()
    waits = [concept.calc_async(x) for x in [arr3,arr4]]
    for w in waits:
            concept.calc_wait(w)
    time3 = time.time()
    waits = [concept.calc_async(x) for x in [arr5,arr6,arr7]]
    for w in waits:
            concept.calc_wait(w)
    time4 = time.time()
    print '%f %f %f %f' % (time1-time0, time2-time1, (time3-time2)/2, (time4-time3)/3)
    
Here, the manual method computes the values in Python, and compares them against an external module, which will be implemented in C.
Note that the API for the external module is reasonably straightforward: one can either call calc_sync, which runs synchronously (in other words, the function will execute in the main thread, and only return when it has finished), or use the calc_async and calc_wait, the first of which begins a parallel execution, and the second of which waits until this computation has finished.
Note that call_async returns an opaque handle (implemented as just a PyIntObject with the thread ID), which is passed back in calc_wait, so that we know which computation we're waiting for.
The test is relatively straightforward: time the manual computation in python, then compare it against one, two, and three copies running parallel in the C code, printing the average run time.

Before we jump into preliminary results, here's the C code I used. It's pretty straightforward use of the pthread classes:

    PyObject* calc_sync(PyObject* self, PyObject* args){
            PyListObject *list = (PyListObject*)args;
            int num = list->ob_size;
            if(num > 0){
                    PyFloatObject** arr = (PyFloatObject**)list->ob_item;
    
                    int i;
                    for(i=0; i<num; i++){
                            arr[i]->ob_fval=func(arr[i]->ob_fval);
                    }
            }
    
            Py_INCREF(args);
            return args;
    }
    
    void* worker_thread(void* arg){
            PyListObject *list = (PyListObject*)arg;
    
            int num = list->ob_size;
            if(num > 0){
                    PyFloatObject** arr = (PyFloatObject**)list->ob_item;
    
                    int i;
                    for(i=0; i<num; i++){
                            arr[i]->ob_fval=func(arr[i]->ob_fval);
                    }
            }
            return list;
    }
    
    PyObject* calc_async(PyObject* self, PyObject* args){
            pthread_t tid;
            pthread_create(&tid, NULL, &worker_thread, args);
            Py_INCREF(args);
            PyObject* retval = PyInt_FromLong((long)tid);
            return retval;
    }
    
    PyObject* calc_wait(PyObject* self, PyObject* args){
            pthread_t tid = (pthread_t)((PyIntObject*)args)->ob_ival;
            PyObject* ans;
            pthread_join(tid, (void*)&ans);
            return ans;
    }

The performance results (running on a linux VM on my personal desktop, which has 4 cores):

    1.670315 0.019440 0.013094 0.011398

So, it looks like we get roughly a 100x speedup by using C! So far, the work is looking pretty promising.

## Notes
There are a couple things I ran into while doing this, that I had to look into to figure out how to deal with.
In my <code>concept.c</code> file, when including both "Python.h" (which I need for the Python libraries) and "pthread.h", there is a conflict on the macro <code>_POSIX_C_SOURCE</code>, which needs to be <cdode>#undef</code>'d. This does indicate a potential API mismatch, but as yet, I haven't hit any problems.
At first, the "Python.h" my C code was compiling against was not the right one; I had to uninstall a different version of Python on my computer (than 2.7.8 in the local source directory for this class), which was causing some very difficult-to-debug problems for a while.
One must use the <code>-fPIC</code> and <code>-shared</code> options for gcc in order to build a shared library correctly.
The cycleX machines have CPU throttling, which means that performance benchmarks on them aren't entirely accurate. For the time being, I'm using a VM on my PC, but word is still out as to whether that's a legitimate form of testing or not.

## Further work
I'm modifying my project goals a little bit based on what I've learned so far, but they're mostly the same, except I'm expanding because it's been easier from a technical perspective than I expected.
### Primary goals
- Verify there are no memory leaks by using many large datasets
- Make sure there are no issues with the Posix library differences
- Find a better way to test performance
- Compare performance against existing solutions (numpy) for high-performance processing.
### Stretch goals
- A generic library that can be used to call existing C functions in existing libraries
- A way to have a sort-of JIT so that Python can create functions on the fly and then have C code execute them efficiently
- Look into a way to pack Python lists more densely (numpy I think has support for this) for faster C computation
