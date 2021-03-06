# bootstrap: parse spellbreak callback, and generate AST

# usage:
#   node = MiniCallback.new(src).parse
#   node.eval #=> output

# NOTE
# - literals are translated into immediate values `VAL_NIL` (no need boxing, since strings are literals)
# - no need to parse if

require_relative "mini-common"

class MiniCallback

  Callback = Struct.new :stmts
  class Callback
    def eval
      Klasses.validate self.class
      multi = build_list stmts.map &:eval
      "NODE(Callback, 1, #{multi})"
    end
  end

  VarDecl = Struct.new :var_name
  class VarDecl
    def eval
      Klasses.validate self.class
      "NODE(VarDecl, 1, #{var_name.eval})"
    end

    def to_s
      var_name.to_s
    end
  end

  # in bootstrap, no assign expression needed yet
  Assign = Struct.new :var_name, :expr
  class Assign
    def eval
      Klasses.validate self.class
      "NODE(Assign, 2, #{var_name.eval}, #{expr.eval})"
    end
  end

  GlobalAssign = Struct.new :var_name, :expr
  class GlobalAssign
    def eval
      Klasses.validate self.class
      "NODE(GlobalAssign, 2, #{var_name.eval}, #{expr.eval})"
    end
  end

  Call = Struct.new :func_name, :argv
  class Call
    def self.funcs
      @funcs ||= []
    end

    def eval
      Klasses.validate self.class
      Call.funcs << [func_name.to_s, argv.size]

      multi = build_list argv.map &:eval
      "NODE(Call, 2, #{func_name.eval}, #{multi})"
    end
  end

  SplatEntry = Struct.new :expr
  class SplatEntry
    def eval
      Klasses.validate self.class
      "NODE(SplatEntry, 1, #{expr.eval})"
    end
  end

  Lit = Struct.new :lit
  class Lit
    def eval
      lit
    end

    def to_s
      lit
    end
  end

  CreateNode = Struct.new :ty, :elems
  class CreateNode
    def eval
      Klasses.validate self.class
      multi = build_list elems.map &:eval
      "NODE(CreateNode, 2, #{ty.eval}, #{multi})"
    end
  end

  CreateList = Struct.new :elems
  class CreateList
    def eval
      Klasses.validate self.class
      multi = build_list elems.map &:eval
      "NODE(CreateList, 1, #{multi})"
    end
  end

  Capture = Struct.new :var_name
  class Capture
    def eval
      Klasses.validate self.class
      "NODE(Capture, 1, #{var_name.eval})"
    end
  end

  VarRef = Struct.new :var_name
  class VarRef
    def eval
      Klasses.validate self.class
      "NODE(VarRef, 1, #{var_name.eval})"
    end
  end

  GlobalVarRef = Struct.new :var_name
  class GlobalVarRef
    def eval
      Klasses.validate self.class
      "NODE(GlobalVarRef, 1, #{var_name.eval})"
    end
  end

  def initialize src
    @s = StringScanner.new src.strip
  end

  # Callback : begin.code Stmt* end.code
  def parse
    r = parse_stmts
    if !@s.eos?
      raise "not reach the end: #{@s.inspect}"
    end
    Callback.new r
  end

  # stmt* and skip :eol
  def parse_stmts
    res = []
    loop do
      pos = @s.pos
      @s.skip(/ */)
      s = parse_stmt
      if s
        res << s if s != :eol
        if @s.pos == pos
          raise "pos not advanced: #{@s.inspect}"
        end
      else
        break
      end
    end
    res
  end

  # Stmt : Expr / space.eol / kw.var name.var
  def parse_stmt
    @s.skip(/ */)
    if @s.scan(/\n|,/)
      :eol
    elsif @s.scan(/var\b/)
      @s.skip(/ +/)
      var_name = @s.scan(/[a-z]\w*/)
      if !var_name
        raise "expect var_name: #{@s.inspect}"
      end
      VarDecl.new Token.new("name.var", var_name)
    else
      parse_expr
    end
  end

  # Expr : Paren / Lit / Assign / Capture / VarRef / GlobalVarRef / CreateNode / CreateList / Call
  def parse_expr
    if @s.scan(/(?=\w+\ *=)/)
      parse_assign
    else
      parse_paren or parse_lit or parse_capture or parse_var_ref or parse_global_var_ref or parse_create_node or parse_create_list or parse_call
    end
  end

  # Paren: '(' expr ')'
  def parse_paren
    if @s.scan(/\(/)
      res = expect :parse_expr
      if !@s.scan(/\)/)
        raise "missing right paren: #{@s.inspect}"
      end
      res
    end
  end

  # Lit: int / true / false / nil
  def parse_lit
    if i = @s.scan(/-?\d+/)
      Lit.new "VAL_FROM_INT(#{i})"
    elsif @s.scan(/true\b/)
      Lit.new "VAL_TRUE"
    elsif @s.scan(/false\b/)
      Lit.new "VAL_FALSE"
    elsif @s.scan(/nil\b/)
      Lit.new "VAL_NIL"
    elsif s = @s.scan(/\"[^"]*\"/)
      Lit.new "STR(#{s})"
    end
  end

  # NOTE: definitive path
  # Assign: name.var '=' Expr
  def parse_assign
    if var_name = @s.scan(/[a-z]\w*/)
      @s.skip(/ *= */)
      expr = expect :parse_expr
      Assign.new Token.new("name.var", var_name), expr
    end
  end

  # Assign: name.var.global '=' Expr
  def parse_global_assign
    if var_name = @s.scan(/\$[a-z]\w*/)
      @s.skip(/ *= */)
      expr = expect :parse_expr
      GlobalAssign.new Token.new("name.var", var_name), expr
    end
  end

  # Capture: name.var.capture
  def parse_capture
    if c = @s.scan(/\$-?\d+/)
      Capture.new Token.new("name.var.capture", c)
    end
  end

  # VarRef: name.var
  def parse_var_ref
    if c = @s.scan(/[a-z]\w*/)
      VarRef.new Token.new("name.var", c)
    end
  end

  # GlobalVarRef: name.var.global
  def parse_global_var_ref
    if c = @s.scan(/\$[a-z]\w*/)
      GlobalVarRef.new Token.new("name.var.global", c)
    end
  end

  # CreateNode: name.type begin.list Entries end.list
  def parse_create_node
    if ty = @s.scan(/[A-Z]\w*/)
      raise "expect '[': #{@s.inspect}" if !@s.scan(/\[\ */)
      args = expect :parse_entries
      raise "expect ']': #{@s.inspect}" if !@s.scan(/\ *\]/)
      CreateNode.new Token.new('name.type', ty), args
    end
  end

  # CreateList: begin.list Entries end.list
  def parse_create_list
    if @s.scan(/\[\ */)
      args = expect :parse_entries
      raise "expect ']': #{@s.inspect}" if !@s.scan(/\ *\]/)
      CreateList.new args
    end
  end

  # Entries: Expr / Entries / op.prefix.splat Expr Entries / space.eol Entries / EPSILON
  def parse_entries
    entries = []
    loop do
      @s.skip(/ +/)
      pos = @s.pos
      if e = parse_expr
        entries << e
        if @s.pos == pos
          raise "pos not advanced: #{@s.inspect}"
        end
      elsif @s.scan(/\*/)
        @s.skip(/ +/)
        e = expect :parse_expr
        entries << (SplatEntry.new e)
      elsif @s.scan(/\n|,/)
      else
        @s.pos = pos
        break
      end
    end
    entries
  end

  # Lines: Expr Lines / space.eol Lines / EPSILON
  def parse_lines
    lines = []
    loop do
      @s.skip(/ +/)
      pos = @s.pos
      if e = parse_expr
        lines << e
        if @s.pos == pos
          raise "pos not advanced: #{@s.inspect}"
        end
      elsif @s.scan(/\n|,/)
      else
        @s.pos = pos
        break
      end
    end
    lines
  end

  # Call : name.func Expr*
  def parse_call
    if fname = @s.scan(/:[\w+\-*\/^&|<>=!%@]+/)
      exprs = []
      loop do
        @s.skip(/ +/)
        pos = @s.pos
        if e = parse_expr
          exprs << e
          if @s.pos == pos
            raise "pos not advanced: #{@s.inspect}"
          end
        elsif @s.scan(/(?=\)|\]|\n|,|\z)/)
          break
        else
          raise "failed to recognize args: #{@s.inspect}"
        end
      end
      Call.new Token.new("name.func", fname), exprs
    end
  end

  def expect meth
    res = send meth
    raise "syntax error, expect #{meth} at #{@s.inspect}" unless res
    res
  end
