# encoding: utf-8

module RubyProf
  # Generate profiling information in calltree format
  # for use by kcachegrind and similar tools.

  class CallTreePrinter  < AbstractPrinter
    # Specify print options.
    #
    # options - Hash table
    #   :min_percent - Number 0 to 100 that specifes the minimum
    #                  %self (the methods self time divided by the
    #                  overall total time) that a method must take
    #                  for it to be printed out in the report.
    #                  Default value is 0.
    #
    #   :print_file  - True or false. Specifies if a method's source
    #                  file should be printed.  Default value if false.
    #
    def print(output = STDOUT, options = {})
      @output = output
      setup_options(options)

      # add a header - this information is somewhat arbitrary
      @output << "events: "
      case RubyProf.measure_mode
        when RubyProf::PROCESS_TIME
          @value_scale = RubyProf::CLOCKS_PER_SEC;
          @output << 'process_time'
        when RubyProf::WALL_TIME
          @value_scale = 1_000_000
          @output << 'wall_time'
        when RubyProf.const_defined?(:CPU_TIME) && RubyProf::CPU_TIME
          @value_scale = RubyProf.cpu_frequency
          @output << 'cpu_time'
        when RubyProf.const_defined?(:ALLOCATIONS) && RubyProf::ALLOCATIONS
          @value_scale = 1
          @output << 'allocations'
        when RubyProf.const_defined?(:MEMORY) && RubyProf::MEMORY
          @value_scale = 1
          @output << 'memory'
        when RubyProf.const_defined?(:GC_RUNS) && RubyProf::GC_RUNS
          @value_scale = 1
          @output << 'gc_runs'
        when RubyProf.const_defined?(:GC_TIME) && RubyProf::GC_TIME
          @value_scale = 1000000
          @output << 'gc_time'
        when RubyProf.const_defined?(:VM_VERSIONS) && RubyProf::VM_VERSIONS
          @value_scale = 1
          @output << 'vm_versions'
        when RubyProf.const_defined?(:CONST_MISSES) && RubyProf::CONST_MISSES
          @value_scale = 1
          @output << 'const_misses'
        else
          raise "Unknown measure mode: #{RubyProf.measure_mode}"
      end
      @output << "\n\n"

      print_threads
    end

    def print_threads
      printable_threads.each do |thread|
        print_thread(thread)
      end
    end

    def printable_threads
      if @options[:only_threads]
        only_thread_ids = @options[:only_threads].map(&:object_id)
        @result.threads.select do |t|
          only_thread_ids.include?(t.id)
        end
      else
        @result.threads
      end
    end

    def convert(value)
      (value * @value_scale).round
    end

    def file(method)
      File.expand_path(method.source_file)
    end

    def print_thread(thread)
      thread.methods.reverse_each do |method|
        print_method(method)
        @output << "\n"
      end
    end

    def print_method(method)
      if method.recursive?
        print_recursive_method(method)
      else
        print_simple_method(method)
      end
    end

    def print_recursive_method(method)
      method.call_infos.each_with_index do |caller, index|
        name = "#{method.calltree_name} [#{index}]"
        print_header(method, name, caller.self_time)
        print_children(method, name, caller.children)
      end
    end

    def print_simple_method(method)
      print_header(method, method.calltree_name, method.self_time)
      print_children(method, method.calltree_name, method.children)
    end

    def print_header(method, name, self_time)
      # Print out the file and method name
      @output << "fl=#{file(method)}\n"
      @output << "fn=#{name}\n"

      # Now print out the function line number and its self time
      @output << "#{method.line} #{convert(self_time)}\n"
    end

    def print_children(method, name, children)
      children.each do |callee|
        target = callee.target

        if target.recursive?
          index = target.call_infos.index(callee)
          name = "#{target.calltree_name} [#{index}]"
        else
          name = target.calltree_name
        end

        @output << "cfl=#{file(target)}\n"
        @output << "cfn=#{name}\n"
        @output << "calls=#{callee.called} #{callee.line}\n"

        # Print out total times here!
        @output << "#{callee.line} #{convert(callee.total_time)}\n"
      end
    end
  end # end class
end # end packages
