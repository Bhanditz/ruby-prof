/* Copyright (C) 2005-2013 Shugo Maeda <shugo@ruby-lang.org> and Charlie Savage <cfis@savagexi.com>
   Please see the LICENSE file for copyright and distribution information */

#include "ruby_prof.h"

#define RP_FL_RESOLVED          (1 << 0)
#define RP_FL_MODULE_INCLUDEE   (1 << 1)
#define RP_FL_MODULE_SINGLETON  (1 << 2)
#define RP_FL_OBJECT_SINGLETON  (1 << 3)

#define RP_RESOLVED(fl)         ((fl) & RP_FL_RESOLVED)
#define RP_MODULE_INCLUDEE(fl)  ((fl) & RP_FL_MODULE_INCLUDEE)
#define RP_MODULE_SINGLETON(fl) ((fl) & RP_FL_MODULE_SINGLETON)
#define RP_OBJECT_SINGLETON(fl) ((fl) & RP_FL_OBJECT_SINGLETON)

VALUE cMethodInfo;

/* ================  Helper Functions  =================*/
static VALUE
figure_singleton_name(VALUE klass)
{
    VALUE result = Qnil;

    /* We have come across a singleton object. First
       figure out what it is attached to.*/
    VALUE attached = rb_iv_get(klass, "__attached__");

    /* Is this a singleton class acting as a metaclass? */
    if (BUILTIN_TYPE(attached) == T_CLASS)
    {
        result = rb_str_new2("<Class::");
        rb_str_append(result, rb_class_name(attached));
        rb_str_cat2(result, ">");
    }

    /* Is this for singleton methods on a module? */
    else if (BUILTIN_TYPE(attached) == T_MODULE)
    {
        result = rb_str_new2("<Module::");
        rb_str_append(result, rb_class_name(attached));
        rb_str_cat2(result, ">");
    }

    /* Is this for singleton methods on an object? */
    else if (BUILTIN_TYPE(attached) == T_OBJECT)
    {
        /* Make sure to get the super class so that we don't
           mistakenly grab a T_ICLASS which would lead to
           unknown method errors. */
        VALUE super = rb_class_superclass(klass);
        result = rb_str_new2("<Object::");
        rb_str_append(result, rb_class_name(super));
        rb_str_cat2(result, ">");
    }

    /* Ok, this could be other things like an array made put onto
       a singleton object (yeah, it happens, see the singleton
       objects test case). */
    else
    {
        result = rb_any_to_s(klass);
    }

    return result;
}

static VALUE
klass_name(VALUE klass)
{
    VALUE result = Qnil;

    if (klass == 0 || klass == Qnil)
    {
        result = rb_str_new2("[global]");
    }
    else if (BUILTIN_TYPE(klass) == T_MODULE || BUILTIN_TYPE(klass) == T_CLASS)
    {
        result = rb_class_name(klass);
    }
    else if (BUILTIN_TYPE(klass) == T_CLASS && FL_TEST(klass, FL_SINGLETON))
    {
        result = figure_singleton_name(klass);
    }
    else
    {
        /* Should never happen. */
        result = rb_str_new2("[unknown]");
    }

    return result;
}

static VALUE
method_name(ID mid)
{
#ifdef ID_ALLOCATOR
    if (mid == ID_ALLOCATOR)
        return rb_str_new2("allocate");
#endif

    if (RTEST(mid))
        return rb_str_dup(rb_id2str(mid));
    else
        return rb_str_new2("[no method]");
}

static VALUE
full_name(VALUE klass, ID mid)
{
    VALUE result = rb_str_dup(klass_name(klass));
    rb_str_cat2(result, "#");
    rb_str_append(result, method_name(mid));
    return result;
}

static VALUE
resolved_klass_name(VALUE resolved_klass)
{
    return RTEST(resolved_klass) ?
          rb_str_dup(rb_class_name(resolved_klass)) :
          rb_str_new2("[global]");
}

static VALUE
calltree_name(VALUE resolved_klass, int flags, ID mid)
{
    VALUE klass_str = resolved_klass_name(resolved_klass);
    VALUE klass_path = rb_str_split(klass_str, "::");
    VALUE result = rb_ary_join(klass_path, rb_str_new2("/"));

    rb_str_cat2(result, "::");

    if (RP_OBJECT_SINGLETON(flags))
        rb_str_cat2(result, "*");

    if (RP_MODULE_SINGLETON(flags))
        rb_str_cat2(result, "^");

    rb_str_append(result, method_name(mid));

    return result;
}

void
method_key(prof_method_key_t* key, VALUE klass, ID mid)
{
    /* Is this an include for a module?  If so get the actual
        module class since we want to combine all profiling
        results for that module. */
    if (klass != 0)
        klass = (BUILTIN_TYPE(klass) == T_ICLASS ? RBASIC(klass)->klass : klass);

    key->klass = klass;
    key->mid = mid;
    key->key = (klass << 4) + (mid << 2);
}

