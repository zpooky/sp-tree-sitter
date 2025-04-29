#define WASD 1

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
