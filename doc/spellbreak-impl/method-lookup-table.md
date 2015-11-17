We put classes data in an array, klass is the index in the array.

method ids are integers

Very fast lookup with Knuth's hash

    klasses[klass].table[method_id * 2654435761 % 2**32]
    // if not, lookup klasses[klasses[klass].super]

    // calculate better magic numbers for different symtable size?

with prime table size, change to `% table_size` should provide very good result


for inline method cache, get class version:

    klasses[klass].version
