# Motivation
Python is a fantastic language in many ways. It fails in ways that are much clearer to understand than most programming languages, and it is similar enough to pseudocode that one doesn't need to know the language to understand what's going on. Among many non-tech-savvy scientists, it is the programming language of choice for these reasons. However, there is a downside: python execution is quite slow.

For many simple scripts, this is not a problem; for example, when we want to download a series of files from the internet, almost all of the time will be spent waiting for the transfer to complete, so the language speed doesn't matter. For many scientific calculations, too, libraries already exist that perform optimized computations of known functions, for example NumPy, which can (among many other things) quickly compute the standard deviation of a list, a task for which native Python would be exceedingly slow.

However, one cannot always assume that the most computationally expensive parts of a program--the hot loops--have corresponding libraries that can solve them efficiently. What if a scientist who isn't a very skilled programmer wants to, say calculate the value of a polynomial like <code>10x^5+4x^4+x^3-20x^2+3</code>? The python compiler will actually optimize this to some extent, but it will ultimately remain a series of <code>BINARY_ADD</code> and <code>BINARY_MULTIPLY</code> instructions, with a pinch of optimizing <code>BINARY_XOR</code> instructions. Yet each of these takes many CPU cycles to compute, even though any CPU architecture is going to have individual instructions for each. Furthermore, it stands to reason that while this is being computed, especially if we want to run this polynomial on a large dataset, we could 

# Project
My project will be to set up a Python library that can be used to calculate functions like the above very quickly by having the Python interpreter execute specially-crafted native code for x86 (or, in principle, any architecture to which C can compile). I will organize my project by first setting out to accomplish my proof-of-concept, then extend it in the secondary goals below. Pending time, I will also try to accomplish some of my stretch goals below
## Proof-of-concept goal
The proof of concept will be to write a C function to calculate the example polynomial from earlier, <code>10x^5+4x^4+x^3-20x^2+3</code>, and have a python script use this function on a large dataset. This script must be able to run without having to recompile Python, as this would be a high entry bar for non-technical users. Then, I will compare the performance against python running natively generated code for this calculation.
## Secondary goals
- The calculations can run asynchronously to the python function by using another thread (which easily leads to parallelism in the data)
- There are no memory leaks
- Arbitrary C functions could be run; the specific function I'm writing doesn't need to be hardcoded anywhere in the python library
## Stretch goals
- The python run line (e.g. "python foo.py") does not need to be altered in order for the library to work
- Some sort of JIT or partial JIT so that arbitrary polynomials (or perhaps more generally functions) can be computed as the Python script runs.
- Can run existing native functions without needing to specially recompile them for use with the Python library
