/**
	@file
	yarp		- yarp
				- MaxMSP external with interfaces to yarp.
				- requires libACE.dylib (OSX) or ACE(d).dll (Windows)
	Johnty Wang -  johntywang@gmail.com
	2014 - Hplus Canraie M+M project
*/

#undef check //for OSX

#include "ext.h"
#include "ext_obex.h"
#include "ext_strings.h"
#include "ext_common.h"
#include "ext_systhread.h"
#include "yarp/os/all.h"

#include <vector>
using namespace std;

// a wrapper for cpost() only called for debug builds on Windows
// to see these console posts, run the DbgView program (part of the SysInternals package distributed by Microsoft)
#if defined( NDEBUG ) || defined( MAC_VERSION )
#define DPOST
#else
#define DPOST cpost
#endif

// a macro to mark exported symbols in the code without requiring an external file to define them
#ifdef WIN_VERSION
	// note that this is the required syntax on windows regardless of whether the compiler is msvc or gcc
	#define T_EXPORT __declspec(dllexport)
#else // MAC_VERSION
	// the mac uses the standard gcc syntax, you should also set the -fvisibility=hidden flag to hide the non-marked symbols
	#define T_EXPORT __attribute__((visibility("default")))

#endif

// a c++ class representing a number, and types for a vector of those numbers
class number {
	private:
		double value;
	public:
		number(double& newValue)
		{
			value = newValue;
		}

		void setValue(const double& newValue)
		{
			value = newValue;
		}

		void getValue(double& retrievedValue)
		{
			retrievedValue = value;
		}
};
typedef std::vector<number>		numberVector;
typedef numberVector::iterator	numberIterator;


// max object instance data
typedef struct _yarp {
	t_object			c_box;
	t_systhread			x_systhread;
	t_systhread_mutex	x_mutex;
	int					x_polltime_ms;
	bool				x_stop_thread;
	void				*c_outlet;
	void				*c_inlet;

	yarp::os::Network *yarp;
	yarp::os::BufferedPort<yarp::os::Bottle> *port;
	yarp::os::Bottle *out;
} t_yarp;


// prototypes
void*	yarp_new(t_symbol *s, long argc, t_atom *argv);
//void*   yarp_new(t_symbol *s);
void	yarp_free(t_yarp* x);
void	yarp_assist(t_yarp *x, void *b, long m, long a, char *s);
void	yarp_bang(t_yarp *x);
void	yarp_count(t_yarp *x);
void	yarp_int(t_yarp *x, long value);
void	yarp_float(t_yarp *x, double value);
void	yarp_list(t_yarp *x, t_symbol *msg, long argc, t_atom *argv);
void	yarp_clear(t_yarp *x);
//setup as yarp reader
void	yarp_read_setup(t_yarp *x, symbol *s);

void	yarp_readbang(t_yarp *x);
void	yarp_poll(t_yarp *x, long value);
void	yarp_start_readthread(t_yarp *x, symbol *s);
void	*yarp_readthread(t_yarp *x);
void	yarp_readthread_stop(t_yarp *x);

//setup as yarp writer
void	yarp_write_setup(t_yarp *x, symbol *s);

void	yarp_send(t_yarp *x, t_symbol *msg, long argc, t_atom *argv);

//setup yarp miniserver address
bool	yarp_addr_setup(t_yarp *x, symbol *s);

//yarp connect
void	yarp_connect(t_yarp *x, symbol *s);

// yarp init
void	init_yarp(t_yarp	*x);

bool	checkAddr(char* addr);


// globals
static t_class	*s_yarp_class = NULL;

/************************************************************************************/

int T_EXPORT main(void)
{
	t_class	*c = class_new("yarp",
							(method)yarp_new,
							(method)yarp_free,
							sizeof(t_yarp),
							(method)NULL,
							A_GIMME,
							0);

	common_symbols_init();

	class_addmethod(c, (method)yarp_bang,	"bang",			0);
	class_addmethod(c, (method)yarp_int,	"int",			A_LONG,	0);
	class_addmethod(c, (method)yarp_float,	"float",		A_FLOAT,0);
	class_addmethod(c, (method)yarp_list,	"list",			A_GIMME,0);
	class_addmethod(c, (method)yarp_clear,	"clear",		0);
	class_addmethod(c, (method)yarp_count,	"count",		0);
	class_addmethod(c, (method)yarp_assist, "assist",		A_CANT, 0);
	class_addmethod(c, (method)yarp_addr_setup, "addr",		A_SYM, 0);
	class_addmethod(c, (method)yarp_read_setup,   "read",			A_SYM, 0);
	class_addmethod(c, (method)yarp_write_setup,   "write",			A_SYM, 0);
	class_addmethod(c, (method)yarp_send,   "send",			A_GIMME, 0);
	class_addmethod(c, (method)yarp_poll,	"poll",			A_LONG,	0);

	class_addmethod(c, (method)yarp_connect, "connect",		A_SYM, 0);
	class_addmethod(c, (method)stdinletinfo,	"inletinfo",	A_CANT, 0);
	
	class_register(_sym_box, c);
	s_yarp_class = c;

	return 0;
}


/************************************************************************************/
// Object Creation Method

