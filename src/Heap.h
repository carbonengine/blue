// Copyright © 2013 CCP ehf.

#pragma once
#ifndef Heap_h
#define Heap_h

// A a heap, which is a partially sorted queue.  Wondrous stuff, this is.
// A heap is partially sorted.  It is an implicit fully populated binary tree
// and the children of each node are larger than the parent.  Given a node
// number i, the children have indices 2i and 2i+1, using base 1 numbering,
// or, using base 0 numbering: heap[k] is parent of heap[2*k+1] and heap[2*k+2],
// likewise, given a child i, its parent is (i-1)/2
// There are three main operations:
// -Inserting
// works by putting a node at the end, and swapping places with successive parents
// until it is no larger than both of its children.  This is a O(logN) operation.
// -popping, removes the smallest node (the root).  The tree is put in shape by
// taking the last node and putting in the place of the root node, then swapping
// places with the smaller of its children until it lands in its correct place.
// This is O(logN).
// Partially sorting a heap is (provably) a O(N) operation.  Done using Heapify()
// This is implemented as a subclass of the std::vector thing for simplicity.
template <class T, class Compare = std::less<T> >
class Heap : public std::vector<T>{
	typedef std::vector<T> parent;
	typedef Heap<T, Compare> klass;
public:

	Heap(const Compare &c = Compare()) : parent(), comp(c) {}
	Heap(const parent &list, const Compare &c = Compare()) : parent(list), comp(c) {}
	Heap(const klass &other) : parent(other), comp(other.comp) {}

	// add an entry.  Put it at the tail and percolate to the top.
	void Insert(const T &item){
		this->push_back(item);
		PercolateUp(this->size()-1);
	}

	// Return the topmost member and return
	T Shift(){
		CCP_ASSERT(this->size());  // must be something here!

		const T result(this->front());
		if (this->size()>1) {
			this->front() = this->back();
			this->pop_back();
			PercolateDown(0);
		} else
			this->pop_back();
		return result;
	}

	// Remove a particular member and return it.  The index is zero based.
    T Remove(typename parent::iterator item) {
		const T result(*item);
		if (item != this->end()-1) {
			*item = this->back();
			this->pop_back();
			Reorder(item);
		} else
			this->pop_back();  //it was the last item
		return result;
	}


	// an item has changed its priority, put it in its place
    void Reorder(typename parent::iterator item) {
		size_t index = item - this->begin();
		CCP_ASSERT(index < this->size());

		//first move up, then down again
		size_t oldplace = index;
		size_t newplace = PercolateUp(oldplace);
		if (newplace == oldplace)
			PercolateDown(newplace);
	}


	// Use this if you have just put random stuff into your heap, to make it ready
	void Heapify()
	{
		for(int i = int( this->size()/2-1 ); i>=0; i--)
			PercolateDown(i);
	}

private:
	size_t PercolateUp(size_t start)
	{
		klass &heap = *this;
		size_t target = start;
		const T item = heap[start];
		for(;target>0;) {
			size_t parent = (target-1)/2; //parent formula for 0-based heaps
			if (!comp(item, heap[parent])) //<=> heap[parent] <= child, use only operator <
				break;
			heap[target] = heap[parent];
			target = parent;
		}
		heap[target] = item;
		return target;
	}

	size_t PercolateDown(size_t start)
	{
		//it is faster to move up nodes until we hit a leaf, and then
		//percolate back up, then to break out of the loop if a parent
		//and child satisfy the heap property.  this is because the latter
		//requires more comparisons in total.  Particularly during pop, 
		//we are percolating down a guy that already belongs to the end.
		klass &heap = *this;
		size_t length = this->size();
		size_t target = start;
		const T item = heap[start];
		for(;;) {
			size_t minChild = target*2+1;
			if (minChild >= length)
				break;
			if (minChild+1 < length && comp(heap[minChild+1], heap[minChild]))
				minChild = minChild+1;
			heap[target] = heap[minChild];
			target = minChild;
		}
		heap[target] = item;
		return PercolateUp(target);
	}

	Compare comp; // the comparator
};


#endif // Heap_h