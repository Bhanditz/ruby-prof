require 'set'

module RubyProf
  class Profile
    module BaseEliminations
      ENUMERABLE_SYMS = Enumerable.instance_methods(false)

      def self.methods
        @methods ||= []
      end

      def self.add_methods(mod, *args)
        name = mod.is_a?(Module) ? mod.name : mod.to_s
        args.flatten.each { |i| methods << "#{name}##{i}" }
      end

      def self.add_enumerable(mod, *args)
        add_methods(mod, :each, *args)
        add_methods(mod, ENUMERABLE_SYMS)
      end

      ##
      #  Kernel Methods
      ##

      add_methods Kernel, [
          :dup,
          :initialize_dup,
          :tap,
          :send,
          :public_send,
        ]

      ##
      #  Fundamental Types
      ##

      add_methods BasicObject,  :"!="
      add_methods Method,       :"[]"
      add_methods Module,       :new
      add_methods Class,        :new
      add_methods Proc,         :call, :yield
      add_methods Range,        :each

      ##
      #  Value Types
      ##

      add_methods String, [
          :sub,
          :sub!,
          :gsub,
          :gsub!,
        ]

      ##
      #  Emumerables
      ##

      add_enumerable Enumerable
      add_enumerable Enumerator

      ##
      #  Collections
      ##

      add_enumerable Array, [
          :each_index,
          :map!,
          :select!,
          :reject!,
          :collect!,
          :sort!,
          :sort_by!,
          :index,
          :delete_if,
          :keep_if,
          :drop_while,
          :uniq,
          :uniq!,
          :"==",
          :eql?,
          :hash,
          :to_json,
          :as_json,
          :encode_json,
        ]

      add_enumerable Hash, [
          :dup,
          :initialize_dup,
          :fetch,
          :"[]",
          :"[]=",
          :each_key,
          :each_value,
          :each_pair,
          :map!,
          :select!,
          :reject!,
          :collect!,
          :delete_if,
          :keep_if,
          :slice,
          :slice!,
          :except,
          :except!,
          :"==",
          :eql?,
          :hash,
          :to_json,
          :as_json,
          :encode_json,
        ]

      add_enumerable Set, [
        :map!,
        :select!,
        :reject!,
        :collect!,
        :classify,
        :delete_if,
        :keep_if,
        :divide,
        :"==",
        :eql?,
        :hash,
        :to_json,
        :as_json,
        :encode_json,
      ]

      ##
      #  Miscellaneous Methods
      ##

      add_methods '<Module::GC>', :start
      add_methods 'Mustache::Context', :find
      add_methods 'Unicorn::HttpServer', :process_client
      add_methods 'Unicorn::OobGC', :process_client

      add_methods 'NewRelic::Agent::Instrumentation::MiddlewareTracing', :call
      add_methods 'NewRelic::Agent::MethodTracerHelpers', :trace_execution_scoped
    end
  end
end