/* ================  prof_method_t   =================*/
prof_method_t*
prof_method_create(VALUE klass, ID mid, const char* source_file, int line)
{
    prof_method_t *result = ALLOC(prof_method_t);
    result->object = Qnil;
    result->call_infos = prof_call_infos_create();

    result->key = ALLOC(prof_method_key_t);
    method_key(result->key, klass, mid);

    //result->call_info_table = call_info_table_create();

    if (source_file != NULL)
    {
      size_t len = strlen(source_file) + 1;
      char *buffer = ALLOC_N(char, len);

      MEMCPY(buffer, source_file, char, len);
      result->source_file = buffer;
    }
    else
    {
      result->source_file = source_file;
    }
    result->line = line;

    result->resolved_klass = 0;
    result->flags = 0;

    return result;
}

/* The underlying c structures are freed when the parent profile is freed.  
   However, on shutdown the Ruby GC frees objects in any will-nilly order.
   That means the ruby thread object wrapping the c thread struct may
   be freed before the parent profile.  Thus we add in a free function
   for the garbage collector so that if it does get called will nil 
   out our Ruby object reference.*/
static void
prof_method_ruby_gc_free(prof_method_t* method)
{
	/* Has this thread object been accessed by Ruby?  If
	   yes clean it up so to avoid a segmentation fault. */
	if (method->object != Qnil)
	{
		RDATA(method->object)->data = NULL;
		RDATA(method->object)->dfree = NULL;
		RDATA(method->object)->dmark = NULL;
    }
	method->object = Qnil;
}

static void
prof_method_free(prof_method_t* method)
{
	prof_method_ruby_gc_free(method);
	prof_call_infos_free(method->call_infos);
	xfree(method->call_infos);

	xfree(method->key);
	method->key = NULL;

	xfree(method);
}

void
prof_method_mark(prof_method_t *method)
{
    if (method->resolved_klass)
        rb_gc_mark(method->resolved_klass);

	if (method->object)
		rb_gc_mark(method->object);

	prof_call_infos_mark(method->call_infos);
}

static VALUE
resolve_klass(prof_method_t* method)
{
    VALUE klass, attached;
    int flags;

    /* We want to group methods according to their source-level
        definitions, not their implementation class. Follow module
        inclusions and singleton classes back to a meaningful root
        while keeping track of these relationships. */

    /* Is this method's klass aleady resolved? */
    if (RP_RESOLVED(method->flags))
        return method->resolved_klass;

    klass = method->key->klass;
    flags = 0;

    while (1)
    {
        /* This is a global/unknown class */
        if (klass == 0 || klass == Qnil)
          break;

        /* Is this a singleton class? (most common case) */
        if (BUILTIN_TYPE(klass) == T_CLASS && FL_TEST(klass, FL_SINGLETON))
        {
          /* We have come across a singleton object. First
            figure out what it is attached to.*/
          attached = rb_iv_get(klass, "__attached__");

          /* Is this a singleton class acting as a metaclass?
              Or for singleton methods on a module? */
          if (BUILTIN_TYPE(attached) == T_CLASS ||
              BUILTIN_TYPE(attached) == T_MODULE)
          {
            flags |= RP_FL_MODULE_SINGLETON;
            klass = attached;
          }
          /* Is this for singleton methods on an object? */
          else if (BUILTIN_TYPE(attached) == T_OBJECT)
          {
            flags |= RP_FL_OBJECT_SINGLETON;
            klass = rb_class_superclass(klass);
          }
          /* This is a singleton of an instance of a builtin type. */
          else
          {
            flags |= RP_FL_OBJECT_SINGLETON;
            klass = rb_class_superclass(klass);
          }
        }
        /* Is this an include for a module?  If so get the actual
            module class since we want to combine all profiling
            results for that module. */
        else if (BUILTIN_TYPE(klass) == T_ICLASS)
        {
          flags |= RP_FL_MODULE_INCLUDEE;
          klass = RBASIC(klass)->klass;
        }
        /* No transformations apply; so bail. */
        else
        {
          break;
        }
    }

    /* This class is fully resolved. */
    flags |= RP_FL_RESOLVED;

    method->resolved_klass = klass;
    method->flags = flags;

    return klass;
}

VALUE
prof_method_wrap(prof_method_t *result)
{
  if (result->object == Qnil)
  {
    result->object = Data_Wrap_Struct(cMethodInfo, prof_method_mark, prof_method_ruby_gc_free, result);
  }
  return result->object;
}

static prof_method_t *
get_prof_method(VALUE self)
{
    /* Can't use Data_Get_Struct because that triggers the event hook
       ending up in endless recursion. */
	prof_method_t* result = DATA_PTR(self);

	if (!result)
	    rb_raise(rb_eRuntimeError, "This RubyProf::MethodInfo instance has already been freed, likely because its profile has been freed.");

   return result;
}

/* ================  Method Table   =================*/
int
method_table_cmp(prof_method_key_t *key1, prof_method_key_t *key2)
{
    return (key1->klass != key2->klass) || (key1->mid != key2->mid);
}

st_index_t
method_table_hash(prof_method_key_t *key)
{
    return key->key;
}

struct st_hash_type type_method_hash = {
    method_table_cmp,
    method_table_hash
};

