#define WASD 1

static int global_static;
extern int global_extern;
int global_data;
volatile int global_volatile_data;

static struct global_static_struct { int d; } global_struct_instance;
struct global_struct {
  int d;
} global_static_struct_instance;

static const char ibmvnic_driver_name[] = "ibmvnic";
static const char ibmvnic_driver_string[] = "IBM System i/p Virtual NIC Driver";

struct struct_wasd {
  int dummy2;
  struct s *w;
};

int
main() {
  static int local_static;
  int local_data;
  volatile int local_volatile_data;
  {
    // shadow BUG
    static int global_static;
    int global_data = WASD;

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
static struct dummy2 { int d; } global_nested_struct_instance;
#endif
#endif
#endif

int (*global_fp)(int adasd, struct asd *);
static int (*global_static_fp)(int adasd, struct asd *);
extern int (*global_extern_fp)(int adasd, struct asd *);
volatile int (*global_volatile_fp)(int adasd, struct asd *);

static int
kkkk(int d) {
  int (*local_fp)(int adasd, struct asd *);
  static int (*local_static_fp)(int adasd, struct asd *);
  volatile int (*local_volatile_fp)(int adasd, struct asd *);
  return 0;
}

struct some_struct {
  int (*struct_fp)(int adasd, struct asd *);
  volatile int (*struct_volatile_fp)(int adasd, struct asd *);
} some_struct_instance;
