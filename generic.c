#include "Python.h"
#define NPY_NO_DEPRECATED_API 0x00000008 // v1.9
#include "arrayobject.h"
#undef _POSIX_C_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include "pthread.h"

#define TUP_OF_NPARRAYS 1

#define MODE_STRIDE 1
#define MODE_CHUNK 2

typedef struct {
	double (*fn)(double);
	PyListObject* array;
} worker_args_d_dd;

typedef void (*func_t)(void*,int,int,int);

typedef struct {
	PyObject_HEAD
	func_t func;
	void* lib_handle;
} PyCFuncObject;

void dealloc_func(PyCFuncObject* obj){
	dlclose(obj->lib_handle);
}

PyTypeObject PyCFunc_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "C function",
    sizeof(PyCFuncObject),
    0,
    (destructor) dealloc_func,                    /* tp_dealloc */
    0,                       /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                       /* tp_compare */
    0,            /* tp_repr */
    0,                             /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                                          /* tp_call */
    0,            /* tp_str */
    0,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    0,          /* tp_flags */
    0,                                    /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    0,                                /* tp_methods */
    0,                                          /* tp_members */
    0,                                 /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    0,                                          /* tp_init */
    0,                                          /* tp_alloc */
    0,                                    /* tp_new */
    0,                         /* tp_free */
};

PyObject* call_c_function(PyObject* self, PyObject* args){
	PyTupleObject* tup = (PyTupleObject*)args;

	PyCFuncObject* fn = (PyCFuncObject*)tup->ob_item[0];
	PyTupleObject* realargs = (PyTupleObject*)tup->ob_item[1];
	PyIntObject* start = (PyIntObject*)tup->ob_item[2];
	PyIntObject* end = (PyIntObject*)tup->ob_item[3];

	fn->func(realargs, start->ob_ival, end->ob_ival, 1);
	Py_INCREF(args);
	return args;
}

typedef struct{
	func_t fn;
	void* args;
	pthread_t thread;
	int start;
	int end;
	int access_pattern;
} worker_arg_t;

void* worker(void* p){
	worker_arg_t* arg = (worker_arg_t*)p;
	arg->fn(arg->args,arg->start,arg->end,arg->access_pattern);
	return NULL;
}

void* main_worker(void* p){
	worker_arg_t* arg = (worker_arg_t*)p;
	int t = (int)arg->thread;
	worker_arg_t* threads = alloca(t*sizeof(worker_arg_t));

	void* args = arg->args;
	switch(arg->access_pattern){
	case MODE_STRIDE:
		for(int i = 0; i < t; i++){
			threads[i].fn = arg->fn;
			threads[i].args = arg->args;
			threads[i].start = arg->start+i;
			threads[i].end = arg->end;
			threads[i].access_pattern = t;
			if(i<t-1){
				int d = pthread_create(&threads[i].thread, NULL, &worker, &threads[i]);
			}
		}
		break;
	case MODE_CHUNK:
		for(int i = 0; i < t; i++){
			threads[i].fn = arg->fn;
			threads[i].args = arg->args;
			int a = arg->start;
			int b = arg->end;
			threads[i].start = a+(b-a)*i/t;
			threads[i].end = a+(b-a)*(i+1)/t;
			threads[i].access_pattern = 1;
			if(i<t-1) pthread_create(&threads[i].thread, NULL, &worker, &threads[i]);
		}
		break;
	default:
		goto end;
	}

	worker(&threads[t-1]);

	for(int i = 0; i < t-1; i++){
		pthread_join(threads[i].thread, NULL);
	}

end:
	return arg;
}

PyObject* call_c_function_parallel(PyObject* self, PyObject* args){
	PyTupleObject* tup = (PyTupleObject*)args;

	PyCFuncObject* fn = (PyCFuncObject*)tup->ob_item[0];
	PyTupleObject* realargs = (PyTupleObject*)tup->ob_item[1];
	PyIntObject* start = (PyIntObject*)tup->ob_item[2];
	PyIntObject* end = (PyIntObject*)tup->ob_item[3];
	PyIntObject* threads = (PyIntObject*)tup->ob_item[4];
	PyIntObject* access_mode = (PyIntObject*)tup->ob_item[5];

	pthread_t tid;
	worker_arg_t* arg = malloc(sizeof(worker_arg_t));
	arg->fn = fn->func;
	arg->args = realargs;
	Py_INCREF(realargs);
	arg->thread = threads->ob_ival;
	arg->start = start->ob_ival;
	arg->end = end->ob_ival;
	arg->access_pattern = access_mode->ob_ival;

	int err = pthread_create(&tid, NULL, &main_worker, arg);
	long l;
	if(err == 0){
		l = (long)tid;
	} else {
		l = -1;
	}

	return (PyObject*)PyInt_FromLong(l);
}

PyObject* wait_for_parallel_finish(PyObject* self, PyObject* args){
	PyTupleObject* tup = (PyTupleObject*)args;
	PyIntObject* i = (PyIntObject*)tup->ob_item[0];
	long l = i->ob_ival;

	if(l == -1) return NULL;

	worker_arg_t* orig_arg;
	pthread_join((pthread_t)l, (void**)&orig_arg);
	PyObject* retval = (PyObject*)orig_arg->args;
	free(orig_arg);
	return retval;
}