void *yarp_new(t_symbol *s, long argc, t_atom *argv)
{
	poststring("new yarp!\n");
	char str[32];
	sprintf(str, "num args=%ld", argc);
	poststring(str);
	
	t_yarp	*x;

	x = (t_yarp*)object_alloc(s_yarp_class);
	if (x) {
		
		systhread_mutex_new(&x->x_mutex, 0);
		x->x_polltime_ms = 50; //poll every ms by default
		x->c_outlet = outlet_new(x, NULL);
		x->c_inlet = inlet_new(x, NULL);
		x->x_stop_thread = true; //default: no thread running
		
	}

	init_yarp(x);
	bool port_success = false;
	if (argc==1) {
		char str[32];
		sprintf(str, "%s", atom_getsym(argv)->s_name);
		post("init yarp address = %s",str);
		if (checkAddr(str))
			yarp_addr_setup(x, atom_getsym(argv));
		else {
			yarp_addr_setup(x, gensym("/yarpMax"));
		}
	}
	else { //default name of address
		yarp_addr_setup(x, gensym("/yarpMax"));
	}
	return(x);
}

void init_yarp(t_yarp	*x) {
	x->yarp = new yarp::os::Network();
	x->port = new yarp::os::BufferedPort<yarp::os::Bottle>();
	//x->out = new yarp::os::Bottle;
}


void yarp_free(t_yarp *x)
{
	x->port->close();
	
	yarp_readthread_stop(x);

	x->port->unprepare();
	delete x->port;
	//delete x->out;
	
	
	delete x->yarp;
	
	systhread_mutex_free(x->x_mutex);
}


/************************************************************************************/
// Methods bound to input/inlets

void yarp_assist(t_yarp *x, void *b, long msg, long arg, char *dst)
{
	if (msg==1) {
		strcpy(dst, "set up yarp (read /XX or write /XX)");
		if (arg == 1)
			strcpy(dst, "write input");
	}
	else if (msg==2)
		strcpy(dst, "output");
	
}


void yarp_bang(t_yarp *x)
{
}


void yarp_count(t_yarp *x)
{
	
}

//sets up object for write mode. (no readthread)
void yarp_write_setup(t_yarp *x, symbol *s)
{
	post("setting up as writer");
	if (yarp_addr_setup(x, s)) {
		//if (read)thread currently running, stop it
		if (x->x_stop_thread == false) {
			yarp_readthread_stop(x);
		}
	}

}


//sets read message from patch: (re)set up read port, and initiate read thread
void yarp_read_setup(t_yarp *x, symbol *s)
{
	post("setting up as reader");
	if (yarp_addr_setup(x, s)) {
		//if thread currently running, stop it
		if (x->x_stop_thread == false) {
			yarp_readthread_stop(x);
		}

		yarp_addr_setup(x, s);

		x->x_stop_thread = false; //set flag to get thread going
		systhread_create((method) yarp_readthread, x, 0, 0, 0, &x->x_systhread);
		
		//testing: just pass the input directly out
		t_atom* argv = NULL;
		argv = new t_atom();
		atom_setsym(argv, s);
		outlet_anything(x->c_outlet, gensym("output"), 1, argv);
		delete argv;
	}
}

void yarp_start_readthread(t_yarp *x, symbol *s) {

}

//start read thread

void *yarp_readthread(t_yarp *x) 
{
	while (1) {
		if (x->x_stop_thread)
			break;
		systhread_mutex_lock(x->x_mutex);
		systhread_sleep(x->x_polltime_ms);
		systhread_mutex_unlock(x->x_mutex);
		//post("thread proc fn!");
		//todo: add read processing here

		if (x->port->getPendingReads()) {
            post(">>>read:");
            yarp::os::Bottle *input = x->port->read();
            if (input != NULL) {
                t_atom* argv = new t_atom();
                atom_setsym(argv, gensym(input->toString().c_str()));
                outlet_anything(x->c_outlet, gensym("read"), 1, argv);
                delete argv;
            }
        }
	}
	systhread_exit(0);
	return NULL;
}

void yarp_readthread_stop(t_yarp *x)
{
	unsigned int ret;
	if (x->x_systhread) {
		post("stopping read thread");
		x->x_stop_thread = true;
		systhread_join(x->x_systhread, &ret);
		x->x_systhread = NULL;
	}
}

void yarp_int(t_yarp *x, long value)
{
	yarp_float(x, value);
	post("int val = %ld", (long)value);
}

void yarp_poll(t_yarp *x, long value)
{
	if (value < 10) {
		post("error! min poll time is 10 ms!");
	}
	else {
		post("poll time changed to %ld ms", (long)value);
		systhread_mutex_lock(x->x_mutex);
		x->x_polltime_ms = value;
		systhread_mutex_unlock(x->x_mutex);
	}
}


void yarp_float(t_yarp *x, double value)
{

}


void yarp_list(t_yarp *x, t_symbol *msg, long argc, t_atom *argv)
{

	char str[32];
	sprintf(str, "num= %ld",argc);
	poststring(str);
}


void yarp_clear(t_yarp *x)
{

}

bool checkAddr(char* addr) {
	if (addr[0] != '/') {
		error("port name must start with '/'");
		return false;
	}
	else {
		return true;
	}
}

bool yarp_addr_setup(t_yarp *x, symbol *s) {
	bool port_success = false;
	if (!x->port->isClosed())
		x->port->close();	
	if (checkAddr(s->s_name)) {
		port_success = x->port->open(s->s_name);
	}
	return port_success;
}


void yarp_send(t_yarp *x, t_symbol *msg, long argc, t_atom *argv) {
	x->out = &(x->port->prepare());
	x->out->clear();
	for (int i=0; i<argc; i++) {
		x->out->addString( atom_getsym(argv+i)->s_name );
		post(atom_getsym(argv+i)->s_name);
	}
	x->port->write();
}

void yarp_connect(t_yarp *x, symbol *s)
{
}