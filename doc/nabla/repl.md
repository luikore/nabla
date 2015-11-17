Should ship with a repl built by web.

## copying repl history to code

To find the code to a certain variable

    :path_to res[3]

Pops a dialog for choosing lines, optional intermediates can be unchecked.

## inspecting different types

Any object with `inspect_html` will generate a tag and be put in iframe

    def inspect_html
      ...
