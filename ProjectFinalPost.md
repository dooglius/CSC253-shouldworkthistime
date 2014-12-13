# High-level overview of the code
I've added the latest version of my code to this repository, which you can check out. The <code>test_poly.py</code> and <code>test_mbrot.py</code> are the two benchmarks I wrote, but they are also good to see how to use my code.
Essentially, I provide a module called "generic" which can be used to just-in-time compile arbitrary C programs, and then have them run, without ever having to get into the nitty-gritty of C code (like allocation and pointers)--things with which a Python programmer would not be familiar.
Compile the project with <code>make</code>, and in order to use the generic module, just enter <code>import generic</code> into Python code to be able to access its functions.
To just-in-time compile some small C code, use the function 
```
    generic.make_c_function(<input-type>, <c code string>, [(<c-type>,<input-var>),...], <output-c-type>)
```
which returns an opaque function object.
input-type should always be the integer 1, which corresponds to a tuple of numpy arrays, each array corresponding to a fixed input variable, and the last array in the tuple corresponding to the output variable. c-type and output-c-type can be only int or double at the moment, although it shouldn't be difficult to add more.
To run the C code in a normal, synchronous way, there are a few different functions you can use. 
```
    generic.call_c_function(<func-obj>, <input-blob>, <start-row>, <end-row>)
```
which performs the function on the input, and stores the answer in the output (as specified by the input-type enumeration from <code>make_function</code>) for those rows between (inclusive) start-row and (exclusive) end-row.
To run the code using some number of parallel threads, use the function 
```
handle = generic.call_c_function_parallel(<func-obj>, <input-blob>, <start-row>, <end-row>, <num-threads>, <access-mode>)
```
access-mode determines how the threads divvy up the data. Currently, two choices are implemented: mode 1 (using the integer 1) strips the data between threads, so that in a 2-thread system, for example, the first thread computes the even rows and the second thread computes the odd rows. The other option, using integer 2, is to split the data up by chunks of rows, so that in a 2 thread system, the first thread gets the first half of the rows, and the second thread gets the second half.
This will return an opaque handle (currently implemented by an integer containing a thread id), which can be put into
```
    generic.wait_for_parallel_finish(handle)
```
which halts computation until all threads represented by handle have completed. 
Note that this method must be called, or else there will be a memory leak, as memory reference counting needs to take place in the main Python thread (or else there would be race conditions and other problems).
See the test files for further examples on how to use.

# Speeding up Python with C -- final notes
Good news first: My project seems to be a complete success, in that it yields better performance than both native Python, and also better performance than numpy, which is considered the industry standard in fast computation in native Python.
## Updates on the project
I was able to give a reasonable demonstration of my numerical improvements for the presentation on Monday, but only with code that hadn't yet been fully optimized, and also hadn't yet been thoroughly completed, as it required (at the time) that all of the C types (e.g. <code>int</code>, <code>double</code>, <code>char*</code>) and Python types (correspondingly, <code>PyIntObject</code>, <code>PyFloatObject</code>, <code>PyStringObject</code>) be hardcoded into my generic library, which is not at all ideal.

However, as I very recently found out, the errors I was getting when trying to generalize were not just bugs, but rather, there turned out to be an underlying issue to the entire project. As you may recall from my previous posts, my C code would modify the underlying Python numeric objects themselves, rather than creating new ones as Python is so eager to do. For example, when the C code would compute the function <code>f(x)=x+1</code>, then the actual <code>ob_ival</code> member of the <code>PyIntObject</code> struct that Python uses to represent integers would be incremented by one. However, strangely, when I made the code around this more complex, bizzare errors started to crop up. After much-too-much debugging, it turns out that I was violating a core assumption Python had about these types: they need to be immutable. The reason they need to be immutable is because as an optimization, Python will keep track of some common objects (ints, strings, etc) that need to be copied around a lot. For these values, however, Python only has one actual object in memory, and many references to it. So, if (as was the case), Python keeps an integer object with value zero in cache, then the next time Python code runs, say, <code>x=0</code>, <code>x</code> will be set to that same object. But, if my code modified the value of this object to be <code>1</code>, then all subsequent references to <code>0</code> would also be changed! It is not hard to see, then, why my initial C code breaks surrounding code.

