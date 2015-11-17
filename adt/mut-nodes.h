// TODO move the intermediate nodes out of array/map/dict impl and put in Val system
// 

#pragma mark array

// mutable nodes for immutable data structures

typedef struct {
  ValHeader h; // size hidden in flags
  Val data[];
} ArrayNode;

#pragma mark map and dict

typedef struct {
  Val k;
  Val v;
} KV;

// int -> slot
typedef struct {
  ValHeader header; // if is mapnode (bitmap controlled by parent), flags contains the slot size.
                    // if is colliarray, size is set in parent node

  // slots can be kv or mapnode* + bitmap (determined by VAL_KLASS) or colliarray* + size
  // this can also work as collision array
  KV kvs[];
} MapNode;

#pragma mark dict

typedef struct {
  ValHeader header;   // flags contains the bmap size
  uint64_t bitmap[4]; // 256
  Val data[];         // dictnode or dictbucket, determined by class
} DictNode;

// serialized leaf bucket
typedef struct {
  ValHeader header;
  char data[];
} DictBucket;

// NOTE hybrid HAT-trie is way too complex to impl
