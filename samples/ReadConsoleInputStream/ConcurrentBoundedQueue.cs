using System;
using System.Collections.Concurrent;
using System.Collections.Generic;

namespace Samples.Terminal
{
    /// <summary>
    /// Implements a bounded queue that won't block on overflow; instead the oldest item is discarded.
    /// </summary>
    /// <typeparam name="T"></typeparam>
    public class ConcurrentBoundedQueue<T> : ConcurrentQueue<T>
    {
        public ConcurrentBoundedQueue(int capacity)
        {
            Capacity = GetAlignedCapacity(capacity);
        }

        public ConcurrentBoundedQueue(IEnumerable<T> collection, int capacity) : base(collection)
        {
            Capacity = GetAlignedCapacity(capacity);
        }

        private int GetAlignedCapacity(int n)
        {
            if (n < 2)
            {
                throw new ArgumentException("Capacity must be at least 2");
            }

            var f = Math.Log(n, 2);
            var p = Math.Ceiling(f);

            return (int) Math.Pow(2, p);
        }

        public new void Enqueue(T item)
        {
            // if we're about to overflow, dump oldest item
            if (Count >= Capacity)
            {
                lock (this)
                {
                    while (Count >= Capacity)
                    {
                        TryDequeue(out _);
                    }
                }
            }

            base.Enqueue(item);
        }

        public int Capacity
        {
            get; private set;
        }
    }
}
