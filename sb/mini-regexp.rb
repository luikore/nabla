# bootstrap: parse subset of regexp, and generate AST building code

# usage:
#   node = MiniRegexp.new(src).parse
#   node.context = "Foo"
#   node.id = 3
#   node.eval #=> output

# shortcuts:
#   - no need to parse \u, \x, {n}, {m,n}?
#   - no need to interpolate aliases, so we can generate result AST actions directly
#   - no predef unicode class
#   - assume all special chars inside [] are escaped

require_relative "mini-common"

class MiniRegexp

  Reg = Struct.new :reg
  Klasses.add 'Regexp', ['reg']
  class Reg
    def eval
      "NODE(Regexp, 1, #{reg.eval})"
    end
  end

  Branches = Struct.new :branches
  Klasses.add 'Branches', ['branches']
  class Branches
    def eval
      if branches.empty?
        raise "no branches found!"
      end
      build_list branches.map &:eval
    end
  end

  Seq = Struct.new :seq
  Klasses.add 'Seq', ['seq']
  class Seq
    def eval
      content = build_list seq.map &:eval
      "NODE(Seq, 1, #{content})"
    end
  end

  PredefAnchor = Struct.new :anchor
  Klasses.add 'PredefAnchor', ['anchor']
  class PredefAnchor
    def eval
      "NODE(PredefAnchor, 1, #{anchor.eval})"
    end
  end

  Flag = Struct.new :flag
  Klasses.add 'Flag', ['flag']
  class Flag
    def eval
      "NODE(Flag, 1, #{flag.eval})"
    end
  end

  Quantified = Struct.new :unit, :quantifier
  Klasses.add 'Quantified', ['unit', 'quantifier']
  class Quantified
    def eval
      "NODE(Quantified, 2, #{unit.eval}, #{quantifier.eval})"
    end
  end

  # c is int
  Char = Struct.new :c
  Klasses.add 'Char', ['c']
  class Char
    def eval
      "VAL_FROM_INT(#{c})"
    end
  end

  Group = Struct.new :special, :branches
  Klasses.add 'Group', ['special', 'branches']
  class Group
    def eval
      "NODE(Group, 2, #{special.eval}, #{branches.eval})"
    end
  end

  CharGroupPredef = Struct.new :tok
  Klasses.add 'CharGroupPredef', ['tok']
  class CharGroupPredef
    def eval
      "NODE(CharGroupPredef, 1, #{tok.eval})"
    end
  end

  BracketCharGroup = Struct.new :beg_tok, :char_classes
  Klasses.add 'BracketCharGroup', ['beg_tok', 'char_classes']
  class BracketCharGroup
    def eval
      multi = build_list char_classes.map &:eval rescue pp char_classes
      "NODE(BracketCharGroup, 2, #{beg_tok.eval}, #{multi})"
    end
  end

  # from and to are int
  CharRange = Struct.new :from, :to
  Klasses.add 'CharRange', ['from', 'to']
  class CharRange
    def eval
      "NODE(CharRange, 2, VAL_FROM_INT(#{from}), VAL_FROM_INT(#{to}))"
    end
  end

  attr_reader :s
  def initialize src
    @s = StringScanner.new src
  end

  def parse
    res = parse_branches
    if !@s.eos?
      raise "expect eos: #{@s.inspect}"
    end
    Reg.new res
  end

  # Branches : Seq >* op.branch Seq
  def parse_branches
    branches = []

    pos = @s.pos
    res = parse_seq
    if res
      branches << res
    else
      @s.pos = pos
      return branches
    end

    while @s.scan(/\s*\|\s*/)
      branches << expect(:parse_seq)
    end
    Branches.new branches
  end

  # Seq : SeqUnit*
  def parse_seq
    seq = []
    while q = parse_seq_unit
      seq << q
    end
    Seq.new seq
  end

  # SeqUnit : anchor / Unit quantifier / Unit
  def parse_seq_unit
    if a = @s.scan(/\^|\$|\\[bBaAzZ]/)
      return PredefAnchor.new Token.new("anchor", a)
    end

    pos = @s.pos
    u = parse_unit
    if !u
      @s.pos = pos
      return
    end

    quantifier = @s.scan(/[\?\*\+]/)
    if quantifier
      return Quantified.new u, Token.new("quantifier", quantifier)
    else
      u
    end
  end

  # Unit : Group / CharGroup / SingleChar
  def parse_unit
    if @s.scan(/(?=\()/)
      parse_group
    elsif @s.scan(/(?=\[|\\[dDwWhHsS]|\.)/)
      parse_char_group
    elsif c = parse_single_char
      Char.new c
    end
  end

  # SingleChar : char.escape.sp / char.escape / char
  def parse_single_char
    if c = @s.scan(/\\[ftnr]/)
      (eval "\"#{c}\"").ord
    elsif c = @s.scan(/\\[^\n]/)
      c[1].ord
    elsif c = @s.scan(/[^\n\\\/\+\*\?\|\{\}\(\)\[\]\^\$]/)
      c.ord
    end
  end

  # some control chars in char group is just normal char
  def parse_char_group_char
    if c = @s.scan(/\\[ftnr]/)
      (eval "\"#{c}\"").ord
    elsif c = @s.scan(/\\[^\n]/)
      c[1].ord
    elsif c = @s.scan(/[^\n\\\/\[\]]/)
      c.ord
    end
  end

  # Group : begin.group group.special Branches end.group
  def parse_group
    @s.scan(/\(/)
    special = @s.scan(/\?:|\?=|\?!|\?<=|\?<!|\?>/)
    branches = parse_branches
    if !@s.scan(/\)/)
      raise "expect ')': #{@s.inspect}"
    end
    Group.new Token.new("group.special", special || ''), branches
  end

  # CharGroup : char-group.predef / begin.char-group CharClass+ end.char-group
  def parse_char_group
    if res = @s.scan(/\\[dDwWhHsS]|\./)
      CharGroupPredef.new Token.new("char-group.predef", res)
    elsif res = @s.scan(/\\p\{[A-Z][a-z]*\}/)
      raise "unicode char class not supported: #{@s.inspect}"
    else
      beg = @s.scan(/\[\^?/)
      if !beg
        return
      end
      if @s.scan(/(?=[^\]]+\z)/)
        raise "char group not closed: #{@s.inspect}"
      end

      classes = []
      c = parse_char_class
      if !c
        raise "expect char class: #{@s.inspect}"
      end
      classes << c
      while c = parse_char_class
        classes << c
      end

      ed = @s.scan(/\]/)
      if !ed
        raise "expect ']': #{@s.inspect}"
      end

      BracketCharGroup.new Token.new("begin.char-group", beg), classes
    end
  end

  # CharClass : CharGroup / SingleChar op.minus SingleChar / SingleChar
  def parse_char_class
    pos = @s.pos
    if char_group = parse_char_group
      return char_group
    end
    @s.pos = pos

    c = parse_char_group_char
    if !c
      return
    end
    if @s.scan(/-/)
      CharRange.new c, expect(:parse_char_group_char)
    else
      CharRange.new c, c
    end
  end

  def expect m
    r = send m
    if !r
      raise "expect #{m}: #{@s.inspect}"
    end
    r
  end
end

if __FILE__ == $PROGRAM_NAME
  require "pp"
  def t src, meth
    puts src
    parser = MiniRegexp.new(src)
    node = parser.send meth
    if !parser.s.eos?
      raise "not parsed: #{parser.s.inspect}"
    end
    pp node
    puts
  end

  t 'a', :parse_char_class
  t 'a-z', :parse_char_class
  t '\w', :parse_char_class
  t 'abc', :parse_seq
  t '', :parse_seq
  t '|.', :parse_branches
  t '()', :parse_group
  t '\\\\p\{[A-Z][a-z]*\}', :parse
  t '[[)]]', :parse_char_group

  puts
  node = MiniRegexp.new('\?\* | \?\? | \? | \+\* | \+\? | \+ | \*\* | \*\? | \*').parse
  # puts node.eval
end