st_table *
method_table_create()
{
  return st_init_table(&type_method_hash);
}

static int
method_table_free_iterator(st_data_t key, st_data_t value, st_data_t dummy)
{
    prof_method_free((prof_method_t*)value);
    return ST_CONTINUE;
}

void
method_table_free(st_table *table)
{
    st_foreach(table, method_table_free_iterator, 0);
    st_free_table(table);
}


size_t
method_table_insert(st_table *table, const prof_method_key_t *key, prof_method_t *val)
{
  return st_insert(table, (st_data_t) key, (st_data_t) val);
}

prof_method_t *
method_table_lookup(st_table *table, const prof_method_key_t* key)
{
    st_data_t val;
    if (st_lookup(table, (st_data_t)key, &val))
    {
      return (prof_method_t *) val;
    }
    else
    {
      return NULL;
    }
}

/* ================  Method Info   =================*/
/* Document-class: RubyProf::MethodInfo
The RubyProf::MethodInfo class stores profiling data for a method.
One instance of the RubyProf::MethodInfo class is created per method
called per thread.  Thus, if a method is called in two different
thread then there will be two RubyProf::MethodInfo objects
created.  RubyProf::MethodInfo objects can be accessed via
the RubyProf::Result object.
*/

/* call-seq:
   line_no -> int

   returns the line number of the method */
static VALUE
prof_method_line(VALUE self)
{
    return rb_int_new(get_prof_method(self)->line);
}

/* call-seq:
   source_file => string

return the source file of the method
*/
static VALUE prof_method_source_file(VALUE self)
{
    const char* sf = get_prof_method(self)->source_file;
    if(!sf)
    {
      return rb_str_new2("ruby_runtime");
    }
    else
    {
      return rb_str_new2(sf);
    }
}


/* call-seq:
   method_class -> klass

Returns the Ruby klass that owns this method. */
static VALUE
prof_method_klass(VALUE self)
{
    prof_method_t *result = get_prof_method(self);
    return result->key->klass;
}

/* call-seq:
   method_id -> ID

Returns the id of this method. */
static VALUE
prof_method_id(VALUE self)
{
    prof_method_t *result = get_prof_method(self);
    return ID2SYM(result->key->mid);
}

/* call-seq:
   klass_name -> string

Returns the name of this method's class.  Singleton classes
will have the form <Object::Object>. */

static VALUE
prof_klass_name(VALUE self)
{
    prof_method_t *method = get_prof_method(self);
    return klass_name(method->key->klass);
}

/* call-seq:
   method_name -> string

Returns the name of this method in the format Object#method.  Singletons
methods will be returned in the format <Object::Object>#method.*/

static VALUE
prof_method_name(VALUE self)
{
    prof_method_t *method = get_prof_method(self);
    return method_name(method->key->mid);
}

/* call-seq:
   full_name -> string

Returns the full name of this method in the format Object#method.*/

static VALUE
prof_full_name(VALUE self)
{
    prof_method_t *method = get_prof_method(self);
    return full_name(method->key->klass, method->key->mid);
}

/* call-seq:
   call_infos -> Array of call_info

Returns an array of call info objects that contain profiling information
about the current method.*/
static VALUE
prof_method_call_infos(VALUE self)
{
    prof_method_t *method = get_prof_method(self);
	if (method->call_infos->object == Qnil)
	{
		method->call_infos->object = prof_call_infos_wrap(method->call_infos);
	}
	return method->call_infos->object;
}

/* call-seq:
   resolved_klass -> klass

Returns the Ruby klass that owns this method. */
static VALUE
prof_resolved_klass(VALUE self)
{
    prof_method_t *method = get_prof_method(self);
    return resolve_klass(method);
}

/* call-seq:
   calltree_name -> string

Returns the full name of this method in the calltree format.*/

static VALUE
prof_calltree_name(VALUE self)
{
    prof_method_t *method = get_prof_method(self);
    VALUE resolved_klass = resolve_klass(method);
    return calltree_name(resolved_klass, method->flags, method->key->mid);
}

void rp_init_method_info()
{
    /* MethodInfo */
    cMethodInfo = rb_define_class_under(mProf, "MethodInfo", rb_cObject);
    rb_undef_method(CLASS_OF(cMethodInfo), "new");

    rb_define_method(cMethodInfo, "klass", prof_method_klass, 0);
    rb_define_method(cMethodInfo, "klass_name", prof_klass_name, 0);
    rb_define_method(cMethodInfo, "method_name", prof_method_name, 0);
    rb_define_method(cMethodInfo, "full_name", prof_full_name, 0);
    rb_define_method(cMethodInfo, "method_id", prof_method_id, 0);
    rb_define_method(cMethodInfo, "source_file", prof_method_source_file,0);
    rb_define_method(cMethodInfo, "line", prof_method_line, 0);
    rb_define_method(cMethodInfo, "call_infos", prof_method_call_infos, 0);

    rb_define_method(cMethodInfo, "resolved_klass", prof_resolved_klass, 0);
    rb_define_method(cMethodInfo, "calltree_name", prof_calltree_name, 0);
}
