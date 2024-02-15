#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <algorithm>
#include <queue>

#include <omp.h>

// ------------------------------------------------------------------------------

//#define USE_STD_SET

#ifdef USE_STD_SET

#include <set>
template<typename T> using set_t = std::set<T>;

#else

#include "cpp-btree/btree/set.h"
template<typename T> using set_t = btree::set<T>;

#endif

// ------------------------------------------------------------------------------

namespace btreesort {
	struct Settings {
		size_t nProcessors;
		size_t nSubBuckets;
		size_t nMinPerSlice;
		size_t nParallelCutoff;
		
		Settings();
		
		static const Settings& get();
	};
	
	template<typename Iter, typename Comparator>
	class BTreeSort {
	public:
		using IterVal = typename std::iterator_traits<Iter>::value_type;
		using IterPair = std::array<Iter, 2>;
		
		// TODO: Clean this mess up
		
		class Slice {
		public:
			size_t id;
			IterPair range;
			size_t count;
			IterVal median;
			
			Slice() = default;
			Slice(size_t id, IterPair range) : id(id), range(range)
			{
				count = std::distance(range[0], range[1]);
				median = get(size() / 2);
			}
			
			size_t size() const { return count; }
			IterVal get(size_t i) const { return *(range[0] + i); }
			
			//bool operator<(const Slice& o) const { return median < o.median; }
			//bool operator==(const Slice& o) const { return median == o.median; }
			bool operator<(const Slice& o) const { 
				if (median != o.median)
					return median < o.median;
				return id < o.id;
			}
			bool operator==(const Slice& o) const { 
				if (median != o.median)
					return false;
				return id == o.id;
			}
		};
		
		using SliceRefRange = std::array<typename std::vector<Slice>::const_iterator, 2>;
		
		class SliceValue {
		public:
			const Slice* pSlice;
			size_t iRead;
			
			SliceValue() = default;
			SliceValue(const Slice* pSlice) : pSlice(pSlice), iRead(0) {}
			
			IterVal get() const { return pSlice->get(iRead); }
			
			bool operator<(const SliceValue& o) const { return get() < o.get(); }
			bool operator>(const SliceValue& o) const { return get() > o.get(); }
			bool operator==(const SliceValue& o) const { return get() == o.get(); }
		};
	private:
		IterPair data;
		Comparator comp;
		
		set_t<Slice> setSlices;
		
		omp_lock_t om_writeLock;
	public:
		BTreeSort(Iter begin, Iter end, Comparator comp);
		~BTreeSort();
		
		void Sort();
	private:
		std::vector<std::array<size_t, 3>> _GenerateDivisions(size_t count, size_t divs);
		
		void _SortBucket(size_t id, IterPair range);
		void _ShuffleSlices(Iter dest, const std::vector<Slice>& slices);
		void _MultiwayHeap(Iter dest, SliceRefRange slices);
		void _InsertionSortRange(Iter begin, Iter end);
	};

	// ------------------------------------------------------------------------------

#define TEMPL template<typename Iter, typename Comparator>
#define DEF_BTreeSort BTreeSort<Iter, Comparator>::

	TEMPL inline DEF_BTreeSort 
	BTreeSort(Iter begin, Iter end, Comparator comp) :
		data({ begin, end }), comp(comp) 
	{
		omp_set_dynamic(false);
		omp_set_num_threads(Settings::get().nProcessors);
		
		omp_init_lock(&om_writeLock);
	}
	TEMPL inline DEF_BTreeSort ~BTreeSort()
	{
		omp_destroy_lock(&om_writeLock);
	}

	TEMPL void DEF_BTreeSort Sort()
	{
		size_t nProcessors = Settings::get().nProcessors;
		size_t nSlices = Settings::get().nSubBuckets;
		
		auto& [itrBegin, itrEnd] = data;
		size_t dataCount = std::distance(itrBegin, itrEnd);
		
		// If too few data, just use normal sorting
		if (dataCount < Settings::get().nParallelCutoff) {
			std::sort(itrBegin, itrEnd, comp);
		}
		else {
			auto buckets = _GenerateDivisions(dataCount, nProcessors);

#pragma omp parallel for
			for (auto& [i, begin, end] : buckets) {
				_SortBucket(i, { itrBegin + begin, itrBegin + end });
			}
			
			{
				std::vector<IterVal> dataNew;
				dataNew.reserve(dataCount);
				
				// Load slices from min to max into the new data
				
				std::vector<Slice> slicesSorted;
				slicesSorted.reserve(setSlices.size());
				
				{
					size_t i = 0;
					for (const Slice& s : setSlices) {
						auto before = dataNew.end();
						
						dataNew.insert(dataNew.end(), s.range[0], s.range[1]);
						
						Slice ns(s.id, { before, dataNew.end() });
						slicesSorted.push_back(std::move(ns));
						
						++i;
					}
				}
				
				_ShuffleSlices(itrBegin, slicesSorted);
				_InsertionSortRange(itrBegin, itrEnd);
				
				/* for (size_t i = 0; i < dataCount; ++i) {
					*(itrBegin + i) = dataNew[i];
				} */
			}
		}
	}

