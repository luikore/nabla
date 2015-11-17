# NOTE: no need to deal with pattern ref / var ref

require_relative "mini-callback"
require_relative "mini-regexp"

class MiniLex
  EOLS_RE = /(\ *(\#[^\n]*)?(\n|\z))+/
  CALLBACK_RE = /\{("(\\.|[^"])"|[^\}])*\}/

  Lex = Struct.new :context, :rules
  class Lex
    def eval
      multi = MiniCallback.build_multi rules.map &:eval
      "NODE(Lex, Lex, 2, #{context.eval}, #{multi})"
    end
  end

  # see also `lex String` and `peg String`
  Str = Struct.new :str
  class Str
    def eval
      "NODE(String, String, 1, nb_string_new_literal_c(#{str.inspect}))"
    end
  end

  RefPartialContext = Struct.new :name
  class RefPartialContext
    def eval
      "NODE(Lex, RefPartialContext, 1, #{name.eval})"
    end
  end

  RefContext = Struct.new :name
  class RefContext
    def eval
      "NODE(Lex, RefContext, 1, #{name.eval})"
    end
  end

  SeqLexRules = Struct.new :rules
  class SeqLexRules
    def eval
      multi_rules = MiniCallback.build_multi rules.map &:eval
      "NODE(Lex, SeqLexRules, 1, #{multi_rules})"
    end
  end

  BeginCallback = Struct.new :first_cb, :rules
  class BeginCallback
    def eval
      cb = first_cb ? first_cb.eval : "VAL_NIL"
      multi_rules = MiniCallback.build_multi rules.map &:eval
      "NODE(Lex, BeginCallback, 2, #{cb}, #{multi_rules})"
    end
  end

  EndCallback = Struct.new :first_cb, :rules
  class EndCallback
    def eval
      cb = first_cb ? first_cb.eval : "VAL_NIL"
      multi_rules = MiniCallback.build_multi rules.map &:eval
      "NODE(Lex, EndCallback, 2, #{cb}, #{multi_rules})"
    end
  end

  Rule = Struct.new :pattern, :code
  class Rule
    def eval
      multi_rules = MiniCallback.build_multi code.map &:eval
      "NODE(Lex, Rule, 2, #{pattern.eval}, #{multi_rules})"
    end
  end

  Token = Struct.new :type, :s
  class Token
    def eval
      "TOKEN(#{type.inspect}, #{s.inspect})"
    end
  end

  def initialize ctx, src
    @ctx = Token.new "kw.lex", ctx
    @s = StringScanner.new src
  end

  # Lex : name.context begin.lex RuleLine* end.lex
  def parse
    parse_eols
    lines = []
    loop do
      pos = @s.pos
      if line = parse_rule_line
        if line != :eol
          lines << line
        end
      else
        @s.pos = pos
        break
      end
    end
    if !@s.eos?
      raise "not reached eos: #{@s.inspect}"
    end

    Lex.new @ctx, lines
  end

  # RuleLine : name.context.partial / Rule+ / Callback / kw.begin Callback? Rule* / kw.end Callback? Rule* / space.eol
  def parse_rule_line
    @s.skip(/[\ \t]+/)
    partial_context_name = @s.scan(/\*[A-Z]\w*/)
    if partial_context_name
      return RefPartialContext.new Token.new('name.context.partial', partial_context_name)
    end

    if rule = parse_rule
      return SeqLexRules[[rule, *parse_rules]]
    end

    pos = @s.pos
    if cb = parse_callback
      return PureCallbackRule.new cb
    end

    @s.pos = pos
    if beg = @s.scan(/(begin|end)\b\ */)
      cb = parse_callback
      rules = parse_rules
      if beg.start_with?('begin')
        return BeginCallback.new cb, rules
      else
        return EndCallback.new cb, rules
      end
    end

    @s.pos = pos
    if @s.scan(/\n/)
      :eol
    end
  end

  # Rule*
  def parse_rules
    rules = []
    loop do
      pos = @s.pos
      if rule = parse_rule
        rules << rule
      else
        @s.pos = pos
        break
      end
    end
    rules
  end

  # Rule : Pattern space.pre-callback* Callback? / name.context
  def parse_rule
    pattern = parse_pattern
    if pattern
      @s.skip(/[\ \t]*/)
      callback = parse_callback
      Rule.new pattern, (callback ? [callback] : [])
    elsif c = @s.scan(/[A-Z]\w*/)
      RefContext.new Token.new("name.context", c)
    end
  end

  # Pattern : String / Regexp
  def parse_pattern
    @s.skip(/[\ \t]*/)
    parse_string or parse_reg
  end

  def parse_string
    if r = @s.scan(/"(\\.|[^"])*"/)
      Str.new r[1...-1]
    end
  end

  def parse_reg
    reg = @s.scan /\/(\\.|[^\/])*\//
    if reg
      code = reg[1...-1]
      MiniRegexp.new(code).parse
    end
  end

  def parse_callback
    callback = @s.scan CALLBACK_RE
    if callback
      code = callback[1...-1]
      MiniCallback.new(code).parse
    end
  end

  def parse_eols
    @s.scan EOLS_RE
  end
end

if __FILE__ == $PROGRAM_NAME
  require "pp"

  def t src, meth
    puts src
    node = MiniLex.new("Context", src).send meth
    pp node
    puts
  end

  t '"]"', :parse_string
  t '{ :return, :token "end.lex"}', :parse_callback
  t '/\*[A-Z]\w*/', :parse_reg
  t 'begin{}', :parse_rule_line
  t 'begin "a" {} /b/ {}', :parse_rule_line

  src = <<-LEXCODE
  "]" { :return, :token "end.lex" }

  Regexp
  String
  Callback
  /\\*[A-Z]\\w*/ { :token "name.context.partial" }

  *Spaces
  LEXCODE
  node = MiniLex.new("Context", src).parse
  pp node
  puts node.eval
end
