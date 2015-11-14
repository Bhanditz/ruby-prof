#!/usr/bin/env ruby
# encoding: UTF-8

require File.expand_path('../test_helper', __FILE__)

class MeasureConstMissesTest < TestCase
  def test_const_misses_mode
    RubyProf::measure_mode = RubyProf::CONST_MISSES
    assert_equal(RubyProf::CONST_MISSES, RubyProf::measure_mode)
  end

  def test_const_misses_enabled_defined
    assert(defined?(RubyProf::CONST_MISSES_ENABLED))
  end

  if RubyProf::CONST_MISSES_ENABLED
    def test_const_misses
      t = RubyProf.measure_const_misses
      ConstMisser.access_missing_constant!
      u = RubyProf.measure_const_misses
      assert u > t, [t, u].inspect
    end
  end
end