	// Divides [count] elements into [divs] divisions roughly equally
	TEMPL std::vector<std::array<size_t, 3>> DEF_BTreeSort 
	_GenerateDivisions(size_t count, size_t divs)
	{
		std::vector<std::array<size_t, 3>> res { divs };
		
		for (size_t i = 0; i < divs; ++i) {
			size_t begin = count * i / divs;
			size_t end = count * (i + 1) / divs;
			res[i] = { i, begin, end };
		}
		
		return res;
	}
	
	TEMPL void DEF_BTreeSort _SortBucket(size_t id, IterPair bucket)
	{
		auto& [itrBegin, itrEnd] = bucket;
		
		// Maybe replace with quicksort rather than STD introsort?
		std::sort(itrBegin, itrEnd, comp);
		
		size_t nSlices = Settings::get().nSubBuckets;
		auto partitions = _GenerateDivisions(std::distance(itrBegin, itrEnd), nSlices);
		
		// Partition slices
		{
			OmpLock lock(om_writeLock); // Lock setSlices for writing
			
			for (auto& [i, begin, end] : partitions) {
				Slice s(id * nSlices + i, { itrBegin + begin, itrBegin + end });
				setSlices.insert(std::move(s));
			}
		}
	}
	TEMPL void DEF_BTreeSort _ShuffleSlices(Iter dest, const std::vector<Slice>& slices)
	{
		size_t nProcessors = Settings::get().nProcessors;
		size_t nSlices = Settings::get().nSubBuckets;
		
		{
			struct _ShufParam {
				size_t index;
				SliceRefRange slices;
				size_t placement;
				size_t count;
			};
			std::vector<_ShufParam> shufParams(nSlices);
			
			auto sliceDivs = _GenerateDivisions(slices.size(), nSlices);
			
			{
				size_t placement = 0;
				for (auto& [i, iBegin, iEnd] : sliceDivs) {
					auto itrSliceBeg = slices.begin() + iBegin;
					auto itrSliceEnd = slices.begin() + iEnd;
					
					// Sum the amount of all data in this slice range
					size_t count = 0;
					for (auto itr = itrSliceBeg; itr != itrSliceEnd; ++itr) {
						count += (*itr).size();
					}
					
					shufParams[i] = _ShufParam {
						i,
						{ itrSliceBeg, itrSliceEnd },
						placement,
						count,
					};
					placement += count;
				}
			}

#pragma omp parallel for
			for (auto& sp : shufParams) {
				_MultiwayHeap(dest + sp.placement, sp.slices);
			}
		}
	}
	TEMPL void DEF_BTreeSort _MultiwayHeap(Iter dest, SliceRefRange slices)
	{
		//multiset_t<SliceValue> heap;
		std::priority_queue<SliceValue, std::vector<SliceValue>, 
			std::greater<SliceValue>> heap;
		
		for (auto itr = slices[0]; itr != slices[1]; ++itr) {
			const Slice* s = &*itr;
			if (s->size() > 0) {
				heap.push(SliceValue(s));
			}
		}
		
		while (!heap.empty()) {
			SliceValue front = std::move(heap.top());
			
			// Pop min element from heap into dest
			*(dest++) = front.get();
			heap.pop();
			
			{
				// Advance read index and move the next element into the heap
				
				front.iRead += 1;
				if (front.iRead < front.pSlice->size()) {
					heap.push(std::move(front));
				}
			}
		}
	}
	
	TEMPL void DEF_BTreeSort _InsertionSortRange(Iter begin, Iter end)
	{
		// Copied from std::__insertion_sort
		
		if (begin == end) return;
		
		for (Iter i = begin + 1; i != end; ++i) {
			if (comp(*i, *begin)) {
				IterVal val = std::move(*i);
				
				std::move_backward(begin, i, i + 1);
				*begin = std::move(val);
			}
			else {
				IterVal val = std::move(*i);
				
				Iter next = i;
				--next;
				
				while (comp(val, *next)) {
					*i = std::move(*next);
					i = next;
					--next;
				}
				
				*i = std::move(val);
			}
		}
	}

#undef TEMPL

	// ------------------------------------------------------------------------------

	Settings::Settings()
	{
		nProcessors = omp_get_num_procs();
		nSubBuckets = nProcessors;

		/* nProcessors = 8;
		nSubBuckets = 4; */

		nMinPerSlice = 4;
		nParallelCutoff = nProcessors * nSubBuckets * nMinPerSlice;
	}
	const Settings& Settings::get()
	{
		static Settings s {};
		return s;
	}
}