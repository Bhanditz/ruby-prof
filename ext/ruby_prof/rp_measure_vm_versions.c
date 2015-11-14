#include "ruby_prof.h"

static VALUE cMeasureVMVersions;

#if defined(HAVE_RB_VM_STATE_VERSION)
  VALUE rb_vm_state_versions(void);
#endif

static double
measure_vm_versions()
{
#if defined(HAVE_RB_VM_STATE_VERSION)
#define MEASURE_VM_VERSIONS_ENABLED Qtrue
  return NUM2ULL(rb_vm_state_version());
#else
#define MEASURE_VM_VERSIONS_ENABLED Qfalse
  return 0;
#endif
}

prof_measurer_t* prof_measurer_vm_versions()
{
  prof_measurer_t* measure = ALLOC(prof_measurer_t);
  measure->measure = measure_vm_versions;
  return measure;
}

/* call-seq:
   measure -> int

Returns the number of VM versions.*/
static VALUE
prof_measure_vm_versions(VALUE self)
{
#if defined(HAVE_LONG_LONG)
    return ULL2NUM(measure_vm_versions());
#else
    return ULONG2NUM(measure_vm_versions());
#endif
}

void rp_init_measure_vm_versions()
{
    rb_define_const(mProf, "VM_VERSIONS", INT2NUM(MEASURE_VM_VERSIONS));
    rb_define_const(mProf, "VM_VERSIONS_ENABLED", MEASURE_VM_VERSIONS_ENABLED);

    cMeasureVMVersions = rb_define_class_under(mMeasure, "VMVersions", rb_cObject);
    rb_define_singleton_method(cMeasureVMVersions, "measure", prof_measure_vm_versions, 0);
}
