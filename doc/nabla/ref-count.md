### cow friendly considerations

We need cow-friendlyness if don't provide threads.

Permanent the obj tree when assigning to consts, so the shared data: constants are cow-friendly

For bytecode to be cow-friendly, method cache should be allocated separately.

Bitmap counting is a bit more friendly for cow, but it is hard to make it work efficiently.

### DRC with method analysis

Some methods do not change the refcount of args, we can put this info in method metadata and do not change
