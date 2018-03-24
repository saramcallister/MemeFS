/* AUTHOR: M Jake Palanker */

#ifndef BLOCKLAYER_H   /* include guard */
#define BLOCKLAYER_H


/* here's the blocksize, please give me buffers of at least this large */
#define BLOCKSIZE 4096

/* call this exactly once to initialize require values and datastructures */
int block_dev_init();

/* give me a blockNumber and a buffer, and I'll fill it!
   make sure the buffer is at least BLOCKSIZE large */
int read_block(int blockNum, char *buf);

/*  give me a blockNumber and a buffer and I'll write it to the block!
    I promise not to change the buffer
    just make sure that it's at least BLOCKSIZE large */
int write_block(int blockNum, const char *buf);

/* ask for a new block and I'll return the blockNum of a block you can use
   I promise it's a totally new blockNum that isn't used for any other files */
int allocate_block();

/* when you're done with a block tell me and I'll get rid of that stuff
   once you free a block you'll never see that data again
   additionally, a previously freed block may be re-allocated later */
int free_block(int blockNum);

#endif /* include guard */