So, I rewrote the code to, rather than dealing with Python tuples and lists, deal with NumPy arrays, which are mutable by design, as they are implemented as contain a block of raw C values, rather than a block of pointers to immutable Python objects. In some ways, I think this is a good thing; as I mentioned in my presentation, the better memory locality made possible by the NumPy arrays' structure helps make the most efficient possible code. One downside is that NumPy arrays are a little hard to deal with sometimes, especially given their multi-dimensional abilities that I don't use, but it seems to be a favorable trade-off.
## Making the JIT fast
One thing I concentrated on was trying to make the just-in-time C compilation happen more quickly. Since this compilation has to actually call GCC, which isn't known or designed for speed in its own running time, the delay for a run-time JIT is considerable. A realized that a large portion of the work likely came from dealing with all of the headers I had to pull into the code to be JIT'd: both "Python.h", which recursively contains just about every other header in the entire Python source, and various numpy headers, which were needed on top to access the numpy structures I needed due to the problems discussed in the previous section. If you think about it though, I don't really need all that much from these headers. In fact, the only thing needed at all from these headers is the offsets of various members of structures being used. However, these values are already known to the code doing the JIT in the <code>generic</code> module, so there's no need to figure them out all over again in the code being compiled at run-time. So, using the <code>offsetof</code> C macro, I had the generic code actually just insert the raw offsets into code that is being JIT'd, so that no external headers whatsoever are needed. This brings compilation, which I even optimized with <code>-O3 -ffast-math -march=native</code>, down to about 50 ms on my machine, which (see the benchmark results section) is small enough to still get the code to run faster than NumPy.
## Benchmarking notes
One interesting thing I found while attempting to benchmark the code on the University cycle machines is that direct, naive benchmarking can lead to problems. I noticed that, despite being the only user on the server at the time, the running times of programs would vary wildly from iteration to iteration, depending on the order in which I ran the benchmarks. As it turns out (this was confirmed for me via StackOverflow at http://stackoverflow.com/questions/27070839/why-does-my-cpu-suddenly-work-twice-as-fast), this was an issue due to the processors using Intel TurboBoost, which (under appropriate temperatures) allows the CPU to jump to twice its former speed, but taking as long as half to a whole second to do so (and so the results can't be considered negligible). 
## Benchmark results
I created two benchmarks for the code in question to compare against NumPy processing: a polynomial (as discussed in previous posts), and a computation of the mandlebrot set. The polynomial had the following results (output of test_poly.py on my machine):
```
    Manual: 6.970659
    Numpy all at once: 0.070722
    JIT:
     creation: 0.055845
     single-thread: 0.007004
     single-thread in parallel: 0.007194
     two-threads with stride access: 0.016489
     two threads with half each: 0.013909
```
Manual here refers to having a Python loop set each element of a numpy array manually. The NumPy all at once method takes the entire NumPy array as a vector, and computes the polynomial on it directly, which works as NumPy defaults to element-wise addition and multiplication. As I mentioned before, the results here are very nice because this means that even with the cost of creating and linking to new C code at runtime, the C code still outpaces NumPy. Note in the JIT code that a parallel single thread runs ever-so-slightly slower than a same-thread computation, probably due to the overhead of creating and joining a new thread.
Interestingly, the two-thread results are significantly higher than the single thread result, which probably indicates that the computation is memory-bound not CPU-bound (to speculate, it may be the case that the caches for the different processors are conflicting over trying to read and write to the same cache lines, which would explain why stride access is poorer than chunk access). 

The results for the mandlebrot set are:
```
    Numpy reference: 10.061495
    JIT:
     creation: 0.055346
     single-thread: 0.221677
     four threads with stride access: 0.070724
     four threads with chunks: 0.111725
```
Note that the Numpy Reference here refers to a copy of the algorithm provided by SciPy at http://wiki.scipy.org/Tentative_NumPy_Tutorial/Mandelbrot_Set_Example, which I'm taking to be a reasonable target to beat. And indeed, I did beat this, by a very large margin in fact! The pure Python way of doing it was so prohibitively slow that I had to remove it from the benchmark, but suffice it to say that I beat it by a large margin as well. 
I suspect that the multiple-thread case here is better because the execution is primarily CPU-bound rather than memory-bound, and so the extra processors mean something. The reason why the stride is so much better than the chunked input is interesting, but is probably due to a very mundane reason: the inner parts of the set either don't converge at all, or converge only after many iterations. On the other hand, the outer edges of the set can be solved in only a few iterations, as they will very quickly diverge.
It is also worth noting that, probably because I used the <code>-ffast-math</code> optimization on the JIT'd code, the output from my code doesn't perfectly match up with the output of the NumPy code, but they are close enough where it doesn't really make a big difference.

# Next steps
While the project is done insofar as the class is concerned, it's a nice piece of code, and I may work on it more in the future so it could actually be useful
 - It would be nice to try to compare the performance against Cython, as that should (in theory) be about as fast as this method (minus the JIT overhead) since it's compiling directly to hardware.
 - Right now, there is no type or error checking done by the generic module; if the wrong input is sent into the generic functions, a segfault is inevitable. While this is arguably necessary in order to efficient operation, it is worth investigating if safety can be added for a small, possibly constant-time overhead on execcution time.