typedef struct {
	char* c_name;
	char* py_name;
	int val_offset;
} ctype_t;

#define NUM_CTYPES 3
ctype_t ctype[NUM_CTYPES] = {
	{"int","PyIntObject",offsetof(PyIntObject,ob_ival)},
	{"double","PyFloatObject",offsetof(PyFloatObject,ob_fval)},
	{"char*","PyStringObject",offsetof(PyStringObject,ob_sval)}
};

int find_type(char* name){
	for(int i = 0; i<NUM_CTYPES; i++){
		if(strcmp(ctype[i].c_name,name) == 0){
			return i;
		}
	}
	return -1;
}

PyObject* make_c_function(PyObject* self, PyObject* args){
	PyTupleObject* tup = (PyTupleObject*)args;

	long m_type = ((PyIntObject*)tup->ob_item[0])->ob_ival;
	char* code = ((PyStringObject*)tup->ob_item[1])->ob_sval;
	PyListObject* argtypelist = (PyListObject*)tup->ob_item[2];
	Py_ssize_t numargs = argtypelist->ob_size;
	PyTupleObject** argpairs = (PyTupleObject**)argtypelist->ob_item;
	char* outtype = ((PyStringObject*)tup->ob_item[3])->ob_sval;

	FILE *f = fopen(".temp-python-code.c", "w");
	if(f == NULL) return NULL;
	fprintf(f, "#include <stdint.h>\n");
	fprintf(f, "static %s f(", outtype);
	for(int i = 0; i < numargs-1; i++){
		fprintf(f, "%s %s,", ((PyStringObject*)argpairs[i]->ob_item[0])->ob_sval, ((PyStringObject*)argpairs[i]->ob_item[1])->ob_sval);
	}
	fprintf(f, "%s %s){%s}", ((PyStringObject*)argpairs[numargs-1]->ob_item[0])->ob_sval, ((PyStringObject*)argpairs[numargs-1]->ob_item[1])->ob_sval,code);

	switch(m_type){
	case TUP_OF_NPARRAYS:
		fprintf(f, "\nvoid* g(void* t,int a,int b,int k){\nchar* o=*(char**)((char*)t+%u);\n", offsetof(PyTupleObject,ob_item[numargs]));
		for(int i = 0; i < numargs; i++){
			fprintf(f, "char* o%x=*(char**)((char*)t+%u);\n", i, offsetof(PyTupleObject,ob_item[i]));
		}

		fprintf(f, "char* d=*(char**)(o+%u);\n", offsetof(PyArrayObject_fields, data));
		for(int i = 0; i < numargs; i++){
			fprintf(f, "char* d%x=*(char**)(o%x+%u);\n", i, i, offsetof(PyArrayObject_fields,data));
		}

		fprintf(f, "intptr_t s=**(intptr_t**)(o+%u);\n", offsetof(PyArrayObject_fields, strides));
		for(int i = 0; i < numargs; i++){
			fprintf(f, "intptr_t s%x=**(intptr_t**)(o%x+%u);\n", i, i, offsetof(PyArrayObject_fields, strides));
		}

		fprintf(f, "int p;\nfor(p=a;p<b;p+=k){\n");
		fprintf(f, "%s m=f(", outtype);
		for(int i = 0; i < numargs-1; i++){
			char* t = ((PyStringObject*)argpairs[i]->ob_item[0])->ob_sval;
			int c = find_type(t);
			if(c==-1) return NULL;
			fprintf(f, "*(%s*)(d%x+p*s%x),",t,i,i);
		}
		char* t = ((PyStringObject*)argpairs[numargs-1]->ob_item[0])->ob_sval;
		int c = find_type(t);
		if(c == -1) return NULL;
		fprintf(f, "*(%s*)(d%x+p*s%x));\n", t, numargs-1, numargs-1);

		c = find_type(outtype);
		if(c==-1) return NULL;
		fprintf(f, "*(%s*)(d+p*s)=m;\n", outtype, ctype[c].val_offset);

		fprintf(f,"}\n}");
		break;
	default:
		return NULL;
	}
	fclose(f);

	int retval = system("cc -g -O3 -ffast-math -march=native -fPIC -shared .temp-python-code.c -o .temp-python-code.so");
	if(retval != 0){
		return NULL;
	}

	void* module = dlopen("./.temp-python-code.so", RTLD_LAZY);
	if(module == NULL){
		return NULL;
	}

	func_t fn = (func_t)dlsym(module, "g");
	if(fn == NULL) return NULL;

	PyCFuncObject* ans = PyObject_New(PyCFuncObject, &PyCFunc_Type);
	ans->func = fn;
	ans->lib_handle = module;
	return (PyObject*)ans;
}

static PyMethodDef methods[] = {
	{"make_c_function", make_c_function, METH_VARARGS, NULL},
	{"call_c_function", call_c_function, METH_VARARGS, NULL},
	{"call_c_function_parallel", call_c_function_parallel, METH_VARARGS, NULL},
	{"wait_for_parallel_finish", wait_for_parallel_finish, METH_VARARGS, NULL},
	// terminator:
	{NULL}
};

PyMODINIT_FUNC initgeneric(void){
	Py_InitModule("generic", methods);
}

