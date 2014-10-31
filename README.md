# Iterating over a Python list

One of the most fundamental types in Python is the list object, which allows a programmer to create lists of objects that can be resized and changed dynamically as the program executes, as opposed to tuples, whose size cannot be changed during execution.

Note that lists can contain a heterogeneous collection of types; as one might expect, the following is valid python:

    a = [1, 2, 3]
    b = ["one", "two", "three"]

But at the same time, we can have a list like this:

    c = [1, "two", 3]

Or even like this, where lists are nested inside one another:

    c = [1, [2], [[3, "three"]]]

At the moment, we are concerned with how exactly a list iterates over its elements, regardless of what they are. Of particular interest is how implementation details are exposed by modifying the list while iterating over it.
In order to expose some of the inner workings of Python lists, let's fix ourselves to the following program:


    x = [1, 2, 3]
    x.append(4)
    for i in x:
        x.remove(i)
    print x


Before continuing onward, go ahead and try to predict what the output of this program will be.

.
.
.
.
.
.

Hard, isn't it? I'll give you a hint, it doesn't give an error like many iterators in C++ do, but has an expected and consistent behavior.

The reason it's difficult is because how the list will behave depends a great deal on the underlying data structure python uses to implement the lists, and how modification happens.

Suppose it used a linked-list sort of structure. Then we would expect the first loop to remove each element of x, one at a time, as the head element is being removed from the linked list.

Now, suppose it uses a fixed array and recreates the whole thing on each modification. Then, we would expect each element of x to be removed because the iterator would take the starting value of the list and iterate over that, oblivious to the fact the list is changing.

As it happens, neither of these are correct. Let's dive in and see what the Python interpreter is actually doing.

The python interpreter compiles the script into the following bytecode:

      1           0 LOAD_CONST               0 (10)
                  3 LOAD_CONST               1 (20)
                  6 LOAD_CONST               2 (30)
                  9 BUILD_LIST               3
                 12 STORE_NAME               0 (x)

      2          15 LOAD_NAME                0 (x)
                 18 LOAD_ATTR                1 (append)
                 21 LOAD_CONST               3 (40)
                 24 CALL_FUNCTION            1
                 27 POP_TOP

      3          28 SETUP_LOOP              27 (to 58)
                 31 LOAD_NAME                0 (x)
                 34 GET_ITER
            >>   35 FOR_ITER                19 (to 57)
                 38 STORE_NAME               2 (i)

      4          41 LOAD_NAME                0 (x)
                 44 LOAD_ATTR                3 (remove)
                 47 LOAD_NAME                2 (i)
                 50 CALL_FUNCTION            1
                 53 POP_TOP
                 54 JUMP_ABSOLUTE           35
            >>   57 POP_BLOCK

      5     >>   58 LOAD_NAME                0 (x)
                 61 PRINT_ITEM
                 62 PRINT_NEWLINE
                 63 LOAD_CONST               4 (None)
                 66 RETURN_VALUE

Let's look at how the interpreter handles each line

#Line 1
			 
