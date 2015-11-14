#include "ruby_prof.h"

static VALUE cMeasureConstMisses;

#if defined(HAVE_RB_VM_CONST_MISSING_COUNT)
  VALUE rb_vm_const_missing_count(void);
#endif

static double
measure_const_misses()
{
#if defined(HAVE_RB_VM_CONST_MISSING_COUNT)
#define MEASURE_CONST_MISSES_ENABLED Qtrue
  return NUM2ULL(rb_vm_const_missing_count());
#else
#define MEASURE_CONST_MISSES_ENABLED Qfalse
  return 0;
#endif
}

prof_measurer_t* prof_measurer_const_misses()
{
  prof_measurer_t* measure = ALLOC(prof_measurer_t);
  measure->measure = measure_const_misses;
  return measure;
}

/* call-seq:
   measure -> int

Returns the number of constant resolution failures.*/
static VALUE
prof_measure_const_misses(VALUE self)
{
#if defined(HAVE_LONG_LONG)
    return ULL2NUM(measure_const_misses());
#else
    return ULONG2NUM(measure_const_misses());
#endif
}

void rp_init_measure_const_misses()
{
    rb_define_const(mProf, "CONST_MISSES", INT2NUM(MEASURE_CONST_MISSES));
    rb_define_const(mProf, "CONST_MISSES_ENABLED", MEASURE_CONST_MISSES_ENABLED);

    cMeasureConstMisses = rb_define_class_under(mMeasure, "ConstMisses", rb_cObject);
    rb_define_singleton_method(cMeasureConstMisses, "measure", prof_measure_const_misses, 0);
}
