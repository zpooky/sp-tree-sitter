#include <stdlib.h>
#include <string>
#include <vector>

/* TODO for member methods in cpp we want to include the class in the prinrout aka "class-name:%s", __func__*/

enum class wasd {
  DAY,
  NIGHT,
  MOVING,
};
typedef struct { // we should generate for this
  struct { // we should not generate for this since there is no type name
    void *buffer;
    int size;
  } memref;
  struct {
    int a;
    int b;
    struct {
      int a2;
      int b2;
    } v2;
  } value;
  wasd w;
  std::vector<int> v;
  vector<int> v2;
  std::string &a;
  std::string b;
  std::string *c;
} TEE_Param;

typedef class klass {
    int package_prot_a;
  private:
    int priv_a;
  public:
    int pub_a;
  protected:
    int prot_a;

  protected:
} klass_t;

class klass2 {
  public:
    constexpr static unsigned int retry_time = 2;
    int var;
    klass2();
  void member1();
  void member2(){//TODO
      int k;

  }
};

void klass2::member1(){
      int k;
}

klass2::klass2(string name) : var(0) {
}

namespace test {
  void fun(int l){
  }

  class klass3 {
    public:
      int var;
      klass3(string name) : var(0) {//TODO
      int k;
      }
    void member1(int l);
    void member2(){//TODO
      int k;
    }
  };

  void klass3::member1(int l){
      int k;
  }
}

class klass4 : klass2 {//TODO
  public:
    int var;
    klass4(string name) : var(0) {//TODO
      int k;
    }
    void member2(){//TODO
      int k;
    }
};

int0
wasd(std::string &a, std::string b, const std::string &c, const std::string *d, string &e, const string &f, ::string &g, a::b::string &h)
{
}

int1
asd(std::vector<int>&v){
}

int2
member::meth(int k) const {
}