end

if __FILE__ == $PROGRAM_NAME
  require "pp"
  def t src, parse_method
    node = MiniCallback.new(src).send parse_method
    # puts src
    # pp node
    # puts
    if !(yield node)
      raise "parse error:\n  #{src}\n  --> #{node}"
    end
  end

  t ' $1 ', :parse_stmt do |r|
    r.var_name.to_s == '$1'
  end
  t '(23)', :parse_paren do |r|
    r.lit.to_s == 'VAL_FROM_INT(23)'
  end
  t ':token true $1', :parse_call do |r|
    r.func_name.to_s == ':token' and r.argv.size == 2
  end
  t ":style (:concat 1 z) $1", :parse_stmts do |r|
    r.size == 1
  end
  t 'x = 4', :parse_assign do |r|
    r.var_name.to_s == 'x' and r.expr.to_s == 'VAL_FROM_INT(4)'
  end
  t '$x = 2 + 1', :parse_global_assign do |r|
    r.var_name.to_s == '$x'
  end
  t 'var x', :parse_stmt do |r|
    r.is_a?(MiniCallback::VarDecl) and r.to_s == "x"
  end
  t "a\n b", :parse_stmts do |r|
    r.is_a?(Array) and r.size == 2
  end
  t "a ,b", :parse_stmts do |r|
    r.is_a?(Array) and r.size == 2
  end
  t ':return, :token "end.lex"', :parse_stmts do |r|
    r.is_a?(Array) and r.size == 2
  end
  t '*a, *b', :parse_entries do |r|
    r.is_a?(Array) and r.size == 2
  end

  Klasses.add 'Callback', ['stmts']
  Klasses.add 'Call', ['func_name', 'argv']
  Klasses.add 'VarRef', ['var_name']
  Klasses.add 'Capture', ['var_name']
  node = MiniCallback.new(<<-NABLA).parse
    :style (:concat 1 z) $1
    :String
    :return, :token true $1
  NABLA
  puts node.eval
end
