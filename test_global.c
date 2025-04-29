wasd_t (*var0)(void);
int (*var1)(void);
struct Struct (*var2)(void);

int (*var4)(int adasd, struct asd *);
static int (*var5)(int adasd, struct asd *);
extern int (*var6)(int adasd, struct asd *);
volatile int (*var7)(int adasd, struct asd *);
int* (*var8)(int adasd, struct asd *);
const int* (*var9)(int adasd, struct asd *);
volatile int* (*var10)(int adasd, struct asd *);
static int* (*var11)(int adasd, struct asd *);
extern int* (*var12)(int adasd, struct asd *);

struct Struct (*var13)(int adasd, struct asd *);
static struct Struct (*var14)(int adasd, struct asd *);
extern struct Struct (*var15)(int adasd, struct asd *);
volatile struct Struct (*var16)(int adasd, struct asd *);
struct Struct* (*var17)(int adasd, struct asd *);
const struct Struct* (*var18)(int adasd, struct asd *);
volatile struct Struct* (*var19)(int adasd, struct asd *);
static struct Struct* (*var20)(int adasd, struct asd *);
extern struct Struct* (*var21)(int adasd, struct asd *);

Struct_t (*var22)(int adasd, struct asd *);
static Struct_t (*var23)(int adasd, struct asd *);
extern Struct_t (*var24)(int adasd, struct asd *);
volatile Struct_t (*var25)(int adasd, struct asd *);
Struct_t* (*var26)(int adasd, struct asd *);
const Struct_t* (*var27)(int adasd, struct asd *);
volatile Struct_t* (*var28)(int adasd, struct asd *);
static Struct_t* (*var29)(int adasd, struct asd *);
extern Struct_t* (*var30)(int adasd, struct asd *);

enum Enum (*var31)(int adasd, struct asd *);
static enum Enum (*var32)(int adasd, struct asd *);
extern enum Enum (*var33)(int adasd, struct asd *);
volatile enum Enum (*var34)(int adasd, struct asd *);
enum Enum* (*var35)(int adasd, struct asd *);
const enum Enum* (*var36)(int adasd, struct asd *);
volatile enum Enum* (*var37)(int adasd, struct asd *);
static enum Enum* (*var38)(int adasd, struct asd *);
extern enum Enum* (*var39)(int adasd, struct asd *);
enum Enum (*var40)(void);
volatile int (*var41)(void);
extern int (*var42)(void);
static int (*var43)(void);

int a1;
int b1 = 1;
int *c1;
int *d1 = NULL;
int *e1 = &a1;
int f1  = DEAD;
int g1  = 0xFFF;
char *h1 = "wasd";
float i1 = 1.1;
char j1[] = "wasd";
char k1[];

struct Struct a2;
struct Struct b2 = {.wasd = 12};
struct Struct *c2;
struct Struct *d2 = NULL;
struct Struct *e2 = &a2;
struct Struct *f2 = a2;

static int a3;
static int b3 = 1;
static int *c3;
static int *d3 = NULL;
static int *e3 = &a1;
static int f3  = DEAD;
static int g3  = 0xFFF;
static char *h3 = "wasd";
static float i3 = 1.1;
static char j3[] = "wasd";
static char k3[];
static char *l3[var + 1] = { "IR", "WhiteLight" };

static struct Struct a4;
static struct Struct b4 = {};
static struct Struct *c4;
static struct Struct *d4 = NULL;
static struct Struct *e4 = &a4;
static struct Struct *f4 = a4;

extern int a5;
extern int *c5;
extern char *h5;
extern float i5 ;
extern char j5[];

extern struct Struct a6;
extern struct Struct *c6;

struct Struct a7[2];
struct Struct b7[2] = {{.wasd = 12}};
struct Struct *c7[2];
struct Struct *d7[2] = {NULL};
struct Struct *e7[1]    = {&a2};
struct Struct *f7[1]    = {c2};
struct Struct *g7[var]    = {c2};
struct Struct *h7[var];
struct Struct i7[var+1];

int a8[2];
int b8[2] = {1};
int *c8[2];
int *d8[2] = {NULL};
int *e8[2] = {&a1};
int f8[2]  = {DEAD};
int g8[2]  = {0xFFF};
char* h8[2]  = {"a","b"};
float i8[2]  = {1.1f, 1.0};
char j8[] = "wasd";
char k8[];
char *l8[var + 1] = { "IR", "WhiteLight" };

