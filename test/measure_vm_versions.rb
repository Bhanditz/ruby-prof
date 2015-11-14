#!/usr/bin/env ruby
# encoding: UTF-8

require File.expand_path('../test_helper', __FILE__)

class MeasureVMVersionsTest < TestCase
  def test_vm_versions_mode
    RubyProf::measure_mode = RubyProf::VM_VERSIONS
    assert_equal(RubyProf::VM_VERSIONS, RubyProf::measure_mode)
  end

  def test_vm_versions_enabled_defined
    assert(defined?(RubyProf::VM_VERSIONS_ENABLED))
  end

  if RubyProf::VM_VERSIONS_ENABLED
    def test_vm_versions
      t = RubyProf.measure_vm_versions
      Module.new { def foo; end }
      u = RubyProf.measure_vm_versions
      assert u > t, [t, u].inspect
    end
  end
end
