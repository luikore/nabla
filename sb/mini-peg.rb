# shortcut: no need to process op-table and lookahead

require_relative "mini-callback"

class MiniPeg
  EOLS_RE = /(\ *(\#[^\n]*)?(\n|\z))+/
  CALLBACK_RE = /\{("(\\.|[^"])"|[^\}])*\}/
  RULE_RE = /[A-Z]\w*(\.\w+)*/
  TOKEN_RE = /[a-z]\w*(\-\w+)*(\.\w+(\-\w+)*)*/

  Peg = Struct.new :context, :rules
  Klasses.add 'Peg', ['context', 'rules']
  class Peg
    def eval
      multi = build_list rules.map &:eval
      "NODE(Peg, 2, #{context.eval}, #{multi})"
    end
  end

  PegRule = Struct.new :name, :body
  Klasses.add 'PegRule', ['name', 'body']
  class PegRule
    def eval
      multi = build_list body.map &:eval
      "NODE(PegRule, 2, #{name.eval}, #{multi})"
    end
  end

  SeqRule = Struct.new :terms, :code
  Klasses.add 'SeqRule', ['terms', 'code']
  class SeqRule
    def eval
      terms_multi = build_list terms.map &:eval
      maybe_code = build_list code.map &:eval
      "NODE(SeqRule, 2, #{terms_multi}, #{maybe_code})"
    end
  end
  
  BranchRight = Struct.new :branch_op, :rhs
  Klasses.add 'BranchRight', ['branch_op', 'rhs']
  class BranchRight
    def eval
      "NODE(BranchRight, 2, #{branch_op.eval}, #{rhs.eval})"
    end
  end

  Term = Struct.new :name, :affix
  Klasses.add 'Term', ['name', 'affix']
  class Term
    def eval
      "NODE(Term, 2, #{name.eval}, #{affix ? affix.eval : 'VAL_NIL'})"
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

  # RuleBody : SeqRule >* space.eol? BranchRight
  def parse_rule_body
    seq_rule = parse_seq_rule
    return if !seq_rule
    res = [seq_rule]
    loop do
      pos = @s.pos
      parse_eols
      if branch_right = parse_branch_right
        res << branch_right
      else
        @s.pos = pos
        break
      end
    end
    if res.any?(&:nil?)
      raise "nil entry in #{res.inspect}"
    end
    res # parse_rule will use build_list
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

  # BranchRight : op.branch-like SeqRule
  def parse_branch_right
    parse_eols
    op = @s.scan(/[\t\ ]*(\/|\>\*)[\t\ ]*/)
    return if !op
    seq_rule = parse_seq_rule
    raise "expect seq rule : #{@s.inspect}" if !seq_rule
    op.strip!
    tok = (op == '/' ? "op.branch" : "op.branch.quantified")
    BranchRight.new Token.new(tok, op), seq_rule
  end

  # Term : name.token-or-rule quantifier?
  def parse_term
    @s.skip(/ +/)
    name = @s.scan(RULE_RE)
    name_tok = "name.rule" if name
    name ||= @s.scan(TOKEN_RE)
    name_tok ||= "name.token"
    return if !name
    name = Token.new(name_tok, name)

    quantifier = @s.scan(/[\*\?\+]/)
    if quantifier
      quantifier = Token.new("op.quantified", quantifier)
    end
    Term.new name, quantifier
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
  require "pp"
  pegcode = <<-PEGCODE
  Peg : name.context begin.peg space.eol* Rule* end.peg { Peg[$1, $4] }
  Rule : name.rule op.def RuleBody space.eol { :node "PegRule" $1 $3 }
  RuleBody : SeqRule { :arr_node $1 } >* space.eol? BranchRight { :push_arr_node $1 $3 }
  BranchRight : op.branch.quantified SeqRule
              / op.branch SeqRule
              / op.branch.op-table
  SeqRule : Term+ Callback? { :node "SeqRule" $1 $2 }
  Term : Name op.quantified    { :node "Term" $1 $2 }
       / Name op.extract       { :node "Term" $1 $2 }
       / Name op.extract.maybe { :node "Term" $1 $2 }
       / Name                  { :node "Term" $1 nil }
       / op.lookahead LookaheadName { :node "Lookahead" $2 }
  Name : name.token { $1 }
       / name.rule  { $1 }
  LookaheadName : name.token   { $1 }
                / name.rule    { $1 }
                / name.pattern { $1 }
  PEGCODE
  node = MiniPeg.new("Context", pegcode).parse
  pp node
  puts node.eval
end