static struct Struct a9[2];
static struct Struct b9[2] = {{.wasd = 12}};
static struct Struct *c9[2];
static struct Struct *d9[2] = {NULL};
static struct Struct *e9[1]    = {&a2};
static struct Struct *f9[1]    = {c2};

static int a10[2];
static int b10[2] = {1};
static int *c10[2];
static int *d10[2] = {NULL};
static int *e10[2] = {&a1};
static int f10[2]  = {DEAD};
static int g10[2]  = {0xFFF};
static char* h10[2]  = {"a","b"};
static float i10[2]  = {1.1f, 1.0};
static char j10[] = "wasd";
static char k10[];
static char *l10[var + 1] = { "IR", "WhiteLight" };

volatile int a11;
volatile int b11 = 1;
volatile int *c11;
volatile int *d11 = NULL;
volatile int *e11 = &a1;
volatile int f11  = DEAD;
volatile int g11  = 0xFFF;
volatile char* h11  = "wasd";
volatile float i11 = 1.1f;

static volatile int a12;
static volatile int b12 = 1;
static volatile int *c12;
static volatile int *d12 = NULL;
static volatile int *e12 = &a1;
static volatile int f12  = DEAD;
static volatile int g12  = 0xFFF;
static volatile float i12  = 0.1f;

static const int a13;
static const int b13 = 1;
static const int *c13;
static const int *d13 = NULL;
static const int *e13 = &a1;
static const int f13  = DEAD;
static const int g13  = 0xFFF;
static const char *h13  = "wasd";
static const char j13[] = "wasd";
static const char k13[];

const int a14;
const int b14 = 1;
const int *c14;
const int *d14 = NULL;
const int *e14 = &a1;
const int f14  = DEAD;
const int g14  = 0xFFF;
const char *h14  = "wasd";
const float i14 = 1.1;

const struct Struct a15;
const struct Struct b15 = {.wasd = 12};
const struct Struct *c15;
const struct Struct *d15 = NULL;
const struct Struct *e15 = &a2;
const struct Struct *f15 = a2;

static const struct Struct a16;
static const struct Struct b16 = {};
static const struct Struct *c16;
static const struct Struct *d16 = NULL;
static const struct Struct *e16 = &a4;
static const struct Struct *f16 = a4;

extern const int a17;
extern const int *c17;
extern const char *h17;
extern const float i17;
extern const char j17[];

const struct Struct a18;
const struct Struct *c18;

//

struct Struct2 { int d; } a19;
static struct Struct3 { int d; } b19;
struct Struct4 { int d; } c19[2];
static struct Struct5 { int d; } d19[];
struct Struct6 { int d; } e19[2] = {{1}, {.d=2}};
static struct Struct7 { int d; } f19[] = {{.d=1}, {2}};
enum _enum_type1 {
  ENUM_TYPE2,
} g19;
static enum _enum_type2 {
  ENUM_TYPE0,
} h19;
enum _enum_type1 {
  ENUM_TYPE2,
} i19[2];
static enum _enum_type2 {
  ENUM_TYPE1,
} j19[2];
enum _enum_type1 {
  ENUM_TYPE2,
} k19[2] = {ENUM_TYPE2,ENUM_TYPE2};
static enum _enum_type2 {
  ENUM_TYPE2,
} l19[2] = {ENUM_TYPE3, ENUM_TYPE4};

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

Struct_t a20;
Struct_t b20 = {.wasd = 12};
Struct_t *c20;
Struct_t *d20 = NULL;
Struct_t *e20 = &a2;
Struct_t *f20 = a2;


static const int a21 = CALL(b20);
const int b21 = CALL(b20);
static int c21 = CALL(b20);
int d21 = CALL(b20);


static Struct_t a22;
static Struct_t b22 = {.wasd = 12};
static Struct_t *c22;
static Struct_t *d22 = NULL;
static Struct_t *e22 = &a2;
static Struct_t *f22 = a2;

