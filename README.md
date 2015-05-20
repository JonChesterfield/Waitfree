Wait free and lock free data structures

At present, a single such data structure is implemented. This is a single producer, single consumer queue.

The central algorithm is derived from "Correct and Efficient Bounded FIFO Queues", N. Le, A. Guatto, A. Cohen and A. Pop which is derived from the work of L. Lamport in "Proving the correctness of multiprocess programs".

The algorithm for push or pop is essentially:

Get a pointer to my end of the queue
Get a pointer to the other end of the queue
Use these to determine if we can continue
Swap an element with my end of the queue
Increment my pointer

The API is not an especially friendly one, in particular whether the element passed to pop or push remainins in a valid state depends on the integer return code from the function calls. This implementation is intended as an implementation detail of a number of easier to use structures.
