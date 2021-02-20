#define WASD 1

static int global_static;
extern int global_extern;
int global_data;
volatile int global_volatile_data;
static struct dummy { int d; } global_struct_instance;
int main(){
  static int local_static;
  int local_data;
  volatile int local_volatile_data;
{
//shadow
static int global_static;
int global_data=WASD;

++global_static;
++global_data;

}
  return 0;
}

#if 1
  #ifndef DUMMY
  #define DUMMY

    #if defined DUMMY2
static int global_nested_static;
extern int global_nested_extern;
int global_nested_data;
volatile int global_nested_volatile_data;
static struct dummy2 {
  int d;
} global_nested_struct_instance;
    #endif
  #endif
#endif