const Struct_t a23;
const Struct_t b23 = {.wasd = 12};
const Struct_t *c23;
const Struct_t *d23 = NULL;
const Struct_t *e23 = &a2;
const Struct_t *f23 = a2;

static const Struct_t a24;
static const Struct_t b24 = {.wasd = 12};
static const Struct_t *c24;
static const Struct_t *d24 = NULL;
static const Struct_t *e24 = &a2;
static const Struct_t *f24 = a2;

extern const Struct_t a25;
extern const Struct_t b25 = {.wasd = 12};
extern const Struct_t *c25;
extern const Struct_t *d25 = NULL;
extern const Struct_t *e25 = &a2;
extern const Struct_t *f25 = a2;

extern const Struct_t a26;
extern const Struct_t b26 = {.wasd = 12};
extern const Struct_t *c26;
extern const Struct_t *d26 = NULL;
extern const Struct_t *e26 = &a2;
extern const Struct_t *f26 = a2;


DMA_HandleTypeDef hdma_adc1;

static uint32_t ir0_set_power = 0;
STATIC uint32_t ir1_set_power = 0;

char *a27[] = { "No", "Temporary", "Power-on", "Permanent" };
type *b27[] = { asd,dasdk };
static char *c27[] = { "No", "Temporary", "Power-on", "Permanent" };
static type *d27[] = { dfa, gfsdf };
char *e27[4] = { "No", "Temporary", "Power-on", "Permanent" };
type *f27[2] = { asd,dasdk };
static char *g27[4] = { "No", "Temporary", "Power-on", "Permanent" };
static type *h27[2] = { dfa, gfsdf };
static enum type *i27[2] = { dfa, gfsdf };
static struc type *j27[2] = { dfa, gfsdf };
const char *k27[4] = { "No", "Temporary", "Power-on", "Permanent" };
const type *l27[2] = { asd,dasdk };
static const char *m27[4] = { "No", "Temporary", "Power-on", "Permanent" };
static const type *n27[2] = { asd,dasdk };

extern gpio_pin_t *a28[];
static const uint8_t b28[2][4] = {
    { 0, 0, 1, 1 },
    { 1, 0, 0, 1 },
};
enum type c28[LENS_MAX][NBR_OF_OPTICS];
struct resolution_config **d28[MAX_NUMBER_OF_SETS];
static gchar *e28[MAX_SUB_GROUPS_NUMBER][MAX_SUB_LIST_LENGTH];
static gchar *f28[1][MAX_SUB_LIST_LENGTH];
static gchar *g28[MAX_SUB_GROUPS_NUMBER][4893+1];
static gchar *h28[1][2];
gchar *j28[MAX_SUB_GROUPS_NUMBER][MAX_SUB_LIST_LENGTH];
gchar *k28[1][MAX_SUB_LIST_LENGTH];
gchar *l28[MAX_SUB_GROUPS_NUMBER][4893+1];
gchar *m28[1][2];
extern gchar *n28[MAX_SUB_GROUPS_NUMBER][MAX_SUB_LIST_LENGTH];
extern gchar *o28[1][MAX_SUB_LIST_LENGTH];
extern gchar *q28[MAX_SUB_GROUPS_NUMBER][4893+1];
extern gchar *r28[1][2];
struct wasd *(*s28)(int a) = NULL;
int (*t28)(void) = NULL;
int* (*u28)(struct wasd *) = NULL;
struct wasd (*v28)(int a) = NULL;
enum wasd (*x28)(int a) = NULL;
STATIC gdouble (*y28)(void) = NULL;
static gdouble (*z28)(void) = NULL;

// should not highlight anything {
typedef int (*type0_t)(void);
typedef struct Struct (*type1_t)(void);
typedef type_t (*type2_t)(void);

struct _FocusPositionData; 
typedef struct _FocusPositionData XXX;

typedef struct {
  int dummy;
} XXX2;

struct YYY {
  int dummy;
};

enum _enum_type;


typedef enum _enum_type {
  ENUM_TYPE,
} emum_type;

typedef enum {
  ENUM_TYPE5,
} ZZZ;

typedef int AA;
typedef AA BB;
typedef enum _enum_type2 CC;
typedef struct YYY DD;

/* static __FORCEINLINE void __O3 stepper_interrupt_for_channel(uint8_t channel) { */
/* } */
// }
