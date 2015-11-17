vm specialization trick:

    ADD:
        if (type == long) {
    OPT_LONG_ADD:
            ...
        } else if (type == string) {
    OPT_STRING_ADD:
            ...
        }

and during runtime, we may replace bytecode with the specialized ones

### how we do dynamic specialization?

dynamic specialization

### about `super`

`super` always calls last impl, the methods are chained in a list by definition

    class X
      def v
        ...
      end

      def v
        ... super
      end
    end
