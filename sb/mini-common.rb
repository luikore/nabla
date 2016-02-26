require "strscan"

Klasses = {}
class << Klasses
  def add ty, args
    if self[ty]
      if self[ty] != args
        raise "conflict fields for #{ty}: #{self[ty].inspect} -- #{args.inspect}"
      end
    else
      self[ty] = args
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
