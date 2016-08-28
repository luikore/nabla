# shortcut: no need to process op-table and lookahead

require_relative "mini-callback"

class MiniPeg
  EOLS_RE = /(\ *(\#[^\n]*)?(\n|\z))+/
  CALLBACK_RE = /\{("(\\.|[^"])"|[^\}])*\}/
  RULE_RE = /[A-Z]\w*(\.\w+)*/
  TOKEN_RE = /[a-z]\w*(\-\w+)*(\.\w+(\-\w+)*)*/

  Peg = Struct.new :context, :rules
  class Peg
    def eval
      Klasses.validate self.class
      multi = build_list rules.map &:eval
      "NODE(Peg, 2, #{context.eval}, #{multi})"
    end
  end

  PegRule = Struct.new :name, :body
  class PegRule
    def eval
      Klasses.validate self.class
      "NODE(PegRule, 2, #{name.eval}, #{body.eval})"
    end
  end

  SeqRule = Struct.new :terms, :code
  class SeqRule
    def eval
      Klasses.validate self.class
      terms_multi = build_list terms.map &:eval
      maybe_code = build_list code.map &:eval
      "NODE(SeqRule, 2, #{terms_multi}, #{maybe_code})"
    end
  end

  Branch = Struct.new :op, :lhs, :rhs_terms, :code
  class Branch
    def eval
      Klasses.validate self.class
      rhs = build_list rhs_terms.map &:eval
      maybe_code = build_list code.map &:eval
      "NODE(Branch, 4, #{op.eval}, #{lhs.eval}, #{rhs}, #{maybe_code})"
    end
  end

  TermMaybe = Struct.new :unit
  class TermMaybe
    def eval
      Klasses.validate self.class
      "NODE(TermMaybe, 1, #{unit.eval})"
    end
  end

  TermStar = Struct.new :unit
  class TermStar
    def eval
      Klasses.validate self.class
      "NODE(TermStar, 1, #{unit.eval})"
    end
  end

  TermPlus = Struct.new :unit
  class TermPlus
    def eval
      Klasses.validate self.class
      "NODE(TermPlus, 1, #{unit.eval})"
    end
  end

  Term = Struct.new :unit
  class Term
    def eval
      Klasses.validate self.class
      "NODE(Term, 1, #{unit.eval})"
    end
  end

  Lookahead = Struct.new :unit
  class Lookahead
    def eval
      Klasses.validate self.class
      "NODE(Lookahead, 1, #{unit.eval})"
    end
  end

  NegLookahead = Struct.new :unit
  class NegLookahead
    def eval
      Klasses.validate self.class
      "NODE(NegLookahead, 1, #{unit.eval})"
    end
  end

  class EpsilonRule
    def eval
      Klasses.validate self.class
      "NODE(EpsilonRule, 0)"
    end
  end

  RefRule = Struct.new :name
  class RefRule
    def eval
      Klasses.validate self.class
      "NODE(RefRule, 1, #{name.eval})"
    end
  end

  def initialize ctx, src
    @ctx = Token.new "kw.peg", ctx
    @s = StringScanner.new src
  end

  # Peg : name.context begin.peg space.eol* PegRule* end.peg
  def parse
    parse_eols
    rules = []
    while rule = parse_rule
      rules << rule
    end
    if !@s.eos?
      raise "expected eos : #{@s.inspect}"
    end
    Peg.new @ctx, rules
  end

  # PegRule : name.rule op.def RuleBody space.eol
  def parse_rule
    @s.skip(/[\t\ ]+/)
    name = @s.scan(RULE_RE)
    return if !name
    op = @s.scan(/\ *:\ */)
    raise "expect op : #{@s.inspect}" if !op
    rule_body = parse_rule_body
    raise "expect rule body : #{@s.inspect}" if !rule_body
    eol = parse_eols
    raise "expect eol : #{@s.inspect}" if !eol
    PegRule.new Token.new("name.rule", name), rule_body
  end

  # RuleBody : SeqRule /* space.eol? op.branch Term* PureCallback? { Branch[$3, $1, $4, $5] }
  def parse_rule_body
    seq_rule = parse_seq_rule
    return if !seq_rule
    res = seq_rule
    loop do
      pos = @s.pos
      branch = parse_branch_right res
      if branch
        res = branch
      else
        @s.pos = pos
        break
      end
    end
    res
  end

  # SeqRule : Term+ Callback?
  def parse_seq_rule
    t = parse_term
    return if !t
    terms = [t]
    while t = parse_term
      terms << t
    end
    SeqRule.new terms, parse_callback
  end

  # space.eol? op.branch Term* PureCallback?
  def parse_branch_right lhs
    parse_eols
    op = @s.scan(/[\t\ ]*(\/[\*\+\?]?)[\t\ ]*/)
    return if !op
    op = Token.new('op.branch', op.strip)
    terms = []
    while t = parse_term
      terms << t
    end
    Branch[op, lhs, terms, parse_callback]
  end

  # Term : Unit op.maybe         { TermMaybe[$1] }
  #      / Unit op.star          { TermStar[$1] }
  #      / Unit op.plus          { TermPlus[$1] }
  #      / Unit                  { Term[$1] }
  #      / op.lookahead Unit     { Lookahead[$2] }
  #      / op.neg-lookahead Unit { NegLoookahead[$2] }
  def parse_term
    @s.skip(/ +/)
    if unit = parse_unit
      if @s.scan(/ *\*/)
        TermStar.new unit
      elsif @s.scan(/ *\?/)
        TermMaybe.new unit
      elsif @s.scan(/ *\+/)
        TermPlus.new unit
      else
        Term.new unit
      end
    end
  end

  # Unit : name.token / name.rule.epsilon { EpsilonRule[] } / name.rule { RefRule[$1] }
  def parse_unit
    if name = @s.scan(RULE_RE)
      if name == 'EPSILON'
        EpsilonRule.new
      else
        tok = Token.new "name.rule", name
        RefRule.new tok
      end
    elsif name = @s.scan(TOKEN_RE)
      Token.new "name.token", name
    end
  end

  # always parses
  def parse_callback
    callback = @s.scan CALLBACK_RE
    if callback
      code = callback[1...-1]
      if code.strip.empty?
        []
      else
        node = MiniCallback.new(code.strip).parse
        raise "failed to parse callback for #{code.inspect}" if !node
        [node]
      end
    else
      []
    end
  end

  def parse_eols
    @s.scan EOLS_RE
  end
end

if __FILE__ == $PROGRAM_NAME
  Klasses.add 'Peg', ['context', 'rules']
  Klasses.add 'PegRule', ['name', 'body']
  Klasses.add 'SeqRule', ['terms', 'code']
  Klasses.add 'Term', ['unit']
  Klasses.add 'TermPlus', ['unit']
  Klasses.add 'TermStar', ['unit']
  Klasses.add 'TermMaybe', ['unit']
  Klasses.add 'EpsilonRule', []
  Klasses.add 'RefRule', ['name']
  Klasses.add 'Lookahead', ['unit']
  Klasses.add 'NegLoookahead', ['unit']
  Klasses.add 'Callback', ['stmts']
  Klasses.add 'CreateNode', ['ty', 'elems']
  Klasses.add 'Capture', ['var_name']
  Klasses.add 'Call', ['func_name', 'args']
  Klasses.add 'Branch', ['op', 'lhs', 'rhs_terms', 'code']
  require "pp"
  pegcode = <<-PEGCODE
  Peg : name.context begin.peg space.eol* Rule* end.peg { Peg[$1, $4] }
  Rule : name.rule op.def RuleBody space.eol { PegRule[$1, $3] }
  RuleBody : SeqRule { $1 }
           /* space.eol? op.branch Term* PureCallback? { Branch[$3, $1, $4, $5] }
  SeqRule : Term+ Callback? { SeqRule[$1, $2] }
  Term : Unit op.maybe         { TermMaybe[$1] }
       / Unit op.star          { TermStar[$1] }
       / Unit op.plus          { TermPlus[$1] }
       / Unit                  { Term[$1] }
       / op.lookahead Unit     { Lookahead[$2] }
       / op.neg-lookahead Unit { NegLoookahead[$2] }
  Unit : name.token { $1 }
       / name.rule.epsilon { EpsilonRule[] }
       / name.rule  { RefRule[$1] }
  PEGCODE
  node = MiniPeg.new("Context", pegcode).parse
  pp node
  puts node.eval
end
