using System;
using System.Collections.Concurrent;
using System.Collections.Generic;

namespace Nivot.Terminal
{
    /// <summary>
    /// Implements a circular buffer.
    /// </summary>
    /// <typeparam name="T"></typeparam>
    public class ConcurrentCircularQueue<T> : ConcurrentQueue<T>
    {
        public ConcurrentCircularQueue(int capacity)
        {
            Capacity = GetAlignedCapacity(capacity);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="collection"></param>
        /// <param name="capacity"></param>
        public ConcurrentCircularQueue(IEnumerable<T> collection, int capacity) : base(collection)
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