For the first line, where <code>x</code> is initialized, it isn't hard to see what's going on by eyeballing. The three initial elements of the list, <code>10</code>, <code>20</code>, and <code>30</code>, are loaded from the constants table (which is also generated by the compiler, but isn't shown here), puts the three items on the stack, then creates a list out of them, storing that object into the local variable <code>x</code>.
At this point, let's skip the loading of constants and storing of values; the only important thing to remember is that the stack is integer objects with values <code>10</code> on the bottom, <code>20</code> in the middle, and <code>30</code> on top, and after execution, a single list object made from these three integer objects.

In the main interpreter loop, the following code handles the list-building instruction:

        case BUILD_LIST:
            x =  PyList_New(oparg);
            if (x != NULL) {
                for (; --oparg >= 0;) {
                    w = POP();
                    PyList_SET_ITEM(x, oparg, w);
                }
                PUSH(x);
                continue;
            }
            break;

			
This begins by calling <code>PyList_New</code>, which allocates a new <code>PyListObject</code>. The physical structure of a <code>PyListObject</code> is this:

    typedef struct {
        PyObject_VAR_HEAD
        /* Vector of pointers to list elements.  list[0] is ob_item[0], etc. */
        PyObject **ob_item;

        /* ob_item contains space for 'allocated' elements.  The number
         * currently in use is ob_size.
         * Invariants:
         *     0 <= ob_size <= allocated
         *     len(list) == ob_size
         *     ob_item == NULL implies ob_size == allocated == 0
         * list.sort() temporarily sets allocated to -1 to detect mutations.
         *
         * Items must normally not be NULL, except during construction when
         * the list is not yet visible outside the function that builds it.
         */
        Py_ssize_t allocated;
    } PyListObject;

Note that <code>PyObject_VAR_HEAD</code> is defined as follows, in order to maintain a variable-sized list:

    #define PyObject_VAR_HEAD               \
        PyObject_HEAD                       \
        Py_ssize_t ob_size; /* Number of items in variable part */

<code>PyList_New</code>, in addition to allocating this structure, also initializes the pointer array <code>ob_item</code> to contain <code>3</code> elements (<code>3</code> being the argument to <code>PyList_New</code> from the bytecode)
Then, after <code>PyList_New</code> is run, the <code>PyList_SET_ITEM</code> macro (which simply sets the array elements) is used to set the elements of <code>ob_item</code> to <code>PyIntObject</code> pointers with values <code>10</code>, <code>20</code>, and <code>30</code>, respectively.

#Line 2

Recall the bytecode for the second line of the script:

      2          15 LOAD_NAME                0 (x)
                 18 LOAD_ATTR                1 (append)
                 21 LOAD_CONST               3 (40)
                 24 CALL_FUNCTION            1
                 27 POP_TOP

To implement line 2 of the script, the Python interpreter loads <code>x</code>, loads its <code>append</code> attribute, and calls the function with an argument of a <code>PyIntObject</code> with value 40. Since <code>x</code> will have type <code>PyListObject</code>, its <code>append</code> attribute (through a few boilerplate steps skipped) bring us to the <code>app1</code> method, a simplified version (in other words, a version with all of the error checking removed) of which is below:

    Py_ssize_t n = PyList_GET_SIZE(self);
    list_resize(self, n+1);
    PyList_SET_ITEM(self, n, v);

The <code>list_resize</code> method checks if the required size would exceed the allocated size of <code>ob_item</code>, and if so, the function actually just calls realloc on <code>ob_item</code>, replacing it in-place with an enlarged array (with old elements copied over).

For those of you familiar with C++, this may seem quite familiar: the <code>std::vector</code> class works in a very similar way: pointers to items are kept in a normal array at any given time, but when enough elements are added, the array is resized. It is also worth noting that the allocation size increases exponentially, which has the side-effect that the append operation takes amortized <code>O(1)</code> time, even though each individual add can take up to <code>O(n)</code> time, where n is the number of elements in the list.

#Line 3

Now, the third line of code is where the magic really happens. To repeat from above, the code that actually sets up the for loop is as follows:

    SETUP_LOOP              27 (to 58)
    LOAD_NAME                0 (x)
    GET_ITER
    FOR_ITER                19 (to 57)
    STORE_NAME               2 (i)


Many of you probably remember this block of code from the homework assignment on iterators. As it turns out, almost the exact same thing is happening here as with our Counter class, except that the iter and next methods are different.
To recap, <code>SETUP_LOOP</code> adds a new block to the block stack of the currently executing frame (for our purposes, since there is only one implicit function for the script, this frame is constant throughout execution.) This object's main use, for our purposes, is to remember the current size of the stack, and when the loop is done, since the loop may have leaked many objects onto the stack, remove everything else.
After that point, we put <code>x</code> on top of the stack, and call <code>GET_ITER</code> on it. 
As with our <code>Counter</code> class, as you may recall, <code>GET_ITER</code> looks at the type of <code>x</code> (here it's a <code>PyListObject</code>), and calls the function referenced by the type's <code>tp_iter</code> pointer. For <code>PyListObject</code>, this is the list_iter method, shown below, with extraneous lines removed:

    static PyObject *list_iter(PyObject *seq){
        listiterobject *it;

        it = PyObject_GC_New(listiterobject, &PyListIter_Type);
        it->it_index = 0;
        it->it_seq = (PyListObject *)seq;
        return (PyObject *)it;
    }


What's interesting about this is that the iter method actually creates a new object that iterates over the list, whereas our <code>Counter</code> class just returned itself. A bit of thought should explain why this is the case. What if someone wanted to iterate over the object more than once? Then, the counter would need to be reset each time <code>GET_ITER</code> is called. This may seem fine, but what if someone wanted to nest iteration of a single list, like in the following example which calculates the cartesian product of a list with itself:

	cart = list()
	for a in mylist:
		for b in mylist:
			cart.append((a,b));

If mylist kept track of the iterator's location in itself, this wouldn't work as it would need to contain two different locations at the same time, one corresponding to which element a is, and one corresponding to which element b is.
So, a very thin object of type <code>PyListIter</code> is created, with the following structure:


	typedef struct {
		PyObject_HEAD
		long it_index;
		PyListObject *it_seq; /* Set to NULL when iterator is exhausted */
	} listiterobject;


Additionally, it is this object, rather than the list itself, that implements the next method (that is to say, <code>PyListObject</code> has <code>tp_iternext</code> set to <code>null</code>, while the <code>PyListIter object</code> does have a function.
This function, in fact, is the very core of the list's iteration, a simplified version, with extraneous lines removed, is as follows:

	static PyObject *
	listiter_next(listiterobject *it)
	{
		PyListObject *seq;
		PyObject *item;

		seq = it->it_seq;
		if (seq == NULL)
			return NULL;

		if (it->it_index < PyList_GET_SIZE(seq)) {
			item = PyList_GET_ITEM(seq, it->it_index);
			++it->it_index;
			return item;
		}

		it->it_seq = NULL;
		return NULL;
	}


All this does is simply increment the counter, to indicate that the current position iterated on has increased by one, and if the end of the list has been reached, return null.
One important thing to note here is that it is this method, rather than anything else, that shows what our script will actually do. No matter what actually happens to the <code>PyListObject</code> underneath, the iterator doesn't actually care, it simply increments the index and loads.

#Line 4

Now let's look at what actually happens inside the loop. To save you some scrolling, the bytecode for this line is:

      4          41 LOAD_NAME                0 (x)
                 44 LOAD_ATTR                3 (remove)
                 47 LOAD_NAME                2 (i)
                 50 CALL_FUNCTION            1
                 53 POP_TOP
                 54 JUMP_ABSOLUTE           35
            >>   57 POP_BLOCK

In the first part of line 4, where the function remove is called, the function remove is called on the <code>PyListObject</code> <code>x</code>, the simplified code of which is

	for (i = 0; i < Py_SIZE(self); i++) {
		int cmp = PyObject_RichCompareBool(self->ob_item[i], v, Py_EQ);
		if (cmp > 0) {
			list_ass_slice(self, i, i+1, (PyObject *)NULL);
			return
		}
	}

<code>list_ass_slice</code> is a rather complex method, but for this purpose, simply removes the pointer at index <code>i</code> from the <code>ob_item</code> array, and realigns. It has absolutely no effect on any <code>PyListIter</code> objects associated with it.
The rest of the script's implementation is identical to the operation of the <code>Counter</code> class from the previous homework, so I'll gloss over it. Essentially, the end of the loop always jumps right back to <code>FOR_ITER</code>, which calls into the <code>listiter_next</code> method on the <code>PyListIter</code> object we created at the beginning of the loop. When this method returns null, which it does as soon as the stored index exceeds the current size of the referenced <code>PyListObject</code> (i.e. the value of <code>x</code>).
When the loop exits, <code>POP_BLOCK</code> looks at the block we created in <code>SETUP_LOOP</code>, and pops everything that was added to the stack since the beginning of the loop. The rest of the code is just printing out the value of <code>x</code>, which while interesting in its own right, isn't the focus here, and the print line is just used to make the program do something.

#Conclusion

So, now that we understand how Python iterates over <code>x</code>, can we predict what the output of this program will be, without running it?
Clearly, <code>x</code> starts the loop with <code>ob_size</code> set to <code>4</code> and <code>ob_item</code> a pointer array corresponding to 

	[PyIntObject(10),PyIntObject(20),PyIntObject(30),PyIntObject(40)]
	
and the corresponding <code>PyListIter</code> object pointing to this object with <code>it_index</code> set to <code>0</code>. On the first iteration, i points to <code>PyIntObject(10)</code>, and <code>it_index</code> is incremented to <code>1</code>. Then, <code>i</code> will be removed from <code>x</code>, meaning it now has <code>ob_size=3</code> and a pointer array corresponding to 

	[PyIntObject(20),PyIntObject(30),PyIntObject(40)].
	
However, this means that <code>20</code> is now at position <code>0</code>, and <code>30</code> is at position <code>1</code>, so the iterator will skip over <code>20</code> entirely, pointing <code>i</code> to <code>PyIntObject(30)</code> and setting <code>it_index</code> to <code>2</code>. When <code>30</code> is removed from <code>x</code>, it's <code>ob_size</code> will be only <code>2</code>, so the loop will exit after that, since it_index is past the end of the list.

Thus, the final value of <code>x</code> is <code>[20, 40]</code>, which is the actual printed output of the program.

This is a somewhat surprising answer, and I highly doubt you guessed it at the beginning, but the way list iteration is done in Python, unlike in other languages (Java, for example, will get messed up if you change various List implementations while iterating over it), is totally oblivious to what's actually going on in the list. While this may not be the easiest thing for Python programmers, I hope at this point you can see why it makes sense from the highly-abstracted point of view of the Python interpreter.
