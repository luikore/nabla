require "strscan"

Klasses = {}
class << Klasses
  def add ty, args
    self[ty] = args
  end

  def validate struct
    name = struct.name[/\w+$/]
    members = struct.members.map &:to_s
    args = self[name]
    if args != members
      raise "mismatch struct #{name}:\n    sb.sb: #{args}\n  mini-sb: #{members}"
    end
  end
end

def build_list arr
  "LIST(#{arr.reverse.join ",\n"})"
end

Token = Struct.new :type, :s
class Token
  def eval
    # "TOKEN(#{type.inspect}, #{s.inspect}, VAL_NIL)"
    "STR(#{s.inspect})"
  end

  def to_s
    s
  end
end
